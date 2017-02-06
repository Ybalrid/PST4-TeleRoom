//Pre-compiled header are used in this project
#include "stdafx.h"

//Include Annwvyn Engine.
#include <Annwvyn.h>
//Every Annwvyn classes are in the Annwvyn namespace

//Include our level/stages here
#include "myLevel.hpp"
#include "PST4Net.hpp"

constexpr const char* const server{ "annwvyn.org" };

using namespace Annwvyn;
AnnMain() //The application entry point is "AnnMain()". return type int.
{
	//Initialize the engine
	AnnEngine::openConsole();//optional : open console
	AnnInit("NameOfApp");

	//Call physics initialization for the player body:
	AnnGetEngine()->initPlayerRoomscalePhysics();

	//Instantiate and register our basic level and "jump" to it:
	AnnGetLevelManager()->addLevel(std::make_shared<MyLevel>());
	AnnGetLevelManager()->jumpToFirstLevel();

	//The game is rendering here now:
	AnnGetEventManager()->useDefaultEventListener();

	AnnGetEngine()->registerUserSubSystem(std::make_shared<PST4::NetSubSystem>(server));

	//The game runs here
	AnnGetEngine()->startGameplayLoop();

	//Destroy the engine now. Engine is RAII managed, but you can manually release it with this command
	AnnQuit();

	return EXIT_SUCCESS;
}