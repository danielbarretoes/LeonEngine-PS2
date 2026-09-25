#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"

#include <memory>

class FGenericWindow;
class GenericApplication;

/**
 * Engine loop driven by GuardedMain (UE: FEngineLoop). Without the engine framework
 * (WITH_ENGINE=0, e.g. the PS2 game) it owns the platform application and main window and ticks
 * FTicker::GetCoreTicker(); game modules register their per-frame work there.
 */
class LAUNCH_API FEngineLoop
{
public:
	FEngineLoop();
	~FEngineLoop();

	/** Platform application + main window, then statically linked module startup. */
	int32 PreInit(int32 ArgC, char* ArgV[]);

	int32 Init();

	/** One frame: poll devices, tick, platform end-of-frame hooks, present. */
	void Tick();

	/** Module shutdown, window + application teardown. */
	void Exit();

	int32 GetExitCode() const
	{
		return ExitCode;
	}

	/** The platform application (nullptr before PreInit). */
	GenericApplication* GetApplication() const
	{
		return Application.get();
	}

	/** The main window (nullptr before PreInit / on desktop engine targets). */
	FGenericWindow* GetMainWindow() const
	{
		return MainWindow.Get();
	}

private:
	std::unique_ptr<GenericApplication> Application;
	TSharedPtr<FGenericWindow> MainWindow;
	uint64 LastFrameCycles = 0;
	int32 ArgCount = 0;
	char** Args = nullptr;
	int32 ExitCode = 0;
};

/** The process' engine loop (UE: GEngineLoop). */
extern FEngineLoop GEngineLoop;

/** Runs PreInit / Init / Tick until exit is requested / Exit (UE: GuardedMain). */
int32 GuardedMain(int32 ArgC, char* ArgV[]);
