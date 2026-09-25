#pragma once

#include "CoreMinimal.h"

class IEngineLoop;

/**
 * Desktop game session driven by FEngineLoop: creates GEngine from `[/Script/Engine.Engine] GameEngine=`, initialises
 * and starts it (the game instance opens the first map through UEngine::LoadMap) and runs one frame per Tick.
 */
class FGameApplication
{
public:
	/**
	 * Reads FCommandLine (-nullrhi, -tick=<Hz>, -Screenshot=, -ExitAfterFrames=) and creates, initialises and starts
	 * the engine. The map comes from the game instance (the first command-line token, -map=, else GameDefaultMap).
	 */
	[[nodiscard]] bool Init(IEngineLoop* EngineLoop);

	/** One frame (windowed) or one fixed step (headless). Returns false once the session is over. */
	[[nodiscard]] bool Tick();

	/** Leaves the game mode and shuts the engine down. */
	void Exit();

private:
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
