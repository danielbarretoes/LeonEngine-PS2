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
	static bool InitDisplay(int Width, int Height);
	static void ClearColor(float R, float G, float B);
	static void WaitVSync();

	// --- View / lights ---
	static void SetViewTarget(const FPS2ViewTarget& ViewTarget);
	static void SetDirectionalLight(const FPS2DirectionalLight& Light);
	static void SetAmbientLightColor(float R, float G, float B);

	// --- Material ---
	static void BindMaterial(const FPS2Material& Material);

	// --- 2D unlit (screen space, origin at the screen centre) ---
	static bool DrawUnlitTriangle();
	static bool DrawUnlitTriangleAt(
		float CenterX, float CenterY, float Size, unsigned Angle256, float R, float G, float B);
	static bool DrawUnlitRect(float X0, float Y0, float X1, float Y1, float R, float G, float B);

	/** Alpha-blended overlay rect: color = (src - dst) * alpha + dst, alpha in [0, 1]. */
	static bool DrawUnlitRectAlpha(float X0, float Y0, float X1, float Y1, float R, float G, float B, float Alpha);

	/**
	 * 5x7 debug glyphs drawn with rects (ASCII A-Z, 0-9, a few symbols).
	 * scale 1 = 2 px cells (12 px advance, 14 px tall); 0.5 = 1 px cells (6 px advance, 7 px tall).
	 */
	static void DrawDebugText(
		float X, float Y, const char* Text, float R = 0.95f, float G = 0.95f, float B = 0.85f, float Scale = 1.0f);

	/** Validates and draws a cooked LPS2 blob (see Docs/ASSET_FORMATS.md, PS2). */
	static bool DrawCookedMesh(const void* Data, unsigned Size);

	/** Lit / textured box: Location, rotation (1/256 turn), Scale as half-extents. */
	static bool DrawBox(float LocationX, float LocationY, float LocationZ, unsigned Yaw256, unsigned Pitch256,
		float ScaleX, float ScaleY, float ScaleZ);

	/** Uniform half-extent convenience. */
	static bool DrawBox(
		float LocationX, float LocationY, float LocationZ, unsigned Yaw256, unsigned Pitch256, float Scale)
	{
		return DrawBox(LocationX, LocationY, LocationZ, Yaw256, Pitch256, Scale, Scale, Scale);
	}

	// --- Draw3D counters (reset every frame by the engine loop) ---
	static void BeginDraw3DStatsFrame();
	static void GetDraw3DStats(FPS2Draw3DStats& Out);

	/** printf snapshot (PCSX2 EE console). */
	static void PrintDraw3DStats(const FPS2Draw3DStats& Stats);
};
