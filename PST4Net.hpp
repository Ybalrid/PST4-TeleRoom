#pragma once

#include <Annwvyn.h>

namespace PST4
{
	class NetSubSystem : public Annwvyn::AnnUserSubSystem
	{
	public:
		NetSubSystem();
		~NetSubSystem();
		bool needUpdate() override;
		void update() override;
		static NetSubSystem* getNet();
	private:
		static NetSubSystem* singleton;
	};
}