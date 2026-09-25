#include "GameApplication.h"

#include "GameFramework/DefaultGameMode.h"
#include "Level/LevelLoader.h"
#include "Migration/LegacyContentPath.h"
#include "RuntimeInput.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace
{

	constexpr const char* DefaultMap = "LevelTemplates/Starter.llev";

	[[nodiscard]] bool HasFlag(int Argc, char** Argv, const char* Flag)
	{
		for (int I = 1; I < Argc; ++I)
		{
			if (Argv[I] != nullptr && std::strcmp(Argv[I], Flag) == 0)
			{
				return true;
			}
		}
		return false;
	}

	/// Value of `-Key=Value` (UE command-line style), or empty.
	[[nodiscard]] std::string ParseValue(int Argc, char** Argv, const char* Key)
	{
		const std::size_t KeyLength = std::strlen(Key);
		for (int I = 1; I < Argc; ++I)
		{
			if (Argv[I] != nullptr && std::strncmp(Argv[I], Key, KeyLength) == 0)
			{
				return Argv[I] + KeyLength;
			}
		}
		return {};
	}

	[[nodiscard]] float ParseTickHz(int Argc, char** Argv, float Fallback)
	{
		for (int I = 1; I + 1 < Argc; ++I)
		{
			if (Argv[I] != nullptr && std::strcmp(Argv[I], "--tick") == 0 && Argv[I + 1] != nullptr)
			{
				const float Hz = std::strtof(Argv[I + 1], nullptr);
				if (Hz >= 1.0f && Hz <= 240.0f)
				{
					return Hz;
				}
			}
		}
		return Fallback;
	}

	/// A path as given (absolute or relative to the working directory), else relative to Engine/Content.
	[[nodiscard]] std::string ResolveMapPath(const std::string& Map)
	{
		std::error_code Ec;
		if (std::filesystem::is_regular_file(Map, Ec) && !Ec)
		{
			return std::filesystem::path(Map).lexically_normal().string();
		}
		return ResolveLegacyContentPath(Map);
	}

} // namespace

bool FGameApplication::Init(int Argc, char** Argv, const char* ProjectName)
{
	// Console strings stay ASCII: Windows cmd often is not UTF-8 (em dash / arrows mojibake).
	bHeadless = HasFlag(Argc, Argv, "-nullrhi");
	const bool bShowStats = HasFlag(Argc, Argv, "--show-stats");
	TickHz = ParseTickHz(Argc, Argv, 60.0f);
	std::string Map = ParseValue(Argc, Argv, "-map=");
	if (Map.empty())
	{
		Map = DefaultMap;
	}
	const std::string MapPath = ResolveMapPath(Map);
	const bool bHasProjectName = ProjectName != nullptr && std::strlen(ProjectName) > 0;
	const std::string Title = bHasProjectName ? std::string("Leon - ") + ProjectName : std::string("Leon");

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
	else if (!Engine->Initialize(1280, 720, Title.c_str()))
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
		std::cerr << "Failed to load map '" << Map << "'\n";
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
