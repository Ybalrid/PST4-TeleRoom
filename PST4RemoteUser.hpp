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
		}

		~RemoteUser()
		{
			Annwvyn::AnnGetGameObjectManager()->removeGameObject(head);
		}

		void setHeadPose(Annwvyn::AnnVect3 pos, Annwvyn::AnnQuaternion orient)
		{
			head->setPosition(pos);
			head->setOrientation(orient);
		}
	private:
		size_t remoteId;
		std::shared_ptr<Annwvyn::AnnGameObject> head;
	};
}