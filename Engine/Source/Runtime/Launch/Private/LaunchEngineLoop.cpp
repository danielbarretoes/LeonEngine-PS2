#include "LaunchEngineLoop.h"

#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformTime.h"
#include "Modules/ModuleManager.h"
#include "PlatformEngineLoopHooks.h"

#if WITH_ENGINE
	#if !PLATFORM_DESKTOP
		#error "WITH_ENGINE targets currently require a desktop platform (the gameplay framework is host-only)"
	#endif
	#include "Desktop/GameApplication.h"
#endif

#include <cstdio>
#include <cstring>
#include <memory>

FEngineLoop GEngineLoop;

namespace
{
	constexpr int32 MainWindowWidth = 640;
	constexpr int32 MainWindowHeight = 448;

#if WITH_ENGINE
	/** The desktop game session (UE: GEngine + the game viewport), driven one frame per Tick. */
	std::unique_ptr<FGameApplication> GGameApplication;
#endif
}

FEngineLoop::FEngineLoop() = default;

FEngineLoop::~FEngineLoop() = default;

int32 FEngineLoop::PreInit(int32 ArgC, char* ArgV[])
{
	ArgCount = ArgC;
	Args = ArgV;

#if !WITH_ENGINE
	// Without the engine framework the loop owns the platform application and the main window;
	// modules starting up below (the primary game module) can already use them.
	Application.reset(FPlatformApplicationMisc::CreateApplication());
	MainWindow = Application->MakeWindow();
	if (!MainWindow->Create(MainWindowWidth, MainWindowHeight, LEON_TARGET_NAME))
	{
		std::printf("FEngineLoop: failed to create the main window\n");
		MainWindow.reset();
		return 1;
	}
#endif

	FModuleManager::Get().StartupStaticallyLinkedModules();
	return 0;
}

int32 FEngineLoop::Init()
{
#if WITH_ENGINE
	// LeonGame runs a project pack: LeonGame --pack <Name> [game flags].
	const char* PackName = LEON_PROJECT_NAME;
	for (int32 Index = 1; Index + 1 < ArgCount; ++Index)
	{
		if (std::strcmp(Args[Index], "--pack") == 0)
		{
			PackName = Args[Index + 1];
		}
	}
	GGameApplication = std::make_unique<FGameApplication>();
	if (!GGameApplication->Init(ArgCount, Args, PackName, [](UGameEngine&, FGameplayRouter&) {}))
	{
		GGameApplication.reset();
		ExitCode = 1;
		RequestEngineExit("Game session failed to start");
		return ExitCode;
	}
#endif
	LastFrameCycles = FPlatformTime::Cycles64();
	return 0;
}

void FEngineLoop::Tick()
{
	const uint64 NowCycles = FPlatformTime::Cycles64();
	const float DeltaTime = static_cast<float>(FPlatformTime::CyclesToMicroseconds(NowCycles - LastFrameCycles)) / 1000000.0f;
	LastFrameCycles = NowCycles;

#if WITH_ENGINE
	// The engine frame (input, world tick, render, present) runs inside the game session.
	FTicker::GetCoreTicker().Tick(DeltaTime);
	if (!GGameApplication || !GGameApplication->Tick())
	{
		RequestEngineExit("Game session finished");
	}
#else

	Application->PollGameDeviceState();
	MainWindow->PollEvents();

	FTicker::GetCoreTicker().Tick(DeltaTime);

	FPlatformEngineLoopHooks::EndFrame(*MainWindow, *Application);
	MainWindow->SwapBuffers();
	FPlatformEngineLoopHooks::PostPresent();

	if (MainWindow->ShouldClose())
	{
		RequestEngineExit("Main window closed");
	}
#endif
}

void FEngineLoop::Exit()
{
#if WITH_ENGINE
	if (GGameApplication)
	{
		GGameApplication->Exit();
		GGameApplication.reset();
	}
#endif
	FModuleManager::Get().ShutdownModules();
	if (MainWindow)
	{
		MainWindow->Destroy();
		MainWindow.reset();
	}
	Application.reset();
}
