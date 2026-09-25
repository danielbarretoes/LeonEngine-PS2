#include "LaunchEngineLoop.h"

#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IProjectManager.h"
#include "Logging/LogMacros.h"
#include "Logging/LogSuppressionInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
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
	/** The log file (desktop): <Project>/Saved/Logs/<Name>.log. */
	TUniquePtr<FOutputDeviceFile> GLogFile;

	constexpr int32 MainWindowWidth = 640;
	constexpr int32 MainWindowHeight = 448;

#if WITH_ENGINE
	/** The desktop game session (UE: GEngine + the game viewport), driven one frame per Tick. */
	std::unique_ptr<FGameApplication> GGameApplication;
#endif
} // namespace

FEngineLoop::FEngineLoop() = default;

FEngineLoop::~FEngineLoop() = default;

int32 FEngineLoop::PreInit(int32 ArgC, char* ArgV[])
{
	ArgCount = ArgC;
	Args = ArgV;

	// The command line first: everything below may read it (UE: FEngineLoop::PreInit order).
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));

	// The project: -project=<path>.lproj (or a first argument ending in .lproj), else the target's own project.
	FString ProjectFile;
	if (!FParse::Value(FCommandLine::Get(), "project=", ProjectFile))
	{
		const TCHAR* Stream = FCommandLine::Get();
		const FString FirstToken = FParse::Token(Stream, false);
		if (FirstToken.EndsWith(".lproj"))
		{
			ProjectFile = FirstToken;
		}
	}
	if (!ProjectFile.IsEmpty())
	{
		FPaths::SetProjectFilePath(ProjectFile);
		FApp::SetProjectName(*FPaths::GetBaseFilename(ProjectFile));
	}
	else if (LEON_PROJECT_NAME[0] != 0)
	{
		FApp::SetProjectName(LEON_PROJECT_NAME);
		FPaths::SetProjectFilePath(FPaths::ProjectDir() + LEON_PROJECT_NAME + ".lproj");
	}

	// Config, then the log file and the verbosity it asks for.
	FConfigCacheIni::InitializeConfigSystem();
#if PLATFORM_DESKTOP
	GLogFile = MakeUnique<FOutputDeviceFile>();
	GLog->AddOutputDevice(GLogFile.Get());
#endif
	FLogSuppressionInterface::Get().ProcessConfigAndCommandLine();

	if (FPaths::IsProjectFilePathSet())
	{
		// Logs a warning itself when the .lproj cannot be read (PS2 without the PCSX2 host filesystem).
		IProjectManager::Get().LoadProjectFile(FPaths::GetProjectFilePath());
	}

	UE_LOG(LogInit, Log, "Command line: %s", FCommandLine::Get());
	UE_LOG(LogInit, Log, "Base directory: %s", FPlatformProcess::BaseDir());
	UE_LOG(LogInit, Log, "Project: %s (%s)", FApp::HasProjectName() ? FApp::GetProjectName() : "none",
		*FPaths::ProjectDir());

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
	// LeonGame: LeonGame [-map=<.llev>] [-nullrhi] [-tick=<Hz>] [-showstats].
	GGameApplication = std::make_unique<FGameApplication>();
	if (!GGameApplication->Init())
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
	const float DeltaTime =
		static_cast<float>(FPlatformTime::CyclesToMicroseconds(NowCycles - LastFrameCycles)) / 1000000.0f;
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

	// Saves what changed in the user config layer (desktop).
	if (GConfig != nullptr)
	{
		GConfig->Flush(false);
	}
	GLog->Flush();
	if (GLogFile)
	{
		GLog->RemoveOutputDevice(GLogFile.Get());
		GLogFile.Reset();
	}
}
