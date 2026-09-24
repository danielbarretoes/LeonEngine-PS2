#pragma once

#include "CoreTypes.h"
#include "PS2RHITypes.h"
#include "PS2Texture.h"

/**
 * PlayStation 2 Graphics Synthesizer immediate-mode API (platform extension RHI).
 *
 * Frame flow:
 *   1. InitDisplay — VRAM + CRTC + z-buffer + draw environment (done by FPS2Window)
 *   2. SetViewTarget / SetDirectionalLight / SetAmbientLightColor
 *   3. ClearColor -> BindMaterial -> DrawBox... -> DrawDebugText
 *   4. WaitVSync — present (FPS2Window::SwapBuffers)
 */
class PS2RHI_API FPS2RHI
{
public:
	static bool InitDisplay(int width, int height);
	static void ClearColor(float r, float g, float b);
	static void WaitVSync();

	// --- View / lights ---
	static void SetViewTarget(const FPS2ViewTarget& viewTarget);
	static void SetDirectionalLight(const FPS2DirectionalLight& light);
	static void SetAmbientLightColor(float r, float g, float b);

	// --- Material ---
	static void BindMaterial(const FPS2Material& material);

	// --- 2D unlit (screen space, origin at the screen centre) ---
	static bool DrawUnlitTriangle();
	static bool DrawUnlitTriangleAt(float centerX, float centerY, float size, unsigned angle256, float r, float g,
		float b);
	static bool DrawUnlitRect(float x0, float y0, float x1, float y1, float r, float g, float b);

	/** Alpha-blended overlay rect: color = (src - dst) * alpha + dst, alpha in [0, 1]. */
	static bool DrawUnlitRectAlpha(float x0, float y0, float x1, float y1, float r, float g, float b, float alpha);

	/**
	 * 5x7 debug glyphs drawn with rects (ASCII A-Z, 0-9, a few symbols).
	 * scale 1 = 2 px cells (12 px advance, 14 px tall); 0.5 = 1 px cells (6 px advance, 7 px tall).
	 */
	static void DrawDebugText(float x, float y, const char* text, float r = 0.95f, float g = 0.95f, float b = 0.85f,
		float scale = 1.0f);

	/** Validates and draws a cooked LPS2 blob (see Docs/ASSET_FORMATS.md, PS2). */
	static bool DrawCookedMesh(const void* data, unsigned size);

	/** Lit / textured box: Location, rotation (1/256 turn), Scale as half-extents. */
	static bool DrawBox(float locationX, float locationY, float locationZ, unsigned yaw256, unsigned pitch256,
		float scaleX, float scaleY, float scaleZ);

	/** Uniform half-extent convenience. */
	static bool DrawBox(float locationX, float locationY, float locationZ, unsigned yaw256, unsigned pitch256,
		float scale)
	{
		return DrawBox(locationX, locationY, locationZ, yaw256, pitch256, scale, scale, scale);
	}

	// --- Draw3D counters (reset every frame by the engine loop) ---
	static void BeginDraw3DStatsFrame();
	static void GetDraw3DStats(FPS2Draw3DStats& out);

	/** printf snapshot (PCSX2 EE console). */
	static void PrintDraw3DStats(const FPS2Draw3DStats& stats);
};
