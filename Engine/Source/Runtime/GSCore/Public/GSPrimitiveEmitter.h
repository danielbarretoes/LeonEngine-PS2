#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSVertexBatch.h"
#include "Misc/MemStack.h"
#include "Misc/Scratchpad.h"

/**
 * A vertex after the transform and the lighting, before clipping (the C++ reference of what VU1 does,
 * Docs/PLANS/ps2-shipping.md D2): clip space of UE's projection (x, y in [-w, w], z in [0, w] with 0 at the near
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

/** The triangle a strip vertex closes with the two before it (LPS2 v2's strip flags). */
enum class EGSStripTriangle : uint8
{
	/** None: a strip's first two vertices, or a degenerate triangle (LPS2 v2's no kick, the GS's ADC). */
	None,
	/** The triangle (i - 2, i - 1, i). */
	Forward,
	/** The triangle (i - 1, i - 2, i): an odd triangle of its strip, wound the other way. */
	Reversed,
};

/**
 * Turns triangles, triangle strips and lines in clip space into GS primitives of a list recorded against a drawing
 * environment (FGSDrawEnvironment): the GS has no clipper, so a triangle is clipped against the near and far planes
 * and a guard band (the GS's 4096 x 4096 primitive space around the frame) and trivially rejected when it is wholly
 * outside the view; the scissor trims the rest. A strip is never clipped: its caller sends only vertices inside the
 * guard band and between the near and far planes (a LPS2 v2 batch whose sphere is inside them, D8); a triangle of it
 * that is outside the view or back facing is not drawn, and only the vertices of the drawn ones are sent. Back faces
 * are culled when asked (front faces wind counter-clockwise with Y up, as in OpenGL). Pixel centers match OpenGL's
 * (the GS samples at integer coordinates, OpenGL at half-integers).
 *
 * Colours become RGBAQ bytes: an untextured draw writes 1.0 as 0xff, a textured one as 0x80 (MODULATE's 1.0).
 * Z is 1 - depth over the PSMZ24 range, so a nearer fragment has the larger Z (the environment's GEQUAL).
 */
class GSCORE_API FGSPrimitiveEmitter
{
public:
	FGSPrimitiveEmitter(const FGSDrawEnvironment& InEnvironment, FGSCommandList& InList);

	/**
	 * Starts a run of triangles (PRIM of type Triangle, Gouraud, with bTextured's STQ and bBlend's ABE); the texture's
	 * TEX0 and the blending are the caller's.
	 */
	void BeginTriangles(bool bTextured, bool bBlend, bool bCullBackFaces);
	void AddTriangle(const FGSClipVertex& A, const FGSClipVertex& B, const FGSClipVertex& C);

	/**
	 * Starts a triangle strip (PRIM of type TriangleStrip, Gouraud, with bTextured's STQ and bBlend's ABE). Its
	 * vertices, which AddStrips takes, must need no clipping.
	 */
	void BeginStrip(bool bTextured, bool bBlend, bool bCullBackFaces);
	/**
	 * Sends strips: Vertices, one after the other, each closing the triangle Triangles says (the same count; a strip
	 * starts with two vertices that close none). A triangle wholly outside the view or back facing is not drawn, and
	 * only the vertices a drawn triangle uses are sent (VU1 is to do the same, N14): a vertex that closes a drawn
	 * triangle as XYZ2, the others it needs as XYZ3 (they only move the GS's vertex queue on).
	 */
	void AddStrips(TArrayView<const FGSClipVertex> Vertices, TArrayView<const EGSStripTriangle> Triangles);

	/**
	 * The vertices of a vertex batch transformed to clip space, lit and coloured as its draw says, and the triangle
	 * each closes (its strip flags), into Batch.NumVertices of each: what VU1's microprograms compute
	 * (Docs/PLANS/ps2-shipping.md N14, D2), in C++. A skinned batch's vertices are posed first with their two palette
	 * bones (N14b: the Skinned programs' reference).
	 */
	static void TransformVertexBatch(const FGSVertexDraw& Draw, const FGSVertexBatch& Batch, FGSClipVertex* OutVertices,
		EGSStripTriangle* OutTriangles);
	/**
	 * Draws a vertex batch as FGSCommandList::DrawVertexBatch describes it (the reference of VU1's GIF packet): its
	 * vertices through TransformVertexBatch, then AddStrips. The strip must have been started (BeginStrip, with the
	 * draw's bTextured and bBlend).
	 */
	void AddVertexBatch(const FGSVertexDraw& Draw, const FGSVertexBatch& Batch);

	/** Starts a run of untextured lines (the end point of each is not drawn, as on the GS). */
	void BeginLines(bool bBlend);
	void AddLine(const FGSClipVertex& A, const FGSClipVertex& B);

	/**
	 * The fog of the triangles and strips begun from now on (N15): with it on they are drawn with FGE and each vertex
	 * is an XYZF2 with its F from its clip w (FGSVertexFog::GetF); off, XYZ2. AddVertexBatch expects the draw's.
	 */
	void SetFog(const FGSVertexFog& InFog)
	{
		Fog = InFog;
	}
	[[nodiscard]] const FGSVertexFog& GetFog() const
	{
		return Fog;
	}

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
	/** Writes the vertex's RGBAQ (when it changed), ST and XYZ2, or XYZ3 without bKick. */
	void Emit(const FGSClipVertex& Vertex, bool bKick = true);
	/** Starts a run of PRIM Type (TriangleStrip or Triangle). */
	void BeginPrimitives(EGSPrimitive Type, bool bInTextured, bool bBlend, bool bInCullBackFaces);

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
	/**
	 * A strip batch's scratch, on the scratchpad (N15; the emitter lives inside the scene renderer's FScratchpadMark
	 * and FMemMark, the frame's stack taking what does not fit).
	 */
	TArray<uint32, TScratchpadAllocator<>> StripOutcodes;
	TArray<bool, TScratchpadAllocator<>> StripDrawn;
	/** AddVertexBatch's scratch, on the scratchpad too: the batch's vertices and the triangle each closes. */
	TArray<FGSClipVertex, TScratchpadAllocator<>> BatchVertices;
	TArray<EGSStripTriangle, TScratchpadAllocator<>> BatchTriangles;
	/** The fog of the primitives begun from now on (SetFog), and whether the current run is fogged (XYZF2). */
	FGSVertexFog Fog;
	bool bFogging = false;
	int32 NumTriangles = 0;
	int32 NumRejected = 0;
};
