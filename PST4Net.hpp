#pragma once

#include <Annwvyn.h>

#include <RakPeerInterface.h>
#include <RakNetTypes.h>
#include <RakVoice.h>
#include <BitStream.h>
#include <MessageIdentifiers.h>

#define I_AM_CLIENT
#include <PST4Packets.hpp>
#include <cstring>

#include "PST4RemoteUser.hpp"
#include "PST4VoiceSystem.hpp"

namespace PST4
{
	class NetSubSystem : public Annwvyn::AnnUserSubSystem
	{
	public:

		///State of the connection
		enum class NetState { NOT_READY, READY, CONNECTED, FALIED };

		///Connect to the port 42420 on the given host
		NetSubSystem(const std::string& serverHostname);

		///Cleanly disconnect here
		~NetSubSystem();

		///Locked to return true
		bool needUpdate() override;

		///Get execution of our netcode here
		void update() override;

		///Access to this class singleton instance
		static NetSubSystem* getNet();

	private:

		void sendCycle();
		void handleRecivedHeadPose();
		void handleReceivedHandPose();
		void handleReceivedVoiceBuffer();
		void waitAndRequestSessionID();
		void handleEndOfRemoteSession();
		void receiveCycle();

		///State
		NetState netState;

		///text address of the server
		std::string serverAddress;

		///Last packet sent ACK's number. For heartbeat
		uint32_t lastHBAck;

		///Singleton
		static NetSubSystem* singleton;

		///The peer, our interface with the network
		RakNet::RakPeerInterface* peer;

		///Pointer to the packet we're processing
		RakNet::Packet* packet;

		///Socket descriptor
		RakNet::SocketDescriptor sd;

		///systemAddress of the server, you need that to send packet to the server directly
		RakNet::SystemAddress serverSystemAddress;

		///Port of the server application. Must have an open UDP path to this port
		static constexpr const auto port = 42420U;

		///A packet with an echo message
		echoPacket p;

		///Time since last heartbeat worked
		double lastHeartbeatTime;

		///ID of the session. Server will give us this number.
		size_t sessionId;

		///Map of remotes, indexed to their session id
		std::unordered_map<size_t, std::unique_ptr<RemoteUser>> remotes;

		//RakNet::RakVoice rakvoice; <- we're using our own

		///Object that permit to record, encode and decode audio.
		std::unique_ptr<VoiceSystem> voiceSystem;
	};
}