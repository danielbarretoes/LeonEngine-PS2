#pragma once

#include "CoreMinimal.h"

/**
 * View matrices in UE's view space (UE: FViewMatrices::UpdateViewMatrix): x = right, y = up, z = forward. The world
 * (X forward, Y right, Z up) and the view space are both left-handed and depth grows along +Z. Row vectors:
 * ViewPoint = WorldPoint * ViewMatrix.
 */

/**
 * The view matrix whose rows give ViewPoint = (WorldPoint - Origin) . [Right, Up, Forward]. Forward, Right and Up must
 * be orthonormal; Right is the camera's screen-right side.
 */
[[nodiscard]] RENDERCORE_API FMatrix MakeViewMatrix(
	const FVector& Origin, const FVector& Forward, const FVector& Right, const FVector& Up);

/**
 * The view matrix of a camera at Origin with a view rotation (UE: FTranslationMatrix(-Origin) *
 * FInverseRotationMatrix(Rotation) * the (forward, right, up) -> (z, x, y) swizzle).
 */
[[nodiscard]] RENDERCORE_API FMatrix MakeViewMatrix(const FVector& Origin, const FRotator& Rotation);

/**
 * The view matrix of an eye looking at Target, with WorldUp kept up on screen. The world is left-handed, so the
 * screen-right side is WorldUp ^ Forward and the view up is Forward ^ Right.
 */
[[nodiscard]] RENDERCORE_API FMatrix MakeLookAtView(const FVector& Eye, const FVector& Target, const FVector& WorldUp);

/**
 * World-space mirror about the horizontal plane z = PlaneZ, applied before the view by the planar reflection pass (row
 * vectors: z' = 2 PlaneZ - z).
 */
[[nodiscard]] RENDERCORE_API FMatrix MakeReflectMatrix(float PlaneZ);

/**
 * Ortho light matrix tightly fitted to a world-space AABB of shadow casters: world to the light's GL clip space (UE
 * light view, UE ortho, then ToGLClipSpace), as the shadow pass and the lit shader's lookup use it.
 */
[[nodiscard]] RENDERCORE_API FMatrix FitLightSpaceMatrix(
	const FVector& LightDirection, const FVector& WorldMin, const FVector& WorldMax, float Padding = 50.0f);
