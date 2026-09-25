#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#if WITH_ENGINE
	#include "UnrealEngine.h"
#endif

class FGenericWindow;
class GenericApplication;

/**
 * Engine loop driven by GuardedMain (UE: FEngineLoop). Without the engine framework
 * (WITH_ENGINE=0, e.g. the PS2 game) it owns the platform application and main window and ticks
 * FTicker::GetCoreTicker(); game modules register their per-frame work there.
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

	int32 GetExitCode() const
	{
		return ExitCode;
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
};

/** The process' engine loop (UE: GEngineLoop). */
extern FEngineLoop GEngineLoop;

/** Runs PreInit / Init / Tick until exit is requested / Exit (UE: GuardedMain). */
int32 GuardedMain(int32 ArgC, char* ArgV[]);
