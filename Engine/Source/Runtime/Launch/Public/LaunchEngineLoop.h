#pragma once

#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#if WITH_ENGINE
	#include "UnrealEngine.h"
#endif

class FGenericWindow;
class GenericApplication;

/**
 * Engine loop driven by GuardedMain (UE: FEngineLoop).
 *
 * - PreInit: the command line, the project, the config and the log, then the platform application, the main window
 *   and the RHI on its context (RHIInit; none for a desktop game with -nullrhi), then the statically linked modules.
 * - With the engine (WITH_ENGINE=1, LeonGame): Init creates GEngine of `[/Script/Engine.Engine] GameEngine=`, queues
 *   `-ExecCmds=`, calls GEngine->Init and Start (the first map); Tick pumps the window's events, runs the deferred
 *   commands and GEngine->Tick; Exit calls GEngine->PreExit.
 * - Without it (WITH_ENGINE=0, the PS2 game) Tick ticks FTicker::GetCoreTicker(), where game modules register their
 *   per-frame work, and presents.
 */
class LAUNCH_API FEngineLoop
#if WITH_ENGINE
	: public IEngineLoop
#endif
{
public:
	FEngineLoop();
	virtual ~FEngineLoop();

	/** Platform application + main window, then statically linked module startup. */
	int32 PreInit(int32 ArgC, char* ArgV[]);

	virtual int32 Init();

	/** One frame: poll devices, tick, platform end-of-frame hooks, present. */
	virtual void Tick();

	/** Module shutdown, window + application teardown. */
	void Exit();

	/** The process return code: the loop's own failure, else the one a requested exit asked for (0 by default). */
	int32 GetExitCode() const
	{
		return ExitCode != 0 ? ExitCode : static_cast<int32>(GetRequestedEngineExitCode());
	}

	/** The platform application (nullptr before PreInit). */
	virtual GenericApplication* GetApplication() const
	{
		return Application.Get();
	}

	/** The main window (nullptr before PreInit, and when nothing renders). */
	virtual FGenericWindow* GetMainWindow() const
	{
		return MainWindow.Get();
	}

private:
	TUniquePtr<GenericApplication> Application;
	TSharedPtr<FGenericWindow> MainWindow;
	uint64 LastFrameCycles = 0;
	int32 ExitCode = 0;

	/** -Screenshot=<file.bmp>: frame ExitAfterFrames is saved (Leon). */
	FString ScreenshotPath;
	/** -ExitAfterFrames=N: the game exits after frame N (Leon). */
	int32 ExitAfterFrames = 0;
	int32 FrameCount = 0;
	/** -tick=<Hz>: the fixed step of a headless run (Leon). */
	float TickHz = 60.0f;
	/** FPlatformTime::Seconds of the last frame / the next headless step. */
	double LastFrameTime = 0.0;
	double NextHeadlessTick = 0.0;
};

/** The process' engine loop (UE: GEngineLoop). */
extern FEngineLoop GEngineLoop;

/** Runs PreInit / Init / Tick until exit is requested / Exit (UE: GuardedMain). */
int32 GuardedMain(int32 ArgC, char* ArgV[]);
