#include "stdafx.h"
#include "PST4Net.hpp"

using namespace PST4;
using namespace Annwvyn;

NetSubSystem* NetSubSystem::singleton(nullptr);

NetSubSystem::NetSubSystem(const std::string& serverHost) : AnnUserSubSystem("PST4Net")
{
	if (singleton) throw std::runtime_error("Tried to instantiante PST4::NetSubsystem more than once!");
	singleton = this;
}

NetSubSystem::~NetSubSystem()
{
	singleton = nullptr;
}

bool NetSubSystem::needUpdate()
{
	return true;
}

void NetSubSystem::update()
{
}

NetSubSystem* NetSubSystem::getNet()
{
	return singleton;
}