#include "LaunchEngineLoop.h"

#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "DynamicRHI.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformFilemanager.h"
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

#if PLATFORM_DESKTOP
	#include "IPlatformFilePak.h"
#endif

#if WITH_ENGINE
	#if !PLATFORM_DESKTOP
		#error "WITH_ENGINE targets currently require a desktop platform (the gameplay framework is host-only)"
	#endif
	#include "Engine/Engine.h"
	#include "Engine/GameEngine.h"
	#include "UObject/GarbageCollection.h"
	#include "UObject/Package.h"
	#include "UnrealClient.h"
#endif

FEngineLoop GEngineLoop;

DEFINE_LOG_CATEGORY_STATIC(LogLaunch, Log, All);

namespace
{
	/** The log file (desktop): <Project>/Saved/Logs/<Name>.log. */
	TUniquePtr<FOutputDeviceFile> GLogFile;

#if PLATFORM_DESKTOP
	/** The pak platform file PreInit put on top of the chain, if any. */
	TUniquePtr<FPakPlatformFile> GPakPlatformFile;
#endif

	/**
	 * The platform file wrappers the command line and the build ask for, on top of the physical one (UE:
	 * LaunchCheckForFileOverride): the pak platform file when the build has paks (.lpak files in the project's
	 * Content/Paks folder, or -pak), always in Shipping, which reads nothing but its paks. False when a Shipping build
	 * finds no pak.
	 */
	bool LaunchCheckForFileOverride()
	{
#if PLATFORM_DESKTOP
		IPlatformFile& CurrentPlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		TUniquePtr<FPakPlatformFile> PakPlatformFile = MakeUnique<FPakPlatformFile>();
		if (PakPlatformFile->ShouldBeUsed(&CurrentPlatformFile, FCommandLine::Get()))
		{
			if (!PakPlatformFile->Initialize(&CurrentPlatformFile, FCommandLine::Get()))
			{
				return false;
			}
			FPlatformFileManager::Get().SetPlatformFile(*PakPlatformFile);
			GPakPlatformFile = MoveTemp(PakPlatformFile);
		}
#endif
		return true;
	}

#if WITH_ENGINE
	/** The game window's size: [/Script/Engine.GameViewportClient] DefaultResolutionX / Y of the Engine config. */
	[[nodiscard]] int32 GetEngineInt(const TCHAR* Section, const TCHAR* Key, int32 Default)
	{
		int32 Value = Default;
		if (GConfig != nullptr)
		{
			GConfig->GetInt(Section, Key, Value, GEngineIni);
		}
		return Value;
	}
#endif
} // namespace

FEngineLoop::FEngineLoop() = default;

FEngineLoop::~FEngineLoop() = default;

int32 FEngineLoop::PreInit(int32 ArgC, char* ArgV[])
{
	// The command line first: everything below may read it (UE: FEngineLoop::PreInit order).
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));

	// The project: -project=<path>.lproj (or a first argument ending in .lproj), else the target's own project, else a
	// staged build's.
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
	else if (FPaths::IsStaged())
	{
		// A staged content-only project runs the engine's game target renamed after it (UE: UE4Game): the project is
		// the folder above Binaries/, and its .lproj comes from the pak.
		FString ProjectDir = FPaths::ProjectDir();
		ProjectDir.LeftChopInline(1);
		const FString ProjectName = FPaths::GetCleanFilename(ProjectDir);
		FApp::SetProjectName(*ProjectName);
		FPaths::SetProjectFilePath(FPaths::ProjectDir() + ProjectName + ".lproj");
	}

	// The paks mount before anything reads a file: the config, the .lproj and the content may all be in them.
	if (!LaunchCheckForFileOverride())
	{
		RequestEngineExit("No pak file: this build reads its content from <Project>/Content/Paks/*.lpak only");
		return 1;
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

	// The platform application, the main window and the RHI on its graphics context (UE: PreInit's RHIInit). The
	// modules starting up below (the renderer, the PS2 game module) can already use them. A desktop game with -nullrhi
	// has none: the engine runs headless.
#if WITH_ENGINE
	const bool bCreateMainWindow = FApp::CanEverRender();
	const int32 WindowWidth = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionX", 1280);
	const int32 WindowHeight = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionY", 720);
	const FString WindowTitle = FApp::HasProjectName() ? FString("Leon - ") + FApp::GetProjectName() : FString("Leon");
#else
	const bool bCreateMainWindow = true;
	constexpr int32 WindowWidth = 640;
	constexpr int32 WindowHeight = 448;
	const FString WindowTitle(LEON_TARGET_NAME);
#endif
	if (bCreateMainWindow)
	{
		Application.Reset(FPlatformApplicationMisc::CreateApplication());
		MainWindow = Application->MakeWindow();
		if (!MainWindow->Create(WindowWidth, WindowHeight, *WindowTitle))
		{
			UE_LOG(LogInit, Error, "FEngineLoop: failed to create the main window");
			MainWindow.Reset();
			return 1;
		}
		if (!RHIInit(MainWindow->GetRHIProcAddressLoader()))
		{
			UE_LOG(LogInit, Error, "FEngineLoop: failed to initialize the RHI");
			MainWindow->Destroy();
			MainWindow.Reset();
			return 1;
		}
		MainWindow->BindRHIViewport();
	}

	FModuleManager::Get().StartupStaticallyLinkedModules();
	return 0;
}

int32 FEngineLoop::Init()
{
#if WITH_ENGINE
	const TCHAR* CmdLine = FCommandLine::Get();

	// Leon's capture and pacing switches: -Screenshot=<file.bmp> saves frame -ExitAfterFrames=N (60 by default), then
	// the game exits; -tick=<Hz> paces a headless run.
	(void)FParse::Value(CmdLine, "ExitAfterFrames=", ExitAfterFrames);
	if (FParse::Value(CmdLine, "Screenshot=", ScreenshotPath) && ExitAfterFrames <= 0)
	{
		ExitAfterFrames = 60;
	}
	float Hz = 60.0f;
	if (FParse::Value(CmdLine, "tick=", Hz) && Hz >= 1.0f && Hz <= 240.0f)
	{
		TickHz = Hz;
	}

	// GEngine's class comes from the config (UE: FEngineLoop::Init, plan decision D18).
	FString GameEngineClassName;
	if (GConfig != nullptr)
	{
		GConfig->GetString("/Script/Engine.Engine", "GameEngine", GameEngineClassName, GEngineIni);
	}
	UClass* EngineClass = !GameEngineClassName.IsEmpty()
		? StaticLoadClass(UEngine::StaticClass(), nullptr, *GameEngineClassName)
		: UGameEngine::StaticClass();
	if (EngineClass == nullptr)
	{
		UE_LOG(LogLaunch, Error, "Failed to load the engine class '%s'", *GameEngineClassName);
		ExitCode = 1;
		RequestEngineExit("No engine class");
		return ExitCode;
	}
	GEngine = NewObject<UEngine>(GetTransientPackage(), EngineClass);
	GEngine->AddToRoot();

	// -ExecCmds="Cmd1;Cmd2": console commands for the first frame, once the map plays (UE: DeferredCommands; UE
	// separates them with commas, which Leon accepts too).
	FString ExecCmds;
	if (FParse::Value(CmdLine, "ExecCmds=", ExecCmds, false))
	{
		ExecCmds.ReplaceInline(TEXT(","), TEXT(";"));
		TArray<FString> Commands;
		ExecCmds.ParseIntoArray(Commands, TEXT(";"), true);
		for (const FString& Command : Commands)
		{
			const FString Trimmed = Command.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				GEngine->DeferredCommands.Add(Trimmed);
			}
		}
	}

	GEngine->Init(this);
	if (!GEngine->IsInitialized())
	{
		UE_LOG(LogLaunch, Error, "Failed to initialize the engine");
		ExitCode = 1;
		RequestEngineExit("Engine initialization failed");
		return ExitCode;
	}
	// The game instance opens the first map.
	GEngine->Start();
	if (IsEngineExitRequested())
	{
		ExitCode = 1;
		return ExitCode;
	}
	if (MainWindow == nullptr)
	{
		UE_LOG(LogLaunch, Log, "Running headless @ %g Hz (Ctrl+C to stop)", static_cast<double>(TickHz));
		NextHeadlessTick = FPlatformTime::Seconds();
	}
	LastFrameTime = FPlatformTime::Seconds();
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
	FTicker::GetCoreTicker().Tick(DeltaTime);
	// The platform's events (UE: Slate pumps them before the engine ticks).
	if (Application)
	{
		Application->PollGameDeviceState();
	}
	if (MainWindow)
	{
		MainWindow->PollEvents();
	}
	if (GEngine == nullptr)
	{
		RequestEngineExit("No engine");
		return;
	}
	GEngine->TickDeferredCommands();

	++FrameCount;
	if (ExitAfterFrames > 0 && FrameCount > ExitAfterFrames)
	{
		RequestEngineExit("ExitAfterFrames");
		return;
	}
	if (!ScreenshotPath.IsEmpty() && FrameCount == ExitAfterFrames)
	{
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	}

	if (MainWindow)
	{
		// A windowed frame: the real frame time, at most 0.1 s.
		const double Now = FPlatformTime::Seconds();
		const float FrameTime = FMath::Min(static_cast<float>(Now - LastFrameTime), 0.1f);
		LastFrameTime = Now;
		GEngine->Tick(FrameTime, false);
	}
	else
	{
		// Headless: fixed steps paced to -tick=<Hz> (no render, no present).
		const float StepSeconds = 1.0f / (TickHz < 1.0f ? 1.0f : TickHz);
		GEngine->Tick(StepSeconds, false);
		NextHeadlessTick += static_cast<double>(StepSeconds);
		const double Now = FPlatformTime::Seconds();
		if (NextHeadlessTick < Now)
		{
			NextHeadlessTick = Now; // fell behind: resync instead of spiralling
		}
		else
		{
			FPlatformProcess::Sleep(static_cast<float>(NextHeadlessTick - Now));
		}
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
	// The engine ends first: the world, then the renderer while the window's context exists (UE: GEngine->PreExit).
	if (GEngine != nullptr)
	{
		GEngine->PreExit();
		GEngine->RemoveFromRoot();
		GEngine = nullptr;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
#endif
	FModuleManager::Get().ShutdownModules();
	RHIExit();
	if (MainWindow)
	{
		MainWindow->Destroy();
		MainWindow.Reset();
	}
	Application.Reset();

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
#if PLATFORM_DESKTOP
	// The paks go last: nothing reads a file after this.
	if (GPakPlatformFile)
	{
		FPlatformFileManager::Get().SetPlatformFile(*GPakPlatformFile->GetLowerLevel());
		GPakPlatformFile.Reset();
	}
#endif
}
