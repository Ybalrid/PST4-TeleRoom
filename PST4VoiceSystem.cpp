#include "stdafx.h"
#include "PST4VoiceSystem.hpp"
#include "PST4Packets.hpp"

using namespace PST4;
using namespace Annwvyn;

VoiceSystem::VoiceSystem()
{
	if (!sanityCheck()) throw std::runtime_error("There's a problem with data size");
	//Detect input device:
	ALchar* inputDeviceIdentifier = nullptr;
	if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT") == AL_TRUE && AnnGetVRRenderer()->usesCustomAudioDevice())
	{
		//Open the audio device specified by the VR System
		detectInputDevice(alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER));
		for (auto& deviceName : detectedDevice)
		{
			if (deviceName.find(AnnGetVRRenderer()->getAudioDeviceIdentifierSubString()) != std::string::npos)
			{
				auto len = deviceName.length() + 1;
				inputDeviceIdentifier = new char[len];
				secure_strcpy(inputDeviceIdentifier, len, deviceName.c_str());
			}
		}
	}

	//Open Input device
	inputDevice = alcCaptureOpenDevice(inputDeviceIdentifier, SAMPLE_RATE, AL_FORMAT_MONO16, CAPTURE_BUFFER_SIZE);
	if (inputDeviceIdentifier) delete[] inputDeviceIdentifier;

	if (!inputDevice)
	{
		switch (alGetError())
		{
		case ALC_INVALID_VALUE: std::cout << "invalid value given at capture device creation\n"; break;
		case ALC_OUT_OF_MEMORY: std::cout << "specified device is invalid or cannot capture audio"; break;
		default:break;
		}
	}
	alcCaptureStart(inputDevice);
	//AnnDebug() << alcGetError(inputDevice);
	//AnnDebug() << alGetError();
	//AnnDebug() << "Error we don't want : " << ALC_INVALID_DEVICE;

	//debugDevice = alcOpenDevice(nullptr);
	alGenSources(1, &playbackSource);
	alSourcef(playbackSource, AL_GAIN, 1);

	alGenBuffers(ALsizei(playbackQueue.size()), playbackQueue.data());
	indexProcessed = 0;
	indexLastQueued = 0;

	//Compression
	speex_bits_init(&encBits);
	enc_state = speex_encoder_init(&speex_nb_mode);
	int output;
	speex_encoder_ctl(enc_state, SPEEX_GET_FRAME_SIZE, &output);

	if (output != BUFFER_SIZE / FRAMES_PER_BUFFER)
		throw std::runtime_error("Inconsistent buffer/frame/frame per buffer configuration");
	if (sizeof(sample_t) != sizeof(short))
		throw std::runtime_error("sample format incompatible");

	speex_bits_reset(&encBits);
	auto maxNbBytes = speex_bits_nbytes(&encBits);
	//AnnDebug() << maxNbBytes << "number of bytes for 160 samples. (uncompressed : was 320)";

	speex_bits_init(&decBits);
	dec_state = speex_decoder_init(&speex_nb_mode);
}

VoiceSystem::~VoiceSystem()
{
	speex_bits_destroy(&encBits);
	speex_bits_destroy(&decBits);
	speex_encoder_destroy(enc_state);
	speex_decoder_destroy(dec_state);

	alcCaptureStop(inputDevice);
	alcCaptureCloseDevice(inputDevice);
}

void VoiceSystem::detectInputDevice(const char* deviceList)
{
	if (!deviceList || *deviceList == '\0')
		std::cout << "enumerated device list empty";
	else do
	{
		std::string deviceName(deviceList);
		std::cout << "Audio device : " << deviceName << '\n';
		detectedDevice.push_back(deviceName);

		deviceList += strlen(deviceList) + 1;
	} while (*deviceList != '\0');
}

void VoiceSystem::capture()
{
	alcGetIntegerv(inputDevice, ALC_CAPTURE_SAMPLES, sizeof availableInputSamples, &availableInputSamples);
	////AnnDebug() << "debug : availableInputSamples = " << availableInputSamples;
	while (availableInputSamples > BUFFER_SIZE)
	{
		buffer640 tmpBuffer;
		alcCaptureSamples(inputDevice, tmpBuffer.data(), BUFFER_SIZE);
		availableInputSamples -= BUFFER_SIZE;

		if (queue.size() > DROP_THRESHOLD)
		{
			//AnnDebug() << "Accumulated more than a second of audio input. Dropping buffers now";
			queue.clear();
		}
		queue.emplace_front(tmpBuffer);
	}
}

bool VoiceSystem::bufferAvailable() const
{
	return queue.size() > 0;
}

VoiceSystem::buffer640 VoiceSystem::getNextBufferToSend()
{
	buffer640 buffer = queue.front();
	queue.pop_back();
	return buffer;
}

void VoiceSystem::debugPlayback(buffer640* buffer)
{
	//make the source follow the head of the user
	auto position = AnnGetVRRenderer()->trackedHeadPose.position;
	alSource3f(playbackSource, AL_POSITION, position.x, position.y, position.z);

	//We emulate some kind of "ring buffer" here. We will feed the buffer to the queue sequentially, so we only need to know
	//how many the source has allready gone through to know where we can "unqeue" them
	ALint processed;
	alGetSourcei(playbackSource, AL_BUFFERS_PROCESSED, &processed);
	//AnnDebug() << "processed " << processed;
	if (processed > 0)
	{
		//AnnDebug() << "should have played " << indexProcessed + processed;
		//if we don't go out of the array :
		if (indexProcessed + processed < playbackQueue.size())
		{
			alSourceUnqueueBuffers(playbackSource, processed, playbackQueue.data() + indexProcessed);
			indexProcessed += processed;
		}
		else
		{
			auto diff = (indexProcessed + processed) - playbackQueue.size();
			alSourceUnqueueBuffers(playbackSource, processed - ALsizei(diff), playbackQueue.data() + indexProcessed);
			alSourceUnqueueBuffers(playbackSource, ALsizei(diff), playbackQueue.data());
			indexProcessed = ALuint(diff);
		}
	}

	//If we've been called with a buffer, we will load the data in OpenAL and queue it
	if (buffer)
	{
		auto newIndex = (indexLastQueued + 1) % playbackQueue.size();
		//AnnDebug() << "new data for buffer at index " << newIndex;
		alBufferData(playbackQueue[newIndex], AL_FORMAT_MONO16, buffer->data(), BUFFER_SIZE * sizeof(sample_t), SAMPLE_RATE);
		alSourceQueueBuffers(playbackSource, 1, &playbackQueue[newIndex]);
		indexLastQueued = ALuint(newIndex);

		AnnDebug() << "index last queued = " << indexLastQueued;
		AnnDebug() << "index Processed = " << indexProcessed;
		AnnDebug() << "Q - P = " << int(indexLastQueued) - int(indexProcessed);
	}

	ALint state;
	alGetSourcei(playbackSource, AL_SOURCE_STATE, &state);
	if (state != AL_PLAYING) alSourcePlay(playbackSource);
}

std::vector<VoiceSystem::byte_t> VoiceSystem::encode(buffer640* buffer, byte_t frame)
{
	if (frame > FRAMES_PER_BUFFER) throw ("frame number invalid : only " + std::to_string(FRAMES_PER_BUFFER) + "frames per buffer");
	//AnnDebug() << "Encoding frame " << frame << " out of " << FRAMES_PER_BUFFER;
	//initialize the encode
	speex_bits_reset(&encBits);
	speex_encode_int(enc_state, buffer->data() + (frame * 160), &encBits);

	//Allocate buffer
	auto maxNubmberBytes = speex_bits_nbytes(&encBits);
	//AnnDebug() << "Maximum number of bytes (over " << 160 * 2 << " uncompressed) : " << maxNubmberBytes;
	std::vector<byte_t> output(maxNubmberBytes);

	//Write buffer content, and trim exess of size.
	auto bytesWritten = speex_bits_write(&encBits, output.data(), maxNubmberBytes);
	if (bytesWritten < maxNubmberBytes) output.resize(bytesWritten);

	//Uses C++11 enforced RVO
	return output;
}

VoiceSystem::buffer640 VoiceSystem::decode(unsigned char* frameSizes, unsigned char* data)
{
	buffer640 buffer; 				//Allocate a new buffer
	byte_t dataPosition{ 0 };		//position offset of the current frame in the compressed data

	for (auto i{ 0 }; i < FRAMES_PER_BUFFER; ++i)
	{
		//AnnDebug() << "decoding frame " << i << " at " << dataPosition << "Bytes into the data";
		//data + dataPosition will point to the 1st byte of the current frame
		speex_bits_read_from(&decBits, reinterpret_cast<byte_t*>(data) + dataPosition, frameSizes[i]);

		//data has to be written on the buffer array. Buffer has FRAME_PER_BUFFER worth of data in it, each frames are FRAME_SIZE samples
		speex_decode_int(dec_state, &decBits, buffer.data() + (i * FRAME_SIZE));

		//AnnDebug() << "wrote data to buffer at position " << i*FRAME_SIZE;
		dataPosition += frameSizes[i];
	}

	//Uses C++11 enforced RVO
	return buffer;
}