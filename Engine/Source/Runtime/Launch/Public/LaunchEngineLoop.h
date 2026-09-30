#pragma once

#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UnrealEngine.h"

class FGenericWindow;
class GenericApplication;

/**
 * Engine loop driven by GuardedMain (UE: FEngineLoop).
 *
 * - PreInit: the command line, the project, the config and the log, then the platform application, the main window
 *   and the RHI on its context (RHIInit; none for a desktop game with -nullrhi), then the statically linked modules.
 * - Init creates GEngine of `[/Script/Engine.Engine] GameEngine=`, queues `-ExecCmds=`, calls GEngine->Init and Start
 *   (the first map); Tick pumps the window's events, runs the deferred commands and GEngine->Tick; Exit calls
 *   GEngine->PreExit. Every game target is compiled against the engine (LeonGame, ShooterGame, on every platform).
 */
class LAUNCH_API FEngineLoop : public IEngineLoop
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
	int32 ExitCode = 0;

	/** -Screenshot=<file.bmp>: frame ExitAfterFrames is saved (Leon). */
	FString ScreenshotPath;
	/** -ExitAfterFrames=N: the game exits after frame N (Leon). */
	int32 ExitAfterFrames = 0;
	/**
	 * -ExecCmdsAfterFrames=N (Leon, not in Shipping): -ExecCmds= run at the start of frame N instead of the first, held
	 * in DelayedCommands until then.
	 */
	int32 ExecCmdsAfterFrames = 0;
	TArray<FString> DelayedCommands;
	/**
	 * -ExitAfterSeconds=N: the game exits once N seconds passed since Init (Leon; a measured run's bound, MeasurePS2),
	 * and when Init ended (FPlatformTime::Cycles64).
	 */
	uint32 ExitAfterSeconds = 0;
	uint64 InitEndCycles = 0;
	/** When PreInit started (FPlatformTime::Cycles64): the load time to the first frame is logged from it. */
	uint64 PreInitCycles = 0;
	int32 FrameCount = 0;
};

/** The process' engine loop (UE: GEngineLoop). */
extern FEngineLoop GEngineLoop;

/** Runs PreInit / Init / Tick until exit is requested / Exit (UE: GuardedMain). */
int32 GuardedMain(int32 ArgC, char* ArgV[]);
