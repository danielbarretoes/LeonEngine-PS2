#pragma once

#include "Engine/GameEngine.h"
#include "GameFramework/GameModeBase.h"

#include <chrono>
#include <memory>

/// Desktop game session driven by FEngineLoop: initialises UGameEngine (windowed or headless), loads the
/// startup level and runs ADefaultGameMode, one frame per Tick.
class FGameApplication
{
public:
	/// Reads FCommandLine (`-map=<.llev>`, `-nullrhi`, `-tick=<Hz>`, `-showstats`) and the Engine config, initialises
	/// the engine and enters the game mode. The map defaults to GameDefaultMap
	/// ([/Script/EngineSettings.GameMapsSettings] in the Engine config), then
	/// Engine/Content/LevelTemplates/Starter.llev.
	[[nodiscard]] bool Init();

	/// One frame (windowed) or one fixed step (headless). Returns false once the session is over.
	[[nodiscard]] bool Tick();

	/// Leaves the game mode and shuts the engine down.
	void Exit();

private:
	std::unique_ptr<UGameEngine> Engine;
	std::unique_ptr<AGameModeBase> GameMode;
	bool bHeadless = false;
	float TickHz = 60.0f;
	std::chrono::steady_clock::time_point LastFrameTime;
	std::chrono::steady_clock::time_point NextHeadlessTick;
};
