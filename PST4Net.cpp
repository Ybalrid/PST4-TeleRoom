#include "stdafx.h"
#include "PST4Net.hpp"

using namespace PST4;
using namespace Annwvyn;

NetSubSystem* NetSubSystem::singleton(nullptr);

NetSubSystem::NetSubSystem(const std::string& serverHost) : AnnUserSubSystem("PST4Net"),
netState{ NetState::NOT_READY },
serverAddress{ serverHost },
p{ "Hello from Annwvyn!" },
lastHeartbeatTime{ 0 },
sessionId{ 0 }
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
		//Transmit current poses to server :
		if (sessionId)
		{
			headPosePacket head{ sessionId, AnnGetVRRenderer()->trackedHeadPose.position, AnnGetVRRenderer()->trackedHeadPose.orientation };
			peer->Send(reinterpret_cast<char*>(&head), sizeof(headPosePacket), LOW_PRIORITY, UNRELIABLE, 0, serverSystemAddress, false);
		}

		static int echoTime = 0;
		if (++echoTime > 1000)
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
			if (!sessionId)
			{
				if (packet->data[0] == ID_PST4_MESSAGE_SESSION_ID)
				{
					auto s2cID = reinterpret_cast<serverToClientIdPacket*>(packet->data);
					sessionId = s2cID->clientId;
					AnnDebug() << "Server given our session the ID : " << sessionId;
				}
				else
				{
					serverToClientIdPacket rq(0);
					peer->Send(reinterpret_cast<char*>(&rq), sizeof rq, IMMEDIATE_PRIORITY, RELIABLE, 0, serverSystemAddress, false);
					continue;
				}
			}

			if (sessionId) if (packet->data[0] == ID_PST4_MESSAGE_HEAD_POSE)
			{
				auto headPose = reinterpret_cast<headPosePacket*>(packet->data);
				if (headPose->sessionId != sessionId)
				{
					//Other client here!!!!!
					if (remotes.count(headPose->sessionId) == 0) remotes[headPose->sessionId] = std::make_unique<RemoteUser>(headPose->sessionId);
					else
					{
						remotes.at(headPose->sessionId)->setHeadPose(headPose->absPos.getAnnVect3(), headPose->absOrient.getAnnQuaternion());
					}
				}
				continue;
			}

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