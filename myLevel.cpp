#include "stdafx.h"
#include "myLevel.hpp"

using namespace Annwvyn;

MyLevel::MyLevel() : constructLevel()
{
}

void MyLevel::load()
{
	//Load Ground:
	auto Ground(addGameObject("Ground.mesh", "GroundPlane"));
	Ground->setPosition({ 0, 0, 0 });
	Ground->setUpPhysics();

	//Create a light source
	auto Sun(addLightObject());
	Sun->setType(AnnLightObject::ANN_LIGHT_DIRECTIONAL);
	Sun->setDirection(AnnVect3{ -1, -1.5f, -1 }.normalisedCopy());

	AnnGetSceneryManager()->setAmbientLight(AnnColor(.25f, .25f, .25));

	auto player{ AnnGetPlayer() };
	player->regroundOnPhysicsBody();
}

void MyLevel::runLogic()
{
}