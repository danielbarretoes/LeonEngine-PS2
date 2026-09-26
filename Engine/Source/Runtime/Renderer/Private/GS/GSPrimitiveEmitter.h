#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"

/**
 * A vertex after the transform and the lighting, before clipping (the C++ reference of what the VU1 would do,
 * Docs/PLANS/ps2-gs-parity.md D2): clip space of UE's projection (x, y in [-w, w], z in [0, w] with 0 at the near
 * plane), the colour with 1.0 as full intensity (up to 2.0 on a textured draw, where MODULATE doubles), the alpha
 * (1.0 opaque) and the texture coordinates (a repeat of the texture per unit).
 */
struct FGSClipVertex
{
	FVector4 Clip = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	FLinearColor Color = FLinearColor::White;
	float U = 0.0f;
	float V = 0.0f;
};

/**
 * Turns triangles and lines in clip space into GS primitives of a list recorded against a drawing environment
 * (FGSDrawEnvironment): the GS has no clipper, so a triangle is clipped against the near and far planes and a guard
 * band (the GS's 4096 x 4096 primitive space around the frame) and trivially rejected when it is wholly outside the
 * view; the scissor trims the rest. Back faces are culled when asked (front faces wind counter-clockwise with Y up,
 * as in OpenGL). Pixel centers match OpenGL's (the GS samples at integer coordinates, OpenGL at half-integers).
 *
 * Colours become RGBAQ bytes: an untextured draw writes 1.0 as 0xff, a textured one as 0x80 (MODULATE's 1.0).
 * Z is 1 - depth over the PSMZ24 range, so a nearer fragment has the larger Z (the environment's GEQUAL).
 */
class FGSPrimitiveEmitter
{
public:
	FGSPrimitiveEmitter(const FGSDrawEnvironment& InEnvironment, FGSCommandList& InList);

	/**
	 * Starts a run of triangles (PRIM of type Triangle, Gouraud, with bTextured's STQ and bBlend's ABE); the texture's
	 * TEX0 and the blending are the caller's.
	 */
	void BeginTriangles(bool bTextured, bool bBlend, bool bCullBackFaces);
	void AddTriangle(const FGSClipVertex& A, const FGSClipVertex& B, const FGSClipVertex& C);

	/** Starts a run of untextured lines (the end point of each is not drawn, as on the GS). */
	void BeginLines(bool bBlend);
	void AddLine(const FGSClipVertex& A, const FGSClipVertex& B);

	/** Z added to every vertex, clamped (decals over the surface they lie on; UE: polygon offset). */
	void SetDepthBias(uint32 InDepthBias)
	{
		DepthBias = InDepthBias;
	}

	/** Triangles sent to the GS, and triangles dropped (outside the view, back facing or clipped away). */
	[[nodiscard]] int32 GetNumTriangles() const
	{
		return NumTriangles;
	}
	[[nodiscard]] int32 GetNumRejected() const
	{
		return NumRejected;
	}

	/** The guard band, in multiples of w: a primitive coordinate stays within the GS's 0..4095 inside it. */
	[[nodiscard]] float GetGuardX() const
	{
		return GuardX;
	}
	[[nodiscard]] float GetGuardY() const
	{
		return GuardY;
	}

private:
	void Emit(const FGSClipVertex& Vertex);

	const FGSDrawEnvironment& Environment;
	FGSCommandList& List;
	float GuardX = 1.0f;
	float GuardY = 1.0f;
	/** 255 untextured, 128 textured (MODULATE). */
	float ColorScale = 255.0f;
	bool bTextured = false;
	bool bCullBackFaces = false;
	uint32 DepthBias = 0;
	/** The last RGBAQ written, to skip repeating it. */
	FGSRGBAQ LastColor;
	bool bHasLastColor = false;
	int32 NumTriangles = 0;
	int32 NumRejected = 0;
};
