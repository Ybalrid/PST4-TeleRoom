#pragma once

#include <Annwvyn.h>

#include <RakPeerInterface.h>
#include <RakNetTypes.h>
#include <BitStream.h>
#include <MessageIdentifiers.h>

namespace PST4
{
	class NetSubSystem : public Annwvyn::AnnUserSubSystem
	{
	public:
		NetSubSystem(const std::string& serverHostname);
		~NetSubSystem();
		bool needUpdate() override;
		void update() override;
		static NetSubSystem* getNet();
	private:
		static NetSubSystem* singleton;
	};
}