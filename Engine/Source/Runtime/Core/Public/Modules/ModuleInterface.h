#pragma once

#include "CoreTypes.h"

/** Interface every module implements (UE: IModuleInterface). Registered with IMPLEMENT_MODULE. */
class IModuleInterface
{
public:
	virtual ~IModuleInterface() = default;

	/** Called after the module is created, in dependency order. */
	virtual void StartupModule()
	{
	}

	/** Called before the module is destroyed, in reverse dependency order. */
	virtual void ShutdownModule()
	{
	}

	/** True for modules that contain game code (IMPLEMENT_GAME_MODULE). */
	virtual bool IsGameModule() const
	{
		return false;
	}
};
