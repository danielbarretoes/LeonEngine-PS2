#pragma once

#include "CoreMinimal.h"

class FLPS2Mesh;

/**
 * The sky's box (Docs/PLANS/ps2-polish.md P8): a cube of half size 1 around the eye as LPS2 v2 render data, which the
 * GS scene renderer draws scaled and centred on the eye like any static mesh's batches, VU1 drawing them on the PS2.
 * One section a face, in ECubeFace's order (material slot = face), each textured with that face of the cube map
 * (UTextureCube::GetFaceDirection: U along the face's Right, V along its Up) and wound to face the centre.
 *
 * A face is QuadsPerSide x QuadsPerSide quads, QuadsPerBatch x QuadsPerBatch to a batch: two strips of 6 vertices.
 * The batches are small so that the sphere of one the view sees stays inside the guard band and past the near plane
 * (plan D8): a batch spans at most 14 degrees from the centre, and one that reaches the view's side is inside the guard
 * band for any vertical field of view up to 90 degrees, so no batch of the sky goes through the EE's clipper.
 */
struct FSkyBoxGeometry
{
	static constexpr int32 QuadsPerSide = 12;
	static constexpr int32 QuadsPerBatch = 2;
	static constexpr int32 BatchesPerFace = (QuadsPerSide / QuadsPerBatch) * (QuadsPerSide / QuadsPerBatch);
	static constexpr int32 NumFaces = 6;

	/** Builds the box's render data into OutMesh (the same bytes every time); false if the blob is not valid. */
	static bool BuildMesh(FLPS2Mesh& OutMesh);

	/**
	 * The box's scale for a projection (UE's row vector FPerspectiveMatrix, clip z from 0 at the near plane to w at the
	 * far one): the geometric mean of the near and far distances (100 times the near one without a far plane), so
	 * its batches are far past the near plane and its corners (sqrt(3) times it) well inside the far one.
	 */
	[[nodiscard]] static float GetRadius(const FMatrix& Projection);
};
