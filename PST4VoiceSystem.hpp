#pragma once

#include <Annwvyn.h>
#include <string>
#include <deque>
#include <algorithm>
#include <speex/speex.h>

namespace PST4
{
	///Capture, encode and decode voice for VOIP functionality
	class VoiceSystem
	{
	public:
		constexpr static size_t SAMPLE_RATE{ 8000 };							//8000 is "telephone" quality. Good enough for now
		constexpr static size_t DROP_THRESHOLD{ 15 };							//Will clear the recording queue if we accumulate half a sec of audio
		constexpr static size_t PLAYBACK_CACHE{ 4 };							//Number of buffer in the playback queue
		constexpr static size_t FRAME_SIZE{ 160 };								//Number of samples in a frame speex expect in "narrow band" mode
		constexpr static size_t FRAMES_PER_BUFFER{ 4 };							//Number of frames in a buffer
		constexpr static size_t BUFFER_SIZE{ 4 * FRAME_SIZE };					//Size of one buffer, in sample count. For speex compression, must be
		constexpr static size_t CAPTURE_BUFFER_SIZE{ 4 * BUFFER_SIZE };			//Size of the internal buffer of the capture device

		using sample_t = int16_t;												//Type of an audio sample
		using byte_t = char;													//Type of a single byte
		using buffer640 = std::array<sample_t, BUFFER_SIZE>;					//Type of a buffer of 640 samples
		using bufferQueue = std::deque<buffer640>;								//Queue of buffers for recording
		using bufferPlaybackQueue = std::array<ALuint, PLAYBACK_CACHE>;			//Queue of buffer for playback

		VoiceSystem();
		~VoiceSystem();

		///Capture audio. Call that as often as possible
		void capture();

		///Return true if you can grab a buffer
		bool bufferAvailable() const;

		///Return a buffer and pop it from the bufferQueue. Have to be compiled with a C++11 compliant compiler for RVO (will waste time in copying the array otherwise)
		buffer640 getNextBufferToSend();

		///Locally play a buffer. For debugging only, this will probably sound weird for the user if you pipe it's own audio, due to the delay
		void debugPlayback(buffer640* buffer = nullptr);

		///Encode a frame from a buffer and returns it. You can take advantage of C++11 RVO with this function.
		///Speex in narrow band mode is expecting 160 samples of 16bits to compress. We are recording in one buffer via OpenAL
		///a multiple of frames in one single buffer. Number of frames in a buffer is FRAMES_PER_BUFFER
		std::vector<byte_t> encode(buffer640* buffer, byte_t frame);
		buffer640 decode(unsigned char* frameSizes, unsigned char* data);

	private:
		static constexpr bool sanityCheck()
		{
			// ReSharper disable CppRedundantBooleanExpressionArgument
			return 2 * sizeof(byte_t) == sizeof(sample_t)		//Types match the expected sizes
				&& SAMPLE_RATE / FRAME_SIZE == 50				//The expected duration in ms of an audio frame is respected
				;
			// ReSharper restore CppRedundantBooleanExpressionArgument
		}

		///Process the list of input devices, and will populate the "detectedDevice" member accordingly
		void detectInputDevice(const char* deviceList);

		ALCdevice* inputDevice;
		ALuint playbackSource;
		bufferPlaybackQueue playbackQueue;
		ALuint indexProcessed, indexLastQueued, available;

		std::vector<std::string> detectedDevice;
		ALint availableInputSamples;
		bufferQueue queue;

		SpeexBits encBits, decBits;
		void* enc_state;
		void* dec_state;
	};
}