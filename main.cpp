//Pre-compiled header are used in this project
#include "stdafx.h"

//Include Annwvyn Engine.
#include <Annwvyn.h>
//Every Annwvyn classes are in the Annwvyn namespace

//Include our level/stages here
#include "myLevel.hpp"
#include "PST4Net.hpp"

//constexpr const char* const server{ "annwvyn.org" };
constexpr const char* const server{ "localhost" };

#include <iostream>
#include <fstream>

using namespace Annwvyn;
AnnMain() //The application entry point is "AnnMain()". return type int.
{
	std::ifstream ipFile;
	ipFile.open("ip.cfg");

	std::string serverAddress;
	if (ipFile.is_open())
		ipFile >> serverAddress;
	ipFile.close();

	//Initialize the engine
	AnnEngine::openConsole();//optional : open console
	AnnInit("NameOfApp");

	AnnGetResourceManager()->addFileLocation("./media/teleroom/");
	AnnGetResourceManager()->initResources();

	//Call physics initialization for the player body:
	AnnGetEngine()->initPlayerStandingPhysics();

	//Instantiate and register our basic level and "jump" to it:
	AnnGetLevelManager()->addLevel(std::make_shared<MyLevel>());
	AnnGetLevelManager()->jumpToFirstLevel();

	//The game is rendering here now:
	AnnGetEventManager()->useDefaultEventListener();

	if (serverAddress.empty())
		serverAddress = server;
	AnnGetEngine()->registerUserSubSystem(std::make_shared<PST4::NetSubSystem>(serverAddress));

	//The game runs here
	AnnGetEngine()->startGameplayLoop();

	//Destroy the engine now. Engine is RAII managed, but you can manually release it with this command
	AnnQuit();

	return EXIT_SUCCESS;
}