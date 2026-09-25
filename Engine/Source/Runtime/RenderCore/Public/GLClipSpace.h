#pragma once

#include "CoreMinimal.h"

/**
 * UE clip space to OpenGL clip space for the GL renderer.
 *
 * UE's projections (FPerspectiveMatrix, FOrthoMatrix) give depth z / w in [0, 1], 0 at the near plane. OpenGL clips
 * z / w to [-1, 1]. The adapter keeps x, y and w and writes z_gl = 2 z - w, so -1 is the near plane and 1 the far one.
 * Everything that reads GL clip space (frustum planes, shadow lookup, SSAO depth, debug frustum) uses the result.
 */
[[nodiscard]] RENDERCORE_API FMatrix ToGLClipSpace(const FMatrix& Projection);
