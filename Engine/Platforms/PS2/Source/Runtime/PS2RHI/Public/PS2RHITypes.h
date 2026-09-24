#pragma once

#include "CoreTypes.h"

class FPS2Texture;

/** Active view target (camera). Rotation in radians (Pitch / Yaw for math3d). */
struct FPS2ViewTarget
{
	float LocationX = 0.0f;
	float LocationY = 14.0f;
	float LocationZ = 58.0f;
	float Pitch = -0.24f;
	float Yaw = 0.0f;
};

/** Directional sun: aim via Yaw256 / Pitch256 (1/256 turn); Intensity scales LightColor. */
struct FPS2DirectionalLight
{
	float Intensity = 1.0f;
	float LightColorR = 1.00f;
	float LightColorG = 0.97f;
	float LightColorB = 0.90f;
	uint32 Yaw256 = 12;
	uint32 Pitch256 = 80; // high pitch = from above
};

enum class EMaterialShadingModel : uint8
{
	DefaultLit = 0,
	Unlit = 1,
};

/** PS2-lite material aligned to .lmat: BaseColor, BaseColorMap, ShadingModel. */
struct FPS2Material
{
	float BaseColorR = 1.0f;
	float BaseColorG = 1.0f;
	float BaseColorB = 1.0f;
	const FPS2Texture* BaseColorMap = nullptr;
	EMaterialShadingModel ShadingModel = EMaterialShadingModel::DefaultLit;

	/** When true and no BaseColorMap: multiply per-face RGB (debug cube). */
	bool UseFaceAlbedo = false;
};

/**
 * Per-frame Draw3D counters (PCSX2 console + HUD). Box -> frustum cull -> backface cull ->
 * per-triangle trivial reject / homogeneous clip (near + guard band) -> GS.
 */
struct FPS2Draw3DStats
{
	uint32 Boxes = 0;            // DrawBox calls
	uint32 CulledBoxes = 0;      // whole box outside the view frustum
	uint32 BackFaces = 0;        // faces facing away from the camera
	uint32 InTris = 0;           // triangles tested after box / face culling
	uint32 Keep3 = 0;            // fully inside the clip volume (sent as-is)
	uint32 Drop0 = 0;            // fully outside the visible frustum
	uint32 Clipped = 0;          // split against near / guard band
	uint32 Emitted = 0;          // triangles sent to the GS
	uint32 PacketQwordsPeak = 0;
};
