#include "Math/Sphere.h"

#include "Math/Box.h"
#include "Math/Matrix.h"
#include "Math/Transform.h"

FSphere::FSphere(const FVector* Pts, int32 Count)
	: Center(0, 0, 0)
	, W(0)
{
	if (Count > 0)
	{
		const FBox Box(Pts, Count);

		*this = FSphere((Box.Min + Box.Max) / 2, 0);

		for (int32 Index = 0; Index < Count; Index++)
		{
			const float Dist = FVector::DistSquared(Pts[Index], Center);

			if (Dist > W)
			{
				W = Dist;
			}
		}

		W = FMath::Sqrt(W) * 1.001f;
	}
}

FSphere FSphere::TransformBy(const FMatrix& M) const
{
	FSphere Result;

	Result.Center = FVector(M.TransformPosition(Center));

	const FVector XAxis(M.M[0][0], M.M[0][1], M.M[0][2]);
	const FVector YAxis(M.M[1][0], M.M[1][1], M.M[1][2]);
	const FVector ZAxis(M.M[2][0], M.M[2][1], M.M[2][2]);

	Result.W = FMath::Sqrt(FMath::Max(XAxis | XAxis, FMath::Max(YAxis | YAxis, ZAxis | ZAxis))) * W;

	return Result;
}

FSphere FSphere::TransformBy(const FTransform& M) const
{
	FSphere Result;

	Result.Center = M.TransformPosition(Center);
	Result.W = M.GetMaximumAxisScale() * W;

	return Result;
}

float FSphere::GetVolume() const
{
	return (4.f / 3.f) * PI * (W * W * W);
}

FSphere& FSphere::operator+=(const FSphere& Other)
{
	if (W == 0.f)
	{
		*this = Other;
		return *this;
	}

	const FVector ToOther = Other.Center - Center;
	const float DistSqr = ToOther.SizeSquared();

	if (FMath::Square(W - Other.W) + KINDA_SMALL_NUMBER >= DistSqr)
	{
		// One sphere contains the other: keep the larger.
		if (W < Other.W)
		{
			*this = Other;
		}
	}
	else
	{
		const float Dist = FMath::Sqrt(DistSqr);

		FSphere NewSphere;
		NewSphere.W = (Dist + Other.W + W) * 0.5f;
		NewSphere.Center = Center;

		if (Dist > SMALL_NUMBER)
		{
			NewSphere.Center += ToOther * ((NewSphere.W - W) / Dist);
		}

		*this = NewSphere;
	}

	return *this;
}
