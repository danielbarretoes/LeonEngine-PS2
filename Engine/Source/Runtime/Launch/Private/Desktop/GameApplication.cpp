#include "GameApplication.h"

#include "GameFramework/DefaultGameMode.h"
#include "Level/LevelLoader.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RuntimeInput.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <thread>

namespace
{

	constexpr const char* DefaultMap = "LevelTemplates/Starter.llev";

	/// A path as given (absolute or relative to the working directory), else a legacy content key under Engine/Content.
	[[nodiscard]] std::string ResolveMapPath(const FString& Map)
	{
		if (FPaths::FileExists(Map))
		{
			return std::string(*FPaths::ConvertRelativePathToFull(Map));
		}
		return std::string(*FPaths::ResolveLegacyContentPath(Map));
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
	const std::string MapPath = ResolveMapPath(Map);

	const int32 ResolutionX = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionX", 1280);
	const int32 ResolutionY = GetEngineInt("/Script/Engine.GameViewportClient", "DefaultResolutionY", 720);
	const std::string Title =
		FApp::HasProjectName() ? std::string("Leon - ") + FApp::GetProjectName() : std::string("Leon");

	Engine = std::make_unique<UGameEngine>();
	if (bHeadless)
	{
		if (!Engine->InitializeHeadless())
		{
			std::cerr << "Failed to initialize headless engine\n";
			Engine.reset();
			return false;
		}
	}
	else if (!Engine->Initialize(ResolutionX, ResolutionY, Title.c_str()))
	{
		std::cerr << "Failed to initialize engine\n";
		Engine.reset();
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
		std::cerr << "Failed to load map '" << *Map << "'\n";
		Engine->Shutdown();
		Engine.reset();
		return false;
	}
	GameMode = std::make_unique<ADefaultGameMode>();
	GameMode->OnEnter(*Engine, MapPath);

	if (bHeadless)
	{
		std::cout << "Running '" << MapPath << "' headless @ " << TickHz << " Hz (Ctrl+C to stop)\n";
		NextHeadlessTick = std::chrono::steady_clock::now();
	}
	else
	{
		Engine->Start();
	}
	LastFrameTime = std::chrono::steady_clock::now();
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
		using FClock = std::chrono::steady_clock;
		NextHeadlessTick += std::chrono::duration_cast<FClock::duration>(std::chrono::duration<double>(StepSeconds));
		const auto Now = FClock::now();
		if (NextHeadlessTick < Now)
		{
			NextHeadlessTick = Now; // fell behind: resync instead of spiralling
		}
		else
		{
			std::this_thread::sleep_until(NextHeadlessTick);
		}
		return Engine->IsRunning();
	}

	const auto Now = std::chrono::steady_clock::now();
	const float DeltaTime = std::min(std::chrono::duration<float>(Now - LastFrameTime).count(), 0.1f);
	LastFrameTime = Now;
	return Engine->Tick(DeltaTime, [this](float Dt) { GameMode->Tick(*Engine, Dt); });
}

void FGameApplication::Exit()
{
	if (GameMode && Engine)
	{
		GameMode->OnExit(*Engine);
	}
	GameMode.reset();
	if (Engine)
	{
		Engine->Shutdown();
		Engine.reset();
	}
}
