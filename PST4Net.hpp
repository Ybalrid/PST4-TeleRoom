#pragma once

#include <Annwvyn.h>

#include <RakPeerInterface.h>
#include <RakNetTypes.h>
#include <BitStream.h>
#include <MessageIdentifiers.h>

#include <PST4Packets.hpp>
#include <cstring>

namespace PST4
{
	class NetSubSystem : public Annwvyn::AnnUserSubSystem
	{
	public:

		enum class NetState { NOT_READY, READY, CONNECTED, FALIED };

		NetSubSystem(const std::string& serverHostname);
		~NetSubSystem();
		bool needUpdate() override;
		void update() override;
		static NetSubSystem* getNet();
	private:
		NetState netState;
		std::string serverAddress;
		static NetSubSystem* singleton;
		RakNet::RakPeerInterface* peer;
		RakNet::Packet* packet;
		RakNet::SocketDescriptor sd;
		RakNet::SystemAddress serverSystemAddress;
		static constexpr const auto port = 42420U;
	};
}