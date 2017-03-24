#include "stdafx.h"
#include "myLevel.hpp"

using namespace Annwvyn;

MyLevel::MyLevel() : constructLevel()
{
}

void MyLevel::load()
{
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

	AnnGetPlayer()->setPosition(Annwvyn::AnnVect3(0, 0, 5));
	AnnGetPlayer()->setOrientation(Ogre::Euler(0));
	AnnGetPlayer()->resetPlayerPhysics();
	AnnGetPlayer()->regroundOnPhysicsBody();
}

void MyLevel::runLogic()
{
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
}