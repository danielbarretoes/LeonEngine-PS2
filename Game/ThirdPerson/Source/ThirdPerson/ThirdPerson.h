#pragma once

#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"

#include <memory>

class FThirdPersonGameMode;

/** Primary game module: starts the game mode and ticks it from the core ticker. */
class FThirdPersonModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual bool IsGameModule() const override
	{
		return true;
	}

private:
	std::unique_ptr<FThirdPersonGameMode> GameMode;
	FTicker::FDelegateHandle TickHandle;
};
