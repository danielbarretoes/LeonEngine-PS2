#pragma once

#include "CoreMinimal.h"

/**
 * View matrices in UE's view space (UE: FViewMatrices::UpdateViewMatrix): x = right, y = up, z = forward. The space is
 * left-handed and depth grows along +Z. Row vectors: ViewPoint = WorldPoint * ViewMatrix.
 *
 * UE builds the matrix from an FRotator (FTranslationMatrix(-Origin) * FInverseRotationMatrix(Rotation) * the
 * (forward, right, up) -> (z, x, y) swizzle). Until the world turns Z-up, the camera passes its basis vectors instead.
 */

/**
 * The view matrix whose rows give ViewPoint = (WorldPoint - Origin) . [Right, Up, Forward]. Forward, Right and Up must
 * be orthonormal; Right is the camera's screen-right side.
 */
[[nodiscard]] RENDERCORE_API FMatrix MakeViewMatrix(
	const FVector& Origin, const FVector& Forward, const FVector& Right, const FVector& Up);

/**
 * The view matrix of an eye looking at Target, with WorldUp kept up on screen. The world is still right-handed Y-up,
 * so the screen-right side is Forward ^ WorldUp.
 */
[[nodiscard]] RENDERCORE_API FMatrix MakeLookAtView(const FVector& Eye, const FVector& Target, const FVector& WorldUp);
