#include "stdafx.h"
#include "PST4Net.hpp"

using namespace PST4;
using namespace Annwvyn;

NetSubSystem* NetSubSystem::singleton(nullptr);

NetSubSystem::NetSubSystem(const std::string& serverHost) : AnnUserSubSystem("PST4Net"),
netState{ NetState::NOT_READY },
serverAddress{ serverHost }
{
	if (singleton) throw std::runtime_error("Tried to instantiate PST4::NetSubsystem more than once!");
	singleton = this;

	peer = RakNet::RakPeerInterface::GetInstance();
	peer->Startup(1, &sd, 1);
	AnnDebug() << "RakPeerInterface initialized";

	peer->Connect(serverAddress.c_str(), port, nullptr, 0);
	AnnDebug() << "Requested connection to " << serverAddress << ':' << port;
	netState = NetState::READY;
}

NetSubSystem::~NetSubSystem()
{
	singleton = nullptr;
	RakNet::RakPeerInterface::DestroyInstance(peer);
}

bool NetSubSystem::needUpdate()
{
	return true;
}

void NetSubSystem::update()
{
	switch (netState)
	{
	default:
	case NetState::NOT_READY:
		break;

	case NetState::READY:
		for (packet = peer->Receive(); packet;
			peer->DeallocatePacket(packet), packet = peer->Receive())
		{
			//Switch on the packed message ID
			switch (packet->data[0])
			{
			case ID_CONNECTION_ATTEMPT_FAILED:
				AnnDebug() << "Failed to connect to server : " << serverAddress << " on UDP port " << port;
				netState = NetState::FALIED;
				break;
			case ID_CONNECTION_REQUEST_ACCEPTED:
				AnnDebug() << "We are connected to the server! ";
				serverSystemAddress = packet->systemAddress;
				netState = NetState::CONNECTED;
				break;
			default:
				AnnDebug() << "Got unknown packet message type : " << unsigned int(packet->data[0]);
				break;
			}
		}
		break;

	case NetState::CONNECTED:
	{
		echoPacket p("Hello from Annwyvn!");
		peer->Send(reinterpret_cast<char*>(&p), sizeof(p), PacketPriority::HIGH_PRIORITY, PacketReliability::RELIABLE_ORDERED, 0, serverSystemAddress, false);
		break;
	}
	}
}

NetSubSystem* NetSubSystem::getNet()
{
	return singleton;
}