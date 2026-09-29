#include "Math/VectorMath.h"

#include "CoreMinimal.h"

namespace
{

	/** An unused plane slot's W: every point is inside it. */
	constexpr float AlwaysInside = 1.0e30f;

} // namespace

FVectorPlaneSet::FVectorPlaneSet()
{
	for (int32 Index = 0; Index < MaxPlanes; ++Index)
	{
		X[Index] = 0.0f;
		Y[Index] = 0.0f;
		Z[Index] = 0.0f;
		W[Index] = AlwaysInside;
		AbsX[Index] = 0.0f;
		AbsY[Index] = 0.0f;
		AbsZ[Index] = 0.0f;
	}
}

void FVectorPlaneSet::AddPlane(float A, float B, float C, float D)
{
	check(NumPlanes < MaxPlanes);
	X[NumPlanes] = A;
	Y[NumPlanes] = B;
	Z[NumPlanes] = C;
	W[NumPlanes] = D;
	AbsX[NumPlanes] = FMath::Abs(A);
	AbsY[NumPlanes] = FMath::Abs(B);
	AbsZ[NumPlanes] = FMath::Abs(C);
	++NumPlanes;
}

void FVectorMathFPU::MatrixMultiply(FMatrix& Result, const FMatrix& A, const FMatrix& B)
{
	FMatrix Product;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			Product.M[Row][Col] = A.M[Row][0] * B.M[0][Col] + A.M[Row][1] * B.M[1][Col] + A.M[Row][2] * B.M[2][Col] +
				A.M[Row][3] * B.M[3][Col];
		}
	}
	Result = Product;
}

void FVectorMathFPU::TransformVector4(FVector4& Result, const FMatrix& M, const FVector4& P)
{
	const FVector4 In = P;
	Result.X = In.X * M.M[0][0] + In.Y * M.M[1][0] + In.Z * M.M[2][0] + In.W * M.M[3][0];
	Result.Y = In.X * M.M[0][1] + In.Y * M.M[1][1] + In.Z * M.M[2][1] + In.W * M.M[3][1];
	Result.Z = In.X * M.M[0][2] + In.Y * M.M[1][2] + In.Z * M.M[2][2] + In.W * M.M[3][2];
	Result.W = In.X * M.M[0][3] + In.Y * M.M[1][3] + In.Z * M.M[2][3] + In.W * M.M[3][3];
}

bool FVectorMathFPU::IsBoxOutside(const FVectorPlaneSet& Planes, const FVector& Center, const FVector& Extent)
{
	// The order VU0 accumulates in: the centre's three terms, the reach's three, then W.
	for (int32 Index = 0; Index < Planes.NumPlanes; ++Index)
	{
		const float Distance = Planes.X[Index] * Center.X + Planes.Y[Index] * Center.Y + Planes.Z[Index] * Center.Z +
			Planes.AbsX[Index] * Extent.X + Planes.AbsY[Index] * Extent.Y + Planes.AbsZ[Index] * Extent.Z +
			Planes.W[Index];
		if (Distance < 0.0f)
		{
			return true;
		}
	}
	return false;
}

bool FVectorMathFPU::IsSphereOutside(const FVectorPlaneSet& Planes, const FVector& Center, float Radius)
{
	for (int32 Index = 0; Index < Planes.NumPlanes; ++Index)
	{
		const float Distance = Planes.X[Index] * Center.X + Planes.Y[Index] * Center.Y + Planes.Z[Index] * Center.Z +
			Planes.W[Index] + Radius;
		if (Distance < 0.0f)
		{
			return true;
		}
	}
	return false;
}

#if !PLATFORM_PS2

// No vector unit: the reference is the platform's (the PS2's VU0 versions are in its Core, PS2VectorMath.cpp).

void FVectorMath::MatrixMultiply(FMatrix& Result, const FMatrix& A, const FMatrix& B)
{
	FVectorMathFPU::MatrixMultiply(Result, A, B);
}

void FVectorMath::TransformVector4(FVector4& Result, const FMatrix& M, const FVector4& P)
{
	FVectorMathFPU::TransformVector4(Result, M, P);
}

bool FVectorMath::IsBoxOutside(const FVectorPlaneSet& Planes, const FVector& Center, const FVector& Extent)
{
	return FVectorMathFPU::IsBoxOutside(Planes, Center, Extent);
}

bool FVectorMath::IsSphereOutside(const FVectorPlaneSet& Planes, const FVector& Center, float Radius)
{
	return FVectorMathFPU::IsSphereOutside(Planes, Center, Radius);
}

bool FVectorMath::IsVectorUnit()
{
	return false;
}

#endif
