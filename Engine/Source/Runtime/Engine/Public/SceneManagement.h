#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

/**
 * How big a sphere looks from a view (UE: ComputeBoundsScreenSize, SceneManagement.h; Docs/PLANS/ps2-shipping.md N15):
 * its projected diameter over the view's height (the larger of the projection's two axes), 1 filling it. ProjMatrix is
 * the view's projection (UE's, its M[0][0] and M[1][1] the axes' scales); the distance is at least 1 cm.
 */
[[nodiscard]] ENGINE_API float ComputeBoundsScreenSize(
	const FVector& BoundsOrigin, float SphereRadius, const FVector& ViewOrigin, const FMatrix& ProjMatrix);

/**
 * The LOD of a static mesh a view draws (UE: ComputeStaticMeshLOD): the last LOD whose screen size
 * (UStaticMesh::GetLODScreenSize) is above the mesh's bounds' screen size (ComputeBoundsScreenSize), LOD 0 when none
 * is. DistanceScale scales the distance as UE's r.StaticMeshLODDistanceScale does (2 draws each LOD from half as far;
 * [/Script/Engine.RendererSettings] StaticMeshLODDistanceScale).
 */
[[nodiscard]] ENGINE_API int32 ComputeStaticMeshLOD(const UStaticMesh& Mesh, const FVector& BoundsOrigin,
	float SphereRadius, const FVector& ViewOrigin, const FMatrix& ProjMatrix, float DistanceScale = 1.0f);
