#pragma once

#include "Engine/GameEngine.h"
#include "GameFramework/GameplayRouter.h"
#include "GameHostSession.h"

#include <chrono>
#include <functional>
#include <memory>

/// Desktop game session driven by FEngineLoop: initialises UGameEngine (windowed or dedicated),
/// starts the project pack session and advances one frame per Tick.
class FGameApplication
{
public:
	using FRegisterModesFunction = std::function<void(UGameEngine&, FGameplayRouter&)>;

	/// Parses the command line (--dedicated/--server, --listen/--host, --join, --map, --port,
	/// --tick-hz, --show-stats), initialises the engine and starts the pack. False on failure.
	/// bDedicatedByDefault starts headless without CLI flags (server executables).
	[[nodiscard]] bool Init(int Argc, char** Argv, const char* PackName, const FRegisterModesFunction& RegisterModes,
		bool bDedicatedByDefault = false);

	/// One frame (windowed) or one fixed step (dedicated). Returns false once the session is over.
	[[nodiscard]] bool Tick();

	/// Stops the session and shuts the engine down.
	void Exit();

	/// Init + Tick until finished + Exit; returns a process exit code.
	[[nodiscard]] int Run(int Argc, char** Argv, const char* PackName, const FRegisterModesFunction& RegisterModes,
		bool bDedicatedByDefault = false);

private:
	std::unique_ptr<UGameEngine> Engine;
	FGameHostSession Session;
	bool bDedicated = false;
	bool bStarted = false;
	float TickHz = 60.0f;
	std::chrono::steady_clock::time_point LastFrameTime;
	std::chrono::steady_clock::time_point NextHeadlessTick;
};
