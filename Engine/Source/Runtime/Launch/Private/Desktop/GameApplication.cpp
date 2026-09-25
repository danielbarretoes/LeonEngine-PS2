#include "GameApplication.h"

#include "CoreGlobals.h"
#include "GameFramework/DefaultGameMode.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Level/LevelLoader.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RuntimeInput.h"

DEFINE_LOG_CATEGORY_STATIC(LogLaunch, Log, All);

namespace
{

	constexpr const TCHAR* DefaultMap = "LevelTemplates/Starter.llev";

	/** A path as given (absolute or relative to the working directory), else a legacy content key under Engine/Content.
	 */
	[[nodiscard]] FString ResolveMapPath(const FString& Map)
	{
		if (FPaths::FileExists(Map))
		{
			return FString(*FPaths::ConvertRelativePathToFull(Map));
		}
		return FString(*FPaths::ResolveLegacyContentPath(Map));
	}

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

bool FGameApplication::Init()
{
	const TCHAR* CmdLine = FCommandLine::Get();

	// Console strings stay ASCII: Windows cmd often is not UTF-8 (em dash / arrows mojibake).
	bHeadless = FParse::Param(CmdLine, "nullrhi");

	bool bShowStats = false;
	if (GConfig != nullptr)
	{
		GConfig->GetBool("/Script/Engine.Engine", "bShowStatsByDefault", bShowStats, GEngineIni);
	}
	bShowStats |= FParse::Param(CmdLine, "showstats");

	float Hz = 60.0f;
	if (FParse::Value(CmdLine, "tick=", Hz) && Hz >= 1.0f && Hz <= 240.0f)
	{
		TickHz = Hz;
	}

	FString Map;
	if (!FParse::Value(CmdLine, "map=", Map) && GConfig != nullptr)
	{
		GConfig->GetString("/Script/EngineSettings.GameMapsSettings", "GameDefaultMap", Map, GEngineIni);
	}
	if (Map.IsEmpty() || Map.Equals("None", ESearchCase::IgnoreCase))
	{
		Map = DefaultMap;
	}
	const FString MapPath = ResolveMapPath(Map);

	const int32 ResolutionX = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionX", 1280);
	const int32 ResolutionY = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionY", 720);
	const FString Title = FApp::HasProjectName() ? FString("Leon - ") + FApp::GetProjectName() : FString("Leon");

	Engine = MakeUnique<UGameEngine>();
	if (bHeadless)
	{
		if (!Engine->InitializeHeadless())
		{
			UE_LOG(LogLaunch, Error, "Failed to initialize headless engine");
			Engine.Reset();
			return false;
		}
	}
	else if (!Engine->Initialize(ResolutionX, ResolutionY, *Title))
	{
		UE_LOG(LogLaunch, Error, "Failed to initialize engine");
		Engine.Reset();
		return false;
	}
	if (bShowStats && !bHeadless)
	{
		Engine->SetHudStatsVisible(true);
	}
	if (!bHeadless)
	{
		WireDefaultInput(*Engine);
	}

	if (!LoadLevelFile(*Engine, MapPath))
	{
		UE_LOG(LogLaunch, Error, "Failed to load map '%s'", *Map);
		Engine->Shutdown();
		Engine.Reset();
		return false;
	}
	GameMode = MakeUnique<ADefaultGameMode>();
	GameMode->OnEnter(*Engine, MapPath);

	if (bHeadless)
	{
		UE_LOG(LogLaunch, Log, "Running '%s' headless @ %g Hz (Ctrl+C to stop)", *MapPath, static_cast<double>(TickHz));
		NextHeadlessTick = FPlatformTime::Seconds();
	}
	else
	{
		Engine->Start();
	}
	LastFrameTime = FPlatformTime::Seconds();
	return true;
}

bool FGameApplication::Tick()
{
	if (!Engine)
	{
		return false;
	}
	if (bHeadless)
	{
		// Fixed-timestep simulation (no render / present), paced to TickHz.
		const float StepSeconds = 1.0f / (TickHz < 1.0f ? 1.0f : TickHz);
		if (!Engine->IsRunning())
		{
			return false;
		}
		GameMode->Tick(*Engine, StepSeconds);
		// Keep the console current when stdout is redirected (CI smoke, servers stopped with Ctrl+C).
		GLog->Flush();
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
		return Engine->IsRunning();
	}

	const double Now = FPlatformTime::Seconds();
	const float DeltaTime = FMath::Min(static_cast<float>(Now - LastFrameTime), 0.1f);
	LastFrameTime = Now;
	return Engine->Tick(DeltaTime, [this](float Dt) { GameMode->Tick(*Engine, Dt); });
}

void FGameApplication::Exit()
{
	if (GameMode && Engine)
	{
		GameMode->OnExit(*Engine);
	}
	GameMode.Reset();
	if (Engine)
	{
		Engine->Shutdown();
		Engine.Reset();
	}
}
