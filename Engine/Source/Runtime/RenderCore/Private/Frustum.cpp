#include "Frustum.h"

namespace
{

	/** Adds the plane A*x + B*y + C*z + D >= 0, normalized (N.x - W >= 0 inside). */
	void AddInsidePlane(FVectorPlaneSet& Planes, float InA, float InB, float InC, float InD)
	{
		const FVector Normal(InA, InB, InC);
		const float Len = Normal.Size();
		if (Len > 1e-8f)
		{
			const float Inv = 1.0f / Len;
			Planes.AddPlane(InA * Inv, InB * Inv, InC * Inv, InD * Inv);
			return;
		}
		Planes.AddPlane(0.0f, 1.0f, 0.0f, 0.0f);
	}

} // namespace

FBox TransformLocalBox(const FVector& LocalMin, const FVector& LocalMax, const FMatrix& Model)
{
	return FBox(LocalMin, LocalMax).TransformBy(Model);
}

void FFrustum::ExtractFromViewProjection(const FMatrix& ViewProjection)
{
	// Gribb / Hartmann: combine the clip-matrix columns into frustum planes. With row vectors, column C of M
	// (M[0..3][C]) gives clip coordinate C; the near and far planes are GL's -w <= z <= w.
	const float (&M)[4][4] = ViewProjection.M;
	Planes = FVectorPlaneSet();
	AddInsidePlane(Planes, M[0][3] + M[0][0], M[1][3] + M[1][0], M[2][3] + M[2][0], M[3][3] + M[3][0]); // left
	AddInsidePlane(Planes, M[0][3] - M[0][0], M[1][3] - M[1][0], M[2][3] - M[2][0], M[3][3] - M[3][0]); // right
	AddInsidePlane(Planes, M[0][3] + M[0][1], M[1][3] + M[1][1], M[2][3] + M[2][1], M[3][3] + M[3][1]); // bottom
	AddInsidePlane(Planes, M[0][3] - M[0][1], M[1][3] - M[1][1], M[2][3] - M[2][1], M[3][3] - M[3][1]); // top
	AddInsidePlane(Planes, M[0][3] + M[0][2], M[1][3] + M[1][2], M[2][3] + M[2][2], M[3][3] + M[3][2]); // near
	AddInsidePlane(Planes, M[0][3] - M[0][2], M[1][3] - M[1][2], M[2][3] - M[2][2], M[3][3] - M[3][2]); // far
}

bool FFrustum::IntersectsAabb(const FBox& Box) const
{
	// Outside when the centre's distance plus the box's reach along the normal is below 0 for some plane (the corner
	// furthest along it is outside).
	return !FVectorMath::IsBoxOutside(Planes, Box.GetCenter(), Box.GetExtent());
}

bool FFrustum::IntersectsSphere(const FVector& Center, float Radius) const
{
	return !FVectorMath::IsSphereOutside(Planes, Center, Radius);
}
