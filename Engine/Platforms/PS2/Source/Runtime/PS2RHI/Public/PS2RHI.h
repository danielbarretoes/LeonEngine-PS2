#pragma once

#include "CoreTypes.h"
#include "GSCommandList.h"
#include "GSTypes.h"
#include "PS2RHITypes.h"
#include "PS2Texture.h"

/**
 * PlayStation 2 Graphics Synthesizer API (platform extension RHI). Every draw appends to the frame's FGSCommandList,
 * which WaitVSync sends to the GIF as one packet (Docs/PLANS/ps2-gs-parity.md, P3).
 *
 * Frame flow:
 *   1. InitDisplay: VRAM, CRTC and the drawing environment (done by FPS2Window)
 *   2. SetViewTarget / SetDirectionalLight / SetAmbientLightColor
 *   3. ClearColor -> BindMaterial -> DrawBox... -> DrawDebugText, or Submit for a recorded list
 *   4. WaitVSync: send the frame, wait for the vertical blank and show it (FPS2Window::SwapBuffers)
 */
class PS2RHI_API FPS2RHI
{
public:
	/**
	 * Width x Height, double buffered, with a Z24 buffer. ColorFormat is PSMCT16S (dithered, the engine's) or PSMCT32;
	 * ReservedVramBytes at the start of the VRAM are left to the caller.
	 */
	static bool InitDisplay(
		int Width, int Height, EGSPixelFormat ColorFormat = EGSPixelFormat::PSMCT16S, uint32 ReservedVramBytes = 0);
	static void ClearColor(float R, float G, float B);
	/** Sends the frame, waits for the vertical blank and shows what was drawn. */
	static void WaitVSync();

	/** Appends a recorded list to the frame; the drawing environment is restored after it. */
	static void Submit(const FGSCommandList& List);
	/** A vertex at screen coordinates (pixels, origin at the screen centre) for a list drawn in that environment. */
	static FGSXYZ ScreenVertex(float X, float Y, uint32 Z = 0);

	// --- View / lights ---
	static void SetViewTarget(const FPS2ViewTarget& ViewTarget);
	static void SetDirectionalLight(const FPS2DirectionalLight& Light);
	static void SetAmbientLightColor(float R, float G, float B);

	// --- Material ---
	static void BindMaterial(const FPS2Material& Material);

	// --- 2D overlays (screen space, origin at the screen centre, no depth test) ---

	/** Alpha-blended overlay rect: color = (src - dst) * alpha + dst, alpha in [0, 1]. */
	static bool DrawUnlitRectAlpha(float X0, float Y0, float X1, float Y1, float R, float G, float B, float Alpha);

	/**
	 * 5x7 debug glyphs drawn with rects (ASCII A-Z, 0-9, a few symbols).
	 * scale 1 = 2 px cells (12 px advance, 14 px tall); 0.5 = 1 px cells (6 px advance, 7 px tall).
	 */
	static void DrawDebugText(
		float X, float Y, const char* Text, float R = 0.95f, float G = 0.95f, float B = 0.85f, float Scale = 1.0f);

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
