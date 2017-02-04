#pragma once
#include <Annwvyn.h>
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

	private:
		size_t remoteId;
		std::shared_ptr<Annwvyn::AnnGameObject> head, leftHand, rightHand;
	};
}