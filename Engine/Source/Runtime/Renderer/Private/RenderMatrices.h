#pragma once

#include "CoreMinimal.h"

/**
 * The normal matrix of a model matrix for GLSL's `uNormalMatrix * n` (a mat3 uploaded untransposed with
 * glUniformMatrix3fv): the inverse transpose of the model's 3x3 part, 9 floats in mat3 memory order. With row
 * vectors a normal goes to n * Inverse(Model)^T; GLSL's column-vector mat3 built from these floats applies that same
 * transform. A singular model gives the identity (FMatrix::Inverse).
 */
void GetNormalMatrix3x3(const FMatrix& Model, float Out[9]);

/**
 * Pixel space to GL clip space for the 2D overlay: (0, 0) is the top-left pixel corner, y grows down, and
 * (Width, Height) is the bottom-right corner. Z passes through unchanged.
 */
[[nodiscard]] FMatrix MakePixelSpaceProjection(float Width, float Height);
