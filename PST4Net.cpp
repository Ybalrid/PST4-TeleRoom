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
	dynObjectDeviationThreshold = 0.15f;
	dynObjectDeviationThreshold *= dynObjectDeviationThreshold;

	if (singleton) throw std::runtime_error("Tried to instantiate PST4::NetSubsystem more than once!");
	singleton = this;

	peer = RakNet::RakPeerInterface::GetInstance();
	peer->Startup(1, &sd, 1);
	AnnDebug() << "RakPeerInterface initialized";

	peer->Connect(serverAddress.c_str(), port, nullptr, 0);
	AnnDebug() << "Requested connection to " << serverAddress << ':' << port;
	netState = NetState::READY;
	try
	{
		voiceSystem = std::make_unique<PST4::VoiceSystem>();
	}
	catch (const std::exception& e)
	{
		AnnDebug() << "Exception catched : "
			<< e.what();
		AnnDebug() << "The user probably doesn't have a microphone. the voiceSystem is not initialized";
		voiceSystem.reset(nullptr);
	}
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
	if (voiceSystem)
		voiceSystem->capture();
	sendCycle();
	receiveCycle();
}

NetSubSystem* NetSubSystem::getNet()
{
	return singleton;
}

void PST4::NetSubSystem::addSyncedPhyscisObject(std::shared_ptr<Annwvyn::AnnGameObject> obj)
{
	syncedPhysicsObject[obj->getName()] = obj;
	lastOwner[obj->getName()] = 0;
}

void PST4::NetSubSystem::setGrabbedObject(std::shared_ptr<Annwvyn::AnnGameObject> obj)
{
	userGrabbedDynamicObject = obj;
}

bool NetSubSystem::dynamicObjectOwned(const std::string& objName)
{
	return lastOwner[objName] != 0;
}

void NetSubSystem::sendCycle()
{
	if (netState != NetState::CONNECTED) return void();
	if (sessionId)
	{
		//Send head position
		headPosePacket head{ sessionId, AnnGetVRRenderer()->trackedHeadPose.position, AnnGetVRRenderer()->trackedHeadPose.orientation };
		peer->Send(reinterpret_cast<char*>(&head), sizeof(headPosePacket), LOW_PRIORITY, UNRELIABLE, 0, serverSystemAddress, false);

		//If hands positions available, send hands positions
		handPosePacket handPose(sessionId, false);
		if (AnnGetVRRenderer()->getRecognizedControllerCount() > 0)
		{
			auto controllers = AnnGetVRRenderer()->getHandControllerArray();
			if (controllers[0] && controllers[1])
				handPose = handPosePacket(sessionId, controllers[0]->getWorldPosition(), controllers[0]->getWorldOrientation(), controllers[1]->getWorldPosition(), controllers[1]->getWorldOrientation());
		}
		peer->Send(reinterpret_cast<char*>(&handPose), sizeof handPose, LOW_PRIORITY, UNRELIABLE, 0, serverSystemAddress, false);

		for (auto elem : syncedPhysicsObject)
		{
			auto dynobj = elem.second;

			Annwvyn::AnnDebug() << dynobj->getName();
			dynamicSceneObjectPacket dsoPacket(dynobj->getName(),
				Vect3f(dynobj->getPosition()),
				Vect3f(dynobj->getScale()),
				Quatf(dynobj->getOrientation()));

			if (dynobj == userGrabbedDynamicObject)
			{
				dsoPacket.owner = sessionId;
			}

			//AnnDebug() << "sending position : " << dsoPacket.position;
			//AnnDebug() << "Object position is :" << AnnGetGameObjectManager()->getObjectFromID("sword")->getPosition();

			dsoPacket.sender = sessionId;
			peer->Send(reinterpret_cast<char*>(&dsoPacket), sizeof dsoPacket, LOW_PRIORITY, UNRELIABLE, 0, serverSystemAddress, false);
		}

		//If voice available, send voice packet
		if (voiceSystem)
		{
			if (voiceSystem->bufferAvailable())
			{
				auto buffer = voiceSystem->getNextBufferToSend();
				//voiceSystem->debugPlayback(&buffer); //Piping your own voice to the headset with a delay is really disturbing.

				std::array<std::vector<VoiceSystem::byte_t>, VoiceSystem::FRAMES_PER_BUFFER> compressed;
				voicePacket voice(sessionId);

				for (auto i{ 0 }; i < VoiceSystem::FRAMES_PER_BUFFER; i++)
				{
					compressed[i] = voiceSystem->encode(&buffer, i);
					//AnnDebug() << "frame " << i << " compressed datalen : " << compressed[i].size();
					voice.frameSizes[i] = unsigned char(compressed[i].size());
					memcpy(voice.data + voice.dataLen, compressed[i].data(), compressed[i].size());
					voice.dataLen += unsigned char(compressed[i].size());
				}

				RakNet::BitStream bitstream;
				bitstream.Write(RakNet::MessageID(ID_PST4_MESSAGE_VOICE_BUFFER));
				bitstream.Write(reinterpret_cast<char*>(&sessionId), sizeof(size_t));
				bitstream.Write(reinterpret_cast<VoiceSystem::byte_t*>(voice.frameSizes), 4);
				bitstream.Write(reinterpret_cast<char*>(&voice.dataLen), sizeof(unsigned char));
				bitstream.Write(reinterpret_cast<VoiceSystem::byte_t*>(voice.data), voice.dataLen);

				//TODO see if we can do fixed lenght packets for that.
				size_t sizeToSend(6 * sizeof(char) + sizeof(size_t) + voice.dataLen);
				//AnnDebug() << "size sent :" << sizeToSend;

				//peer->Send(, sizeof sizeToSend, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, 1, serverSystemAddress, false);
				peer->Send(&bitstream, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, 1, serverSystemAddress, false);
			}
			else
			{
				voiceSystem->debugPlayback();
			}
		}
	}

	if (AnnGetEngine()->getTimeFromStartupSeconds() - lastHeartbeatTime > 5)
	{
		heartbeatPacket heartbeat;
		lastHBAck = peer->Send(reinterpret_cast<char*>(&heartbeat), sizeof(heartbeat), IMMEDIATE_PRIORITY, RELIABLE_WITH_ACK_RECEIPT, 0, serverSystemAddress, false);
		lastHeartbeatTime = AnnGetEngine()->getTimeFromStartupSeconds();
		AnnDebug() << "Sending heartbeat " << lastHBAck;
	}
}

void NetSubSystem::handleRecivedHeadPose()
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
}

void NetSubSystem::handleReceivedHandPose()
{
	auto handPose = reinterpret_cast<handPosePacket*>(packet->data);
	if (handPose->hasHands && sessionId != handPose->sessionId)
	{
		if (remotes.count(handPose->sessionId) == 0) remotes[handPose->sessionId] = std::make_unique<RemoteUser>(handPose->sessionId);

		else remotes.at(handPose->sessionId)->setHandPoses(
			handPose->leftPos.getAnnVect3(), handPose->leftOrient.getAnnQuaternion(),
			handPose->rightPos.getAnnVect3(), handPose->rightOrient.getAnnQuaternion()
		);
	}
	return;
}

void NetSubSystem::handleReceivedVoiceBuffer()
{
	if (!voiceSystem) return;

	RakNet::BitStream voice(packet->data, packet->length, true);

	size_t remoteId;
	unsigned char dataLen = 0;
	unsigned char frameSizes[4];

	voice.IgnoreBytes(1);
	voice.Read(reinterpret_cast<char*>(&remoteId), sizeof(size_t));
	voice.Read(reinterpret_cast<char*>(frameSizes), 4);
	voice.Read(reinterpret_cast<char*>(&dataLen), 1);

	if (remoteId != sessionId)
	{
		unsigned char* data = new unsigned char[dataLen];
		voice.Read(reinterpret_cast<char*>(data), dataLen);

		//Extract audio buffer from the encoded frame
		auto buffer = voiceSystem->decode(frameSizes, data);
		delete[] data;

		//make associatedConnectedRemote queue that buffer
		if (remotes.count(remoteId) == 0) return; //If the remote is unknown
		remotes[remoteId]->streamVoice(&buffer);
	}
}

void PST4::NetSubSystem::handleReceivedDynamicObject()
{
	auto dynObj = reinterpret_cast<dynamicSceneObjectPacket*>(packet->data);

	auto obj = AnnGetGameObjectManager()->getObjectFromID(dynObj->idstring);
	auto rigidBody = obj->getBody();

	auto world = AnnGetPhysicsEngine()->getWorld();

	//If I'm not the "sender of this". Sender is master physics on the server
	if (dynObj->sender != sessionId)
	{
		world->removeRigidBody(rigidBody);

		if (dynObj->owner != sessionId)
		{
			obj->setPosition(dynObj->position.getAnnVect3());
			obj->setOrientation(dynObj->orientation.getAnnQuaternion());
		}
	}

	else if (dynObj->sender == sessionId)
	{
		if (dynObj->isOwned() && dynObj->owner != sessionId)
		{
			obj->setPosition(dynObj->position.getAnnVect3());
			obj->setOrientation(dynObj->orientation.getAnnQuaternion());

			rigidBody->setLinearVelocity(btVector3(0, 0, 0));
			rigidBody->setAngularVelocity(btVector3(0, 0, 0));
			rigidBody->activate();
		}
	}

	//This sets the current owner of the object for the next simzulated frame
	lastOwner[dynObj->idstring] = dynObj->owner;
}

void NetSubSystem::waitAndRequestSessionID()
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
	}
}

void NetSubSystem::handleEndOfRemoteSession()
{
	auto sessionEnded = reinterpret_cast<sessionEndedPacket*>(packet->data);
	remotes.erase(sessionEnded->sessionId);
}

void NetSubSystem::receiveCycle()
{
	switch (netState)
	{
	case NetState::NOT_READY:
	case NetState::FAILED:
		break;

	case NetState::READY: //Wait for the connection confirmation by the server
	{
		for (packet = peer->Receive(); packet;
			peer->DeallocatePacket(packet), packet = peer->Receive())
		{
			//Switch on the packed message ID
			switch (packet->data[0])
			{
			case ID_CONNECTION_ATTEMPT_FAILED:
				AnnDebug() << "Failed to connect to server : " << serverAddress << " on UDP port " << port;
				netState = NetState::FAILED;
				break;
			case ID_CONNECTION_REQUEST_ACCEPTED:
				AnnDebug() << "We are connected to the server! ";
				serverSystemAddress = packet->systemAddress;
				AnnDebug() << "Storing server system address : " << serverSystemAddress.ToString();
				netState = NetState::CONNECTED;
				break;
			default:
				//AnnDebug() << "Got unknown packet message type : " << unsigned int(packet->data[0]);
				break;
			}
		}
	}
	break;
	case NetState::CONNECTED:
		for (packet = peer->Receive(); packet;
			peer->DeallocatePacket(packet), packet = peer->Receive())
		{
			switch (packet->data[0])
			{
				//In all cases : if a remote session has ended,
			case ID_PST4_MESSAGE_NOTIFY_SESSION_END:
				handleEndOfRemoteSession();
				continue;

			case ID_CONNECTION_LOST:
				AnnDebug() << "Connection definitively lost";
				netState = NetState::FAILED;
				continue;
			case ID_SND_RECEIPT_ACKED:
			{
				uint32_t ack;
				memcpy(&ack, packet->data + 1, 4);
				if (ack == lastHBAck)
					AnnDebug() << "last heartbeat ok";
				else
					AnnDebug() << "got ack for heartbeat " << ack << " and was expecting " << lastHBAck;
				continue;
			}
			default:break;
			} ///////////////Enf of basic backet data :

			if (!sessionId)
			{
				waitAndRequestSessionID();
			}
			else //We know our own session, we can handle important game messages :
			{
				switch (packet->data[0])
				{
				case ID_PST4_MESSAGE_HEAD_POSE:
					handleRecivedHeadPose();
					break;
				case ID_PST4_MESSAGE_HAND_POSE:
					handleReceivedHandPose();
					break;
				case ID_PST4_MESSAGE_VOICE_BUFFER:
					handleReceivedVoiceBuffer();
					break;
				case ID_PST4_MESSAGE_DYNAMIC_SCENE_OBJECT:
					handleReceivedDynamicObject();
					break;
				default:
					AnnDebug() << "Session: " << sessionId << " unknown message form server: " << int(packet->data[0]);
				}
			}
		}
		break;
	}
}