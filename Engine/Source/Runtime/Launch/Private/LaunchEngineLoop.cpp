#include "LaunchEngineLoop.h"

#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/IPlatformFileOpenLogWrapper.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "IPlatformFilePak.h"
#include "Interfaces/IProjectManager.h"
#include "Logging/LogMacros.h"
#include "Logging/LogSuppressionInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MemStack.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UnrealClient.h"

FEngineLoop GEngineLoop;

DEFINE_LOG_CATEGORY_STATIC(LogLaunch, Log, All);

namespace
{
	/** The log file (desktop): <Project>/Saved/Logs/<Name>.log. */
	TUniquePtr<FOutputDeviceFile> GLogFile;

	/** The pak platform file PreInit put on top of the chain, if any. */
	TUniquePtr<FPakPlatformFile> GPakPlatformFile;

	/** The file open order log PreInit put on top of the chain (-LogFileOpenOrder), if any. */
	TUniquePtr<FPlatformFileOpenLog> GFileOpenLog;

	/**
	 * The platform file wrappers the command line and the build ask for, on top of the physical one (UE:
	 * LaunchCheckForFileOverride): the pak platform file when the build has paks (.lpak files in the project's
	 * Content/Paks folder, or -pak), always in Shipping, which reads nothing but its paks. False when a Shipping build
	 * finds no pak.
	 */
	bool LaunchCheckForFileOverride()
	{
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
		// On top: the order the game opens its files in, as the paks name them (Docs/PLANS/ps2-shipping.md N23).
		TUniquePtr<FPlatformFileOpenLog> FileOpenLog = MakeUnique<FPlatformFileOpenLog>();
		IPlatformFile& Top = FPlatformFileManager::Get().GetPlatformFile();
		if (FileOpenLog->ShouldBeUsed(&Top, FCommandLine::Get()) && FileOpenLog->Initialize(&Top, FCommandLine::Get()))
		{
			FPlatformFileManager::Get().SetPlatformFile(*FileOpenLog);
			GFileOpenLog = MoveTemp(FileOpenLog);
		}
		return true;
	}

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
} // namespace

FEngineLoop::FEngineLoop() = default;

FEngineLoop::~FEngineLoop() = default;

int32 FEngineLoop::PreInit(int32 ArgC, char* ArgV[])
{
	PreInitCycles = FPlatformTime::Cycles64();
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
#if !PLATFORM_DESKTOP
	// A console build has no defaults to fall back on: without its staged Engine config (the pak or the loose files
	// beside the executable) nothing below sizes itself right, so say where it looked and stop (UE: a fatal error).
	const FConfigFile* EngineConfig = GConfig->FindConfigFile(GEngineIni);
	if (EngineConfig == nullptr || EngineConfig->Num() == 0)
	{
		// The executable itself tells a folder that cannot be read (PCSX2 without its host filesystem) from one that
		// holds no cooked content (the build's output, Binaries/<Platform>/, instead of the staged game).
		const FString Executable = FString(FPlatformProcess::BaseDir()) + FPlatformProcess::ExecutableName(false);
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*Executable))
		{
			UE_LOG(LogInit, Error,
				"No Engine config in %s (nor a pak in %sContent/Paks/): '%s' holds the executable but no cooked "
				"content; "
				"this is a build output, boot the staged game (BuildCookRun -stage -pak, or Package.bat)",
				*FPaths::EngineConfigDir(), *FPaths::ProjectDir(), FPlatformProcess::BaseDir());
		}
		else
		{
			UE_LOG(LogInit, Error,
				"No Engine config in %s (nor a pak in %sContent/Paks/): the base directory '%s' is not readable",
				*FPaths::EngineConfigDir(), *FPaths::ProjectDir(), FPlatformProcess::BaseDir());
		}
		return 1;
	}
#endif
#if PLATFORM_DESKTOP
	FPaths::ApplyLogDirectoryOverrides();
	GLogFile = MakeUnique<FOutputDeviceFile>();
	GLog->AddOutputDevice(GLogFile.Get());
#endif
	FLogSuppressionInterface::Get().ProcessConfigAndCommandLine();
	// The memory budgets of the platform (PS2Engine.ini's), checked from here on.
	FLowLevelMemTracker::LoadBudgetsFromConfig();

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
	const bool bCreateMainWindow = FApp::CanEverRender();
	const int32 WindowWidth = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionX", 1280);
	const int32 WindowHeight = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionY", 896);
	const FString WindowTitle = FApp::HasProjectName() ? FString("Leon - ") + FApp::GetProjectName() : FString("Leon");
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
	const TCHAR* CmdLine = FCommandLine::Get();

	// Leon's capture switches: -Screenshot=<file.bmp> saves frame -ExitAfterFrames=N (60 by default), then the game
	// exits; -ExitAfterSeconds=N exits after N seconds. The steps are the engine's fixed ones (UEngine::
	// UpdateTimeAndHandleMaxTickRate; -benchmark: they do not wait for the clock).
	(void)FParse::Value(CmdLine, "ExitAfterFrames=", ExitAfterFrames);
	int32 Seconds = 0;
	if (FParse::Value(CmdLine, "ExitAfterSeconds=", Seconds) && Seconds > 0)
	{
		ExitAfterSeconds = uint32(Seconds);
	}
	if (FParse::Value(CmdLine, "Screenshot=", ScreenshotPath) && ExitAfterFrames <= 0)
	{
		ExitAfterFrames = 60;
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
		UE_LOG(LogLaunch, Log, "Running headless, steps of 1/%u s%s (Ctrl+C to stop)",
			GEngine->GetFixedStepClock().GetStepsPerSecond(), FApp::IsBenchmarking() ? ", unpaced (-benchmark)" : "");
	}
	InitEndCycles = FPlatformTime::Cycles64();
	return 0;
}

void FEngineLoop::Tick()
{
	if (GEngine == nullptr)
	{
		RequestEngineExit("No engine");
		return;
	}
	// The frame's time and the fixed steps it holds (ps2-shipping D4); a paced headless run waits here for its step.
	GEngine->UpdateTimeAndHandleMaxTickRate();
	const float DeltaTime = static_cast<float>(FApp::GetDeltaTime());
	const uint64 NowCycles = FPlatformTime::Cycles64();

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
	GEngine->TickDeferredCommands();

	++FrameCount;
	if (ExitAfterFrames > 0 && FrameCount > ExitAfterFrames)
	{
		RequestEngineExit("ExitAfterFrames");
		return;
	}
	if (ExitAfterSeconds > 0 &&
		FPlatformTime::CyclesToMicroseconds(NowCycles - InitEndCycles) >= uint64(ExitAfterSeconds) * 1000000ull)
	{
		RequestEngineExit("ExitAfterSeconds");
		return;
	}
	if (!ScreenshotPath.IsEmpty() && FrameCount == ExitAfterFrames)
	{
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	}

	// The world steps and the frame is drawn (windowed) between its last two steps; the frame's temporaries go with it.
	GEngine->Tick(DeltaTime, false);
	FMemStack::Get().EndFrame();
	if (FrameCount == 1)
	{
		// The load time (Docs/PLANS/ps2-shipping.md N23): the engine's start to the first frame, on the platform's
		// clock (the emulated EE's in PCSX2).
		const auto Seconds = [this](uint64 Cycles)
		{ return double(FPlatformTime::CyclesToMicroseconds(Cycles - PreInitCycles)) / 1000000.0; };
		UE_LOG(LogLaunch, Display, "First frame after %.2f s (the map ready after %.2f s), from the engine's start",
			Seconds(FPlatformTime::Cycles64()), Seconds(InitEndCycles));
		// What the paks' reads cost until then (Docs/PLANS/ps2-shipping.md N24b): the disc's reads and the block
		// cache's.
		for (const bool bAsync : {false, true})
		{
			const FPakFile::FReadStats Reads = FPakFile::GetReadStats(bAsync);
			UE_LOG(LogLaunch, Display,
				"The paks until the first frame, %s: %u read(s) from the file, %llu KB in %llu ms; %u from the block "
				"cache",
				bAsync ? TEXT("the IO thread") : TEXT("the game thread"), Reads.FileReads, Reads.FileBytes / 1024,
				FPlatformTime::CyclesToMicroseconds(Reads.FileCycles) / 1000, Reads.CachedReads);
		}
	}
}

void FEngineLoop::Exit()
{
	// The engine ends first: the world, then the renderer while the window's context exists (UE: GEngine->PreExit).
	if (GEngine != nullptr)
	{
		GEngine->PreExit();
		GEngine->RemoveFromRoot();
		GEngine = nullptr;
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
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
	// The file open order, then the paks: nothing reads a file after this.
	if (GFileOpenLog)
	{
		(void)GFileOpenLog->WriteOrderFile();
		FPlatformFileManager::Get().SetPlatformFile(*GFileOpenLog->GetLowerLevel());
		GFileOpenLog.Reset();
	}
	if (GPakPlatformFile)
	{
		FPlatformFileManager::Get().SetPlatformFile(*GPakPlatformFile->GetLowerLevel());
		GPakPlatformFile.Reset();
	}
}
