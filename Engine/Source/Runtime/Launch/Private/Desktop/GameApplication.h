#pragma once

#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "GameFramework/GameModeBase.h"

/**
 * Desktop game session driven by FEngineLoop: initialises UGameEngine (windowed or headless), loads the
 * startup level and runs ADefaultGameMode, one frame per Tick.
 */
class FGameApplication
{
public:
	/**
	 * Reads FCommandLine (-map=<.llev>, -nullrhi, -tick=<Hz>, -showstats, -AxesGizmo) and the Engine config,
	 * initialises the engine and enters the game mode. The map defaults to GameDefaultMap
	 * ([/Script/EngineSettings.GameMapsSettings] in the Engine config), then
	 * Engine/Content/LevelTemplates/Starter.llev.
	 */
	[[nodiscard]] bool Init();

	/** One frame (windowed) or one fixed step (headless). Returns false once the session is over. */
	[[nodiscard]] bool Tick();

	/** Leaves the game mode and shuts the engine down. */
	void Exit();

private:
	TUniquePtr<UGameEngine> Engine;
	TUniquePtr<AGameModeBase> GameMode;
	bool bHeadless = false;
	float TickHz = 60.0f;
	/** -Screenshot=<file.bmp> saves frame -ExitAfterFrames=N (default 60), then the game exits. */
	FString ScreenshotPath;
	int32 ExitAfterFrames = 0;
	int32 FrameCount = 0;
	/** FPlatformTime::Seconds of the previous frame / the next headless step. */
	double LastFrameTime = 0.0;
	double NextHeadlessTick = 0.0;
};
