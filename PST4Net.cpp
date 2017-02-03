#include "stdafx.h"
#include "PST4Net.hpp"

using namespace PST4;
using namespace Annwvyn;

NetSubSystem* NetSubSystem::singleton(nullptr);

NetSubSystem::NetSubSystem(const std::string& serverHost) : AnnUserSubSystem("PST4Net"),
netState{ NetState::NOT_READY },
serverAddress{ serverHost },
p{ "Hello from Annwvyn!" },
lastHeartbeatTime{ 0 }
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
	peer->Shutdown(300);
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
		static int echoTime = 0;
		if (++echoTime > 100)
		{
			echoTime = 0;
			peer->Send(reinterpret_cast<char*>(&p), sizeof(p), PacketPriority::LOW_PRIORITY, PacketReliability::UNRELIABLE, 0, serverSystemAddress, false);
		}
		if (AnnGetEngine()->getTimeFromStartupSeconds() - lastHeartbeatTime > 5)
		{
			heartbeatPacket heartbeat;
			lastHBAck = peer->Send(reinterpret_cast<char*>(&heartbeat), sizeof(heartbeat), IMMEDIATE_PRIORITY, RELIABLE_WITH_ACK_RECEIPT, 0, serverSystemAddress, false);
			lastHeartbeatTime = AnnGetEngine()->getTimeFromStartupSeconds();
			AnnDebug() << "Sending heartbeat " << lastHBAck;
		}

		for (packet = peer->Receive(); packet;
			peer->DeallocatePacket(packet), packet = peer->Receive())
		{
			if (packet->data[0] == ID_CONNECTION_LOST)
			{
				AnnDebug() << "Connection definitively lost";
				netState = NetState::FALIED;
			}
			else if (packet->data[0] == ID_SND_RECEIPT_ACKED)
			{
				uint32_t ack;
				memcpy(&ack, packet->data + 1, 4);
				if (ack == lastHBAck)
					AnnDebug() << "last heartbeat ok";
				else
					AnnDebug() << "got ack for heartbeat " << ack << " and was expecting " << lastHBAck;
			}
			else
				AnnDebug() << "Got unknown packet message type : " << unsigned int(packet->data[0]);
		}
	}
	break;
	}
}

NetSubSystem* NetSubSystem::getNet()
{
	return singleton;
}