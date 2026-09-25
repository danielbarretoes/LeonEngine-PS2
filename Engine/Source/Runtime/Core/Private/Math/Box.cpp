#include "Math/Box.h"

#include "Math/Matrix.h"
#include "Math/Transform.h"
#include "Templates/UnrealTemplate.h"

FBox::FBox(const FVector* Points, int32 Count)
	: Min(0, 0, 0)
	, Max(0, 0, 0)
	, IsValid(0)
{
	for (int32 Index = 0; Index < Count; Index++)
	{
		*this += Points[Index];
	}
}

FBox& FBox::operator+=(const FBox& Other)
{
	if (IsValid && Other.IsValid)
	{
		Min.X = FMath::Min(Min.X, Other.Min.X);
		Min.Y = FMath::Min(Min.Y, Other.Min.Y);
		Min.Z = FMath::Min(Min.Z, Other.Min.Z);

		Max.X = FMath::Max(Max.X, Other.Max.X);
		Max.Y = FMath::Max(Max.Y, Other.Max.Y);
		Max.Z = FMath::Max(Max.Z, Other.Max.Z);
	}
	else if (Other.IsValid)
	{
		*this = Other;
	}

	return *this;
}

FVector FBox::GetClosestPointTo(const FVector& Point) const
{
	// Start by considering the point inside the box.
	FVector ClosestPoint = Point;

	// Now clamp to inside box if it's outside.
	if (Point.X < Min.X)
	{
		ClosestPoint.X = Min.X;
	}
	else if (Point.X > Max.X)
	{
		ClosestPoint.X = Max.X;
	}

	if (Point.Y < Min.Y)
	{
		ClosestPoint.Y = Min.Y;
	}
	else if (Point.Y > Max.Y)
	{
		ClosestPoint.Y = Max.Y;
	}

	if (Point.Z < Min.Z)
	{
		ClosestPoint.Z = Min.Z;
	}
	else if (Point.Z > Max.Z)
	{
		ClosestPoint.Z = Max.Z;
	}

	return ClosestPoint;
}

FBox FBox::Overlap(const FBox& Other) const
{
	if (!Intersect(Other))
	{
		return FBox(ForceInit);
	}

	// Otherwise they overlap, so find the overlapping box.
	FVector MinVector, MaxVector;

	MinVector.X = FMath::Max(Min.X, Other.Min.X);
	MaxVector.X = FMath::Min(Max.X, Other.Max.X);

	MinVector.Y = FMath::Max(Min.Y, Other.Min.Y);
	MaxVector.Y = FMath::Min(Max.Y, Other.Max.Y);

	MinVector.Z = FMath::Max(Min.Z, Other.Min.Z);
	MaxVector.Z = FMath::Min(Max.Z, Other.Max.Z);

	return FBox(MinVector, MaxVector);
}

FBox FBox::TransformBy(const FMatrix& M) const
{
	// If we are not valid, return another invalid box.
	if (!IsValid)
	{
		return FBox(ForceInit);
	}

	const FVector Origin = GetCenter();
	const FVector Extent = GetExtent();

	const FVector NewOrigin(M.TransformPosition(Origin));
	const FVector NewExtent = FVector(M.M[0][0], M.M[0][1], M.M[0][2]).GetAbs() * Extent.X +
		FVector(M.M[1][0], M.M[1][1], M.M[1][2]).GetAbs() * Extent.Y +
		FVector(M.M[2][0], M.M[2][1], M.M[2][2]).GetAbs() * Extent.Z;

	return FBox(NewOrigin - NewExtent, NewOrigin + NewExtent);
}

FBox FBox::TransformBy(const FTransform& M) const
{
	return TransformBy(M.ToMatrixWithScale());
}

FBox FBox::InverseTransformBy(const FTransform& M) const
{
	const FVector Vertices[8] = {FVector(Min), FVector(Min.X, Min.Y, Max.Z), FVector(Min.X, Max.Y, Min.Z),
		FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Max.Y, Min.Z), FVector(Max.X, Min.Y, Max.Z),
		FVector(Min.X, Max.Y, Max.Z), FVector(Max)};

	FBox NewBox(ForceInit);

	for (int32 VertexIndex = 0; VertexIndex < int32(UE_ARRAY_COUNT(Vertices)); VertexIndex++)
	{
		NewBox += M.InverseTransformPosition(Vertices[VertexIndex]);
	}

	return NewBox;
}

FString FBox::ToString() const
{
	return FString::Printf(
		"IsValid=%s, Min=(%s), Max=(%s)", IsValid ? "true" : "false", *Min.ToString(), *Max.ToString());
}
