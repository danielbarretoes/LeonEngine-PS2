#include "GLClipSpace.h"

FMatrix ToGLClipSpace(const FMatrix& Projection)
{
	// Row vectors: (x, y, z, w) * Adapter = (x, y, 2 z - w, w).
	const FMatrix Adapter(FPlane(1.0f, 0.0f, 0.0f, 0.0f), FPlane(0.0f, 1.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 2.0f, 0.0f), FPlane(0.0f, 0.0f, -1.0f, 1.0f));
	return Projection * Adapter;
}
