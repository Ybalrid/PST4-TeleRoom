#pragma once
#include <Annwvyn.h>
#include "PST4VoiceSystem.hpp"
namespace PST4
{
	class RemoteUser
	{
	public:
		RemoteUser(size_t sessionId) : remoteId{ sessionId }
		{
			head = Annwvyn::AnnGetGameObjectManager()->createGameObject("headProxy.mesh", std::to_string(remoteId) + "_head");
			leftHand = Annwvyn::AnnGetGameObjectManager()->createGameObject("lhand.mesh", std::to_string(remoteId) + "_lHand");
			rightHand = Annwvyn::AnnGetGameObjectManager()->createGameObject("rhand.mesh", std::to_string(remoteId) + "_rHand");

			leftHand->setInvisible();
			rightHand->setInvisible();

			alGenSources(1, &audioSource);
		}

		~RemoteUser()
		{
			Annwvyn::AnnGetGameObjectManager()->removeGameObject(head);
			Annwvyn::AnnGetGameObjectManager()->removeGameObject(leftHand);
			Annwvyn::AnnGetGameObjectManager()->removeGameObject(rightHand);
		}

		void setHeadPose(Annwvyn::AnnVect3 pos, Annwvyn::AnnQuaternion orient)
		{
			head->setPosition(pos);
			head->setOrientation(orient);

			alSource3f(audioSource, AL_POSITION, pos.x, pos.y, pos.z);
		}

		void setHandPoses(Annwvyn::AnnVect3 leftPos, Annwvyn::AnnQuaternion leftOrient, Annwvyn::AnnVect3 rightPos, Annwvyn::AnnQuaternion rightOrient)
		{
			leftHand->setVisible();
			leftHand->setPosition(leftPos);
			leftHand->setOrientation(leftOrient);

			rightHand->setVisible();
			rightHand->setPosition(rightPos);
			rightHand->setOrientation(rightOrient);
		}

		void steamVoice(VoiceSystem::buffer640* buffer)
		{
			ALint processed;
			alGetSourcei(audioSource, AL_BUFFERS_PROCESSED, &processed);
			if (processed > 0)
			{
				for (auto i = 0; i < processed; i++)
				{
					auto wasFront = playbackQueue.front();
					playbackQueue.pop_front();
					availableBuffers.push_back(wasFront);
					alSourceUnqueueBuffers(audioSource, 1, &wasFront);
				}
			}

			//If we've been called with a buffer, we will load the data in OpenAL and queue it
			if (buffer)
			{
				if (availableBuffers.empty())
				{
					Annwvyn::AnnDebug() << "no buffers were available for remote : " << remoteId;

					ALuint newBuffer;
					alGenBuffers(1, &newBuffer);
					availableBuffers.push_back(newBuffer);

					Annwvyn::AnnDebug() << "Remote : " << remoteId << " uses " << availableBuffers.size() + playbackQueue.size() << " OpenAL buffers";
				}

				auto nextBuffer = availableBuffers.front();
				availableBuffers.pop_front();
				alBufferData(nextBuffer, AL_FORMAT_MONO16, buffer->data(), VoiceSystem::BUFFER_SIZE * sizeof(VoiceSystem::sample_t), VoiceSystem::SAMPLE_RATE);
				alSourceQueueBuffers(audioSource, 1, &nextBuffer);
				playbackQueue.push_back(nextBuffer);
			}

			ALint state;
			alGetSourcei(audioSource, AL_SOURCE_STATE, &state);
			if (state != AL_PLAYING) alSourcePlay(audioSource);
		}

	private:
		size_t remoteId;
		std::shared_ptr<Annwvyn::AnnGameObject> head, leftHand, rightHand;
		VoiceSystem::bufferPlaybackQueue availableBuffers, playbackQueue;
		ALuint audioSource;
	};
}