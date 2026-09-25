#include "Frustum.h"

namespace
{

	/** Plane A*x + B*y + C*z + D >= 0 as a normalized FPlane (N.x - W >= 0 inside). */
	FPlane MakeInsidePlane(float InA, float InB, float InC, float InD)
	{
		const FVector Normal(InA, InB, InC);
		const float Len = Normal.Size();
		if (Len > 1e-8f)
		{
			const float Inv = 1.0f / Len;
			return FPlane(Normal * Inv, -InD * Inv);
		}
		return FPlane(0.0f, 1.0f, 0.0f, 0.0f);
	}

} // namespace

FBox TransformLocalBox(const FVector& LocalMin, const FVector& LocalMax, const FMatrix& Model)
{
	return FBox(LocalMin, LocalMax).TransformBy(Model);
}

void FFrustum::ExtractFromViewProjection(const FMatrix& ViewProjection)
{
	// Gribb / Hartmann: combine the clip-matrix columns into frustum planes. M[C][R] is column C, row R of the
	// column-vector clip transform (the GL memory layout; see LegacyGLMath.h).
	const float (&M)[4][4] = ViewProjection.M;
	Planes[0] = MakeInsidePlane(M[0][3] + M[0][0], M[1][3] + M[1][0], M[2][3] + M[2][0], M[3][3] + M[3][0]); // left
	Planes[1] = MakeInsidePlane(M[0][3] - M[0][0], M[1][3] - M[1][0], M[2][3] - M[2][0], M[3][3] - M[3][0]); // right
	Planes[2] = MakeInsidePlane(M[0][3] + M[0][1], M[1][3] + M[1][1], M[2][3] + M[2][1], M[3][3] + M[3][1]); // bottom
	Planes[3] = MakeInsidePlane(M[0][3] - M[0][1], M[1][3] - M[1][1], M[2][3] - M[2][1], M[3][3] - M[3][1]); // top
	Planes[4] = MakeInsidePlane(M[0][3] + M[0][2], M[1][3] + M[1][2], M[2][3] + M[2][2], M[3][3] + M[3][2]); // near
	Planes[5] = MakeInsidePlane(M[0][3] - M[0][2], M[1][3] - M[1][2], M[2][3] - M[2][2], M[3][3] - M[3][2]); // far
}

bool FFrustum::IntersectsAabb(const FBox& Box) const
{
	for (const FPlane& Plane : Planes)
	{
		// The box corner furthest along the plane normal.
		const FVector Positive(Plane.X >= 0.0f ? Box.Max.X : Box.Min.X, Plane.Y >= 0.0f ? Box.Max.Y : Box.Min.Y,
			Plane.Z >= 0.0f ? Box.Max.Z : Box.Min.Z);
		if (Plane.PlaneDot(Positive) < 0.0f)
		{
			return false;
		}
	}
	return true;
}
