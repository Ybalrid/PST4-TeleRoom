#include "stdafx.h"
#include "myLevel.hpp"
#include "PST4Net.hpp"

using namespace Annwvyn;

MyLevel::MyLevel() : constructLevel()
{
	squaredThreshold = AnnVect3{ 1,1,1 };

	grabber = std::make_shared<ObjectGrabber>(this);
}

void MyLevel::load()
{
	AnnGetEventManager()->addListener(grabber);
	AnnGetSceneryManager()->setWorldBackgroundColor(AnnColor(0.8, 0.8, 0.8));
	AnnGetSceneryManager()->setAmbientLight(AnnColor(.6f, .6f, .6f));
	auto house = addGameObject("teleroom/house.mesh");
	house->setUpPhysics();

	auto Table = addGameObject("teleroom/table.mesh");
	Table->setPosition(-5, -1.5, 0);
	Table->setUpPhysics();

	//Add other source of light
	auto Sun = addLightObject();
	Sun->setType(AnnLightObject::ANN_LIGHT_DIRECTIONAL);
	Sun->setDirection(AnnVect3::NEGATIVE_UNIT_Y + 1.5f* AnnVect3::NEGATIVE_UNIT_Z);

	auto sword = addGameObject("Sword.mesh", "sword");
	sword->setScale(0.1, 0.1, 0.1);
	sword->setPosition(-5, 0, 0);
	sword->setUpPhysics(1, convexShape);
	PST4::NetSubSystem::getNet()->addSyncedPhyscisObject(sword);
	grabable.push_back(sword);

	auto sword2 = addGameObject("Sword.mesh", "sword2");
	sword2->setScale(0.1, 0.1, 0.1);
	sword2->setPosition(-6, 0, 0);
	sword2->setUpPhysics(1, convexShape);
	PST4::NetSubSystem::getNet()->addSyncedPhyscisObject(sword2);
	grabable.push_back(sword2);

	AnnGetPlayer()->setPosition(Annwvyn::AnnVect3(0, 0, 5));
	AnnGetPlayer()->setOrientation(Ogre::Euler(AnnAngle::degree(90)));
	AnnGetPlayer()->resetPlayerPhysics();
	AnnGetPlayer()->regroundOnPhysicsBody();
}

void MyLevel::unload()
{
	AnnGetEventManager()->removeListener(grabber);
	grabable.clear();
	reachable.clear();
	AnnLevel::unload();
}

void MyLevel::grabRequested()
{
	if (grabbed)
	{
		ungrab();
		return;
	}

	AnnDebug() << "should grab an object";
	if (!reachable.empty())
	{
		auto playerPosition = AnnGetVRRenderer()->trackedHeadPose.position;

		//Actually sort the array by player distance
		std::sort(reachable.begin(), reachable.end(),
			[&](std::shared_ptr<AnnGameObject> a, std::shared_ptr<AnnGameObject> b)
		{
			auto distA = playerPosition.squaredDistance(a->getPosition());
			auto distB = playerPosition.squaredDistance(b->getPosition());

			return distA > distB;
		});

		auto toGrab = reachable[0];

		if (PST4::NetSubSystem::getNet()->dynamicObjectOwned(toGrab->getName())) return;
		grabbed = toGrab;

		AnnGetPhysicsEngine()->getWorld()->removeRigidBody(grabbed->getBody());
		AnnDebug() << "grab object : " << toGrab->getName();
		PST4::NetSubSystem::getNet()->setGrabbedObject(grabbed);
	}
}

void MyLevel::ungrab()
{
	AnnGetPhysicsEngine()->getWorld()->addRigidBody(grabbed->getBody());
	grabbed->getBody()->activate();
	PST4::NetSubSystem::getNet()->setGrabbedObject(nullptr);
	grabbed = nullptr;
}

std::shared_ptr<Annwvyn::AnnGameObject> MyLevel::getGrabbed()
{
	return grabbed;
}

void MyLevel::runLogic()
{
	reachable.clear();
	for (auto obj : grabable)
	{
		auto position = AnnGetVRRenderer()->trackedHeadPose.position;

		if (position.squaredDistance(obj->getPosition()) <= squaredThreshold.squaredLength())
		{
			reachable.push_back(obj);
		}
	}

	for (auto controller : AnnGetVRRenderer()->getHandControllerArray()) if (controller)
	{
		if (!controller->getModel())
		{
			if (controller->getSide() == AnnHandController::rightHandController)
				controller->attachModel(AnnGetEngine()->getSceneManager()->createEntity("rhand.mesh"));
			else
				controller->attachModel(AnnGetEngine()->getSceneManager()->createEntity("lhand.mesh"));
		}
	}

	if (grabbed)
	{
		auto pose = AnnGetVRRenderer()->trackedHeadPose;

		grabbed->setPosition(pose.position + pose.orientation*AnnVect3::NEGATIVE_UNIT_Z);
		grabbed->setOrientation(pose.orientation);
	}
}

ObjectGrabber::ObjectGrabber(MyLevel* l) : constructListener()
{
	level = l;
	lastState = false;
}

void ObjectGrabber::HandControllerEvent(Annwvyn::AnnHandControllerEvent e)
{
}

void ObjectGrabber::MouseEvent(Annwvyn::AnnMouseEvent e)
{
	//If clicked
	if (!lastState && e.getButtonState(MouseButtonId::Left))
	{
		level->grabRequested();
	}

	//save last
	lastState = e.getButtonState(MouseButtonId::Left);
}