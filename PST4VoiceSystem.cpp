#include "stdafx.h"
#include "PST4VoiceSystem.hpp"
#include "PST4Packets.hpp"

using namespace PST4;
using namespace Annwvyn;

VoiceSystem::VoiceSystem()
{
	//This should even be optimized away by the compiler becaue of constexpr,
	//Of if I did something really stupid, it will crash the program before
	//doing unexplicable esoteric bugs sent directly bht the lord of darkness.
	if (!sanityCheck()) throw std::runtime_error("There's a problem with data size");

	//Detect what input device to use
	ALchar* inputDeviceIdentifier = nullptr;

	//If we can enumerate devices, and the rendersystem "hint" about what device to use (ex : Oculus and "Rift Audio")
	if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT") == AL_TRUE && AnnGetVRRenderer()->usesCustomAudioDevice())
	{
		//Get the list of devices.
		//This is in on a "[string]\0[string]\0[string]\0\0" format that is a bit annoying in C++, so detectInputDevice
		//will fill an array of std::string with them instead.
		detectInputDevice(alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER));
		for (auto& deviceName : detectedDevice)
		{
			//Found the hinted device
			if (deviceName.find(AnnGetVRRenderer()->getAudioDeviceIdentifierSubString()) != std::string::npos)
			{
				//Store the name
				auto len = deviceName.length() + 1;
				inputDeviceIdentifier = new char[len];
				secure_strcpy(inputDeviceIdentifier, len, deviceName.c_str());
				break;
			}
		}
	}

	//Open Input device. If inputDeviceIdentifier is still NULL at this point, it will open the OS default device
	inputDevice = alcCaptureOpenDevice(inputDeviceIdentifier, SAMPLE_RATE, AL_FORMAT_MONO16, CAPTURE_BUFFER_SIZE);

	//if something got real wrong here
	if (!inputDevice)
	{
		switch (alGetError())
		{
		case ALC_INVALID_VALUE: AnnDebug() << "invalid value given at capture device creation"; break;
		case ALC_OUT_OF_MEMORY: AnnDebug() << "specified device is invalid or cannot capture audio"; break;
		default:break;
		}
		if (inputDeviceIdentifier)
			throw std::runtime_error("Cannot open device : " + std::string(inputDeviceIdentifier));
		else
			throw std::runtime_error("Cannot open audio input device, and inputDeviceIdentifier is nullptr");
	}
	alcCaptureStart(inputDevice);
	if (alcGetError(inputDevice) == ALC_INVALID_DEVICE)
	{
		AnnDebug() << "Selected device: " << inputDeviceIdentifier << " is not a recording device";
	}
	if (inputDeviceIdentifier) delete[] inputDeviceIdentifier;

	alGenSources(1, &playbackSource);
	alSourcef(playbackSource, AL_GAIN, 1);

	//encode
	speex_bits_init(&encBits);
	enc_state = speex_encoder_init(&speex_nb_mode);
	int frameLength, samplingRate;
	speex_encoder_ctl(enc_state, SPEEX_GET_FRAME_SIZE, &frameLength);
	speex_encoder_ctl(enc_state, SPEEX_GET_SAMPLING_RATE, &samplingRate);

	if (frameLength != BUFFER_SIZE / FRAMES_PER_BUFFER)
		throw std::runtime_error("Inconsistent buffer/frame/frame per buffer configuration");
	if (sizeof(sample_t) != sizeof(short))
		throw std::runtime_error("sample format incompatible");

	AnnDebug() << "Sampling Rate : " << samplingRate;
	if (samplingRate != SAMPLE_RATE) throw std::runtime_error("Speex sample rate doesn't match the voice system sample rate");

	speex_bits_reset(&encBits);

	//decode
	speex_bits_init(&decBits);
	dec_state = speex_decoder_init(&speex_nb_mode);
	int vad = 1, state;
	speex_encoder_ctl(enc_state, SPEEX_SET_VAD, &vad);
	speex_encoder_ctl(enc_state, SPEEX_GET_VAD, &state);
	AnnDebug() << "speex encoder vad : " << state;

	preprocess_state = speex_preprocess_state_init(frameLength, samplingRate);

	int off = 1, on = 2;
	//Activate preprocessing function : De-noising, Automatic Gain Control, Voice Activity Detection, Dereverb
	speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_DENOISE, &on);
	speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_AGC, &on);
	speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_VAD, &on);
	speex_preprocess_ctl(preprocess_state, SPEEX_PREPROCESS_SET_DEREVERB, &on);
}

VoiceSystem::~VoiceSystem()
{
	//clean speex
	speex_bits_destroy(&encBits);
	speex_bits_destroy(&decBits);
	speex_encoder_destroy(enc_state);
	speex_decoder_destroy(dec_state);
	speex_preprocess_state_destroy(preprocess_state);

	//clean AL
	alDeleteSources(1, &playbackSource);
	for (auto buffer : availableBufferList)
		alDeleteBuffers(1, &buffer);
	for (auto buffer : playbackQueue)
		alDeleteBuffers(1, &buffer);
	alcCaptureStop(inputDevice);
	alcCaptureCloseDevice(inputDevice);
}

void VoiceSystem::detectInputDevice(const char* deviceList)
{
	if (!deviceList || *deviceList == '\0')
		AnnDebug() << "enumerated device list empty";
	else do
	{
		std::string deviceName(deviceList);
		AnnDebug() << "Audio device : " << deviceName << '\n';
		detectedDevice.push_back(deviceName);

		deviceList += strlen(deviceList) + 1;
	} while (*deviceList != '\0');
}

void VoiceSystem::capture()
{
	alcGetIntegerv(inputDevice, ALC_CAPTURE_SAMPLES, sizeof availableInputSamples, &availableInputSamples);
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

	ALint processed;
	alGetSourcei(playbackSource, AL_BUFFERS_PROCESSED, &processed);
	if (processed > 0)
	{
		for (auto i = 0; i < processed; i++)
		{
			auto wasFront = playbackQueue.front();
			playbackQueue.pop_front();
			availableBufferList.push_back(wasFront);
			alSourceUnqueueBuffers(playbackSource, 1, &wasFront);
		}
	}

	//If we've been called with a buffer, we will load the data in OpenAL and queue it
	if (buffer)
	{
		if (availableBufferList.empty())
		{
			AnnDebug() << "no buffers were available!, adding one.";
			ALuint newBuffer;
			alGenBuffers(1, &newBuffer);
			availableBufferList.push_back(newBuffer);

			AnnDebug() << "System uses " << availableBufferList.size() + playbackQueue.size() << " OpenAL buffers";
		}

		auto nextBuffer = availableBufferList.front();
		availableBufferList.pop_front();
		alBufferData(nextBuffer, AL_FORMAT_MONO16, buffer->data(), BUFFER_SIZE * sizeof(sample_t), SAMPLE_RATE);
		alSourceQueueBuffers(playbackSource, 1, &nextBuffer);
		playbackQueue.push_back(nextBuffer);
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

	speex_preprocess_run(preprocess_state, buffer->data() + (frame * 160));

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