#include "GameApplication.h"

#include "CoreGlobals.h"
#include "Engine/GameEngine.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogLaunch, Log, All);

bool FGameApplication::Init(IEngineLoop* EngineLoop)
{
	const TCHAR* CmdLine = FCommandLine::Get();

	// Console strings stay ASCII: Windows cmd often is not UTF-8 (em dash / arrows mojibake).
	bHeadless = !FApp::CanEverRender();

	float Hz = 60.0f;
	(void)FParse::Value(CmdLine, "ExitAfterFrames=", ExitAfterFrames);
	if (FParse::Value(CmdLine, "Screenshot=", ScreenshotPath) && ExitAfterFrames <= 0)
	{
		ExitAfterFrames = 60;
	}
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
		? StaticLoadClass(UGameEngine::StaticClass(), nullptr, *GameEngineClassName)
		: UGameEngine::StaticClass();
	if (EngineClass == nullptr)
	{
		UE_LOG(LogLaunch, Error, "Failed to load the engine class '%s'", *GameEngineClassName);
		return false;
	}
	GEngine = NewObject<UEngine>(GetTransientPackage(), EngineClass);
	GEngine->AddToRoot();
	GEngine->Init(EngineLoop);
	if (!GEngine->IsInitialized())
	{
		UE_LOG(LogLaunch, Error, "Failed to initialize the engine");
		return false;
	}
	GEngine->Start();
	if (IsEngineExitRequested())
	{
		return false;
	}
	if (bHeadless)
	{
		UE_LOG(LogLaunch, Log, "Running headless @ %g Hz (Ctrl+C to stop)", static_cast<double>(TickHz));
		NextHeadlessTick = FPlatformTime::Seconds();
	}
	LastFrameTime = FPlatformTime::Seconds();
	return true;
}

bool FGameApplication::Tick()
{
	if (GEngine == nullptr || IsEngineExitRequested())
	{
		return false;
	}
	if (bHeadless)
	{
		// Fixed-timestep simulation (no render / present), paced to TickHz.
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
		return !IsEngineExitRequested();
	}

	const double Now = FPlatformTime::Seconds();
	const float DeltaTime = FMath::Min(static_cast<float>(Now - LastFrameTime), 0.1f);
	LastFrameTime = Now;
	++FrameCount;
	if (ExitAfterFrames > 0 && FrameCount > ExitAfterFrames)
	{
		return false;
	}
	if (!ScreenshotPath.IsEmpty() && FrameCount == ExitAfterFrames)
	{
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			GameEngine->RequestScreenshot(ScreenshotPath);
		}
	}
	GEngine->Tick(DeltaTime, false);
	return !IsEngineExitRequested();
}

void FGameApplication::Exit()
{
	if (GEngine == nullptr)
	{
		return;
	}
	GEngine->PreExit();
	GEngine->RemoveFromRoot();
	GEngine = nullptr;
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}
