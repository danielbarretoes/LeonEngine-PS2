#pragma once

#include "Containers/Ticker.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

#include <memory>

class FThirdPersonGameMode;

DECLARE_LOG_CATEGORY_EXTERN(LogThirdPerson, Log, All);

/** Primary game module: starts the game mode and ticks it from the core ticker. */
class THIRDPERSON_API FThirdPersonModule : public IModuleInterface
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
	FDelegateHandle TickHandle;
};
