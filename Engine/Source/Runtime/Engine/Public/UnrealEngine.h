#pragma once

#include "CoreMinimal.h"

class FGenericWindow;
class GenericApplication;

/**
 * The engine loop as the engine sees it, so engine code never includes the launch module's headers (UE: IEngineLoop,
 * UnrealEngine.h). FEngineLoop implements it and passes itself to UEngine::Init.
 */
class IEngineLoop
{
public:
	virtual ~IEngineLoop() = default;

	virtual int32 Init() = 0;
	virtual void Tick() = 0;

	/**
	 * The platform application and the main window FEngineLoop::PreInit created, or null when nothing renders
	 * (`-nullrhi`) (Leon: UE's engine makes its game window through Slate).
	 */
	[[nodiscard]] virtual GenericApplication* GetApplication() const = 0;
	[[nodiscard]] virtual FGenericWindow* GetMainWindow() const = 0;
};
