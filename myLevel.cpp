#include "stdafx.h"
#include "myLevel.hpp"
#include "PST4Net.hpp"

using namespace Annwvyn;

MyLevel::MyLevel() : constructLevel()
{
	squaredThreshold = AnnVect3{ 1,1,1 };

	grabber = std::make_shared<ObjectGrabber>(this);

	holder = grabbedBy::mouse;
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
		AnnVect3 playerPosition = AnnGetVRRenderer()->trackedHeadPose.position;

		switch (holder)
		{
		case grabbedBy::leftHand:
			playerPosition = AnnGetVRRenderer()->getHandControllerArray()[0]->getWorldPosition();
			break;
		case grabbedBy::rightHand:
			playerPosition = AnnGetVRRenderer()->getHandControllerArray()[1]->getWorldPosition();
			break;
		default:break;
		}

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

void MyLevel::setHolder(grabbedBy gb)
{
	holder = gb;
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
		AnnVect3 position = AnnGetVRRenderer()->trackedHeadPose.position;
		AnnQuaternion orientation = AnnGetVRRenderer()->trackedHeadPose.orientation;
		AnnVect3 offset = AnnVect3::ZERO;

		switch (holder)
		{
		case grabbedBy::leftHand:
			position = AnnGetVRRenderer()->getHandControllerArray()[0]->getWorldPosition();
			orientation = AnnGetVRRenderer()->getHandControllerArray()[0]->getWorldOrientation();
			break;
		case grabbedBy::rightHand:
			position = AnnGetVRRenderer()->getHandControllerArray()[1]->getWorldPosition();
			orientation = AnnGetVRRenderer()->getHandControllerArray()[1]->getWorldOrientation();
			break;
		case grabbedBy::mouse:
			offset = AnnVect3::NEGATIVE_UNIT_Z;
			break;
		}

		grabbed->setPosition(position + orientation*offset);
		grabbed->setOrientation(orientation * AnnQuaternion(AnnDegree(180), AnnVect3::UNIT_Y));
	}
}

ObjectGrabber::ObjectGrabber(MyLevel* l) : constructListener()
{
	level = l;
	lastState = false;
}

void ObjectGrabber::HandControllerEvent(Annwvyn::AnnHandControllerEvent e)
{
	static float leftValue = 0, rightValue = 0;

	auto controller = e.getController();

	bool triggered = false;

	switch (controller->getSide())
	{
	case AnnHandController::leftHandController:
		if (controller->getAxis(2).getValue() > 0.5f && leftValue <= 0.5f)
		{
			triggered = true;
			level->setHolder(MyLevel::grabbedBy::leftHand);
		}
		if (controller->getAxis(2).getValue() < 0.5f && leftValue >= 0.5f)
		{
			triggered = true;
			level->setHolder(MyLevel::grabbedBy::leftHand);
		}

		leftValue = controller->getAxis(2).getValue();
		break;
	case AnnHandController::rightHandController:
		if (controller->getAxis(2).getValue() > 0.5f && rightValue <= 0.5f)
		{
			triggered = true;
			level->setHolder(MyLevel::grabbedBy::rightHand);
		}
		if (controller->getAxis(2).getValue() < 0.5f && rightValue >= 0.5f)
		{
			triggered = true;
			level->setHolder(MyLevel::grabbedBy::rightHand);
		}

		rightValue = controller->getAxis(2).getValue();
		break;
	}

	if (triggered) level->grabRequested();
}

void ObjectGrabber::MouseEvent(Annwvyn::AnnMouseEvent e)
{
	//If clicked
	if (!lastState && e.getButtonState(Left))
	{
		level->grabRequested();
	}

	//save last
	lastState = e.getButtonState(Left);
}