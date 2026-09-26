#pragma once

#include "CoreMinimal.h"

class FGenericWindow;
class UGameViewportClient;

/**
 * The game window as a render target (UE: FViewport, UnrealClient.h): Draw makes the frame's canvas, has the viewport
 * client draw the scene and the HUD into it, flushes it, saves a requested screenshot and presents.
 *
 * Leon: a thin wrapper of the platform window (UE's FSceneViewport sits on a Slate window).
 */
class ENGINE_API FViewport
{
public:
	FViewport(UGameViewportClient* InViewportClient, FGenericWindow* InWindow);

	/** The size the renderer draws at, in pixels (UE: GetSizeXY, the render target's): the GS frame, 640 x 448. */
	[[nodiscard]] FIntPoint GetSizeXY() const;
	/** The window's framebuffer size in pixels, where the frame is shown. */
	[[nodiscard]] FIntPoint GetWindowSize() const;

	/** The window (Leon). */
	[[nodiscard]] FGenericWindow* GetWindow() const
	{
		return Window;
	}

	/** Draws a frame and, with bShouldPresent, shows it (UE: Draw). */
	void Draw(bool bShouldPresent = true);

private:
	UGameViewportClient* ViewportClient = nullptr;
	FGenericWindow* Window = nullptr;
};

/** A pending screenshot (UE: FScreenshotRequest): the viewport saves the next frame it draws. */
class ENGINE_API FScreenshotRequest
{
public:
	/** Asks for the next frame, saved to InFilename (Leon writes a 24-bit .bmp) (UE). */
	static void RequestScreenshot(const FString& InFilename, bool bInShowUI, bool bAddFilenameSuffix);
	[[nodiscard]] static bool IsScreenshotRequested();
	[[nodiscard]] static const FString& GetFilename();
	/** Forgets the request once it is served (UE). */
	static void Reset();
};
