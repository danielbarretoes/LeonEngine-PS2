#include "Debug/DebugDraw.h"

namespace
{
	/** The 12 edges of a box whose corners are numbered like AddAabb's. */
	constexpr int32 BoxEdges[12][2] = {
		{0, 1},
		{1, 2},
		{2, 3},
		{3, 0},
		{4, 5},
		{5, 6},
		{6, 7},
		{7, 4},
		{0, 4},
		{1, 5},
		{2, 6},
		{3, 7},
	};
} // namespace

void FDebugDraw::Clear()
{
	Vertices.Reset();
}

void FDebugDraw::AddLine(const FVector& A, const FVector& B, const FLinearColor& InColor)
{
	const FVector Color(InColor.R, InColor.G, InColor.B);
	Vertices.Add(FLineVertex{A, Color});
	Vertices.Add(FLineVertex{B, Color});
}

void FDebugDraw::AddArrow(
	const FVector& From, const FVector& To, const FLinearColor& InColor, float HeadLength, float HeadWidth)
{
	AddLine(From, To, InColor);

	const FVector Shaft = To - From;
	const float Len = Shaft.Size();
	if (Len < 1.0e-2f)
	{
		return;
	}
	const FVector Dir = Shaft / Len;
	// A side vector across the shaft, horizontal when it can be (world up ^ shaft; along X for a vertical shaft).
	FVector Side = FVector(0.0f, 0.0f, 1.0f) ^ Dir;
	if ((Side | Side) < 1.0e-6f)
	{
		Side = FVector(1.0f, 0.0f, 0.0f) ^ Dir;
	}
	Side = Side.GetUnsafeNormal() * HeadWidth;
	const FVector Back = To - (Dir * HeadLength);
	AddLine(To, Back + Side, InColor);
	AddLine(To, Back - Side, InColor);
}

void FDebugDraw::AddAabb(const FVector& WorldMin, const FVector& WorldMax, const FLinearColor& InColor)
{
	const FVector& Mn = WorldMin;
	const FVector& Mx = WorldMax;
	const FVector C[8] = {
		FVector(Mn.X, Mn.Y, Mn.Z),
		FVector(Mx.X, Mn.Y, Mn.Z),
		FVector(Mx.X, Mx.Y, Mn.Z),
		FVector(Mn.X, Mx.Y, Mn.Z),
		FVector(Mn.X, Mn.Y, Mx.Z),
		FVector(Mx.X, Mn.Y, Mx.Z),
		FVector(Mx.X, Mx.Y, Mx.Z),
		FVector(Mn.X, Mx.Y, Mx.Z),
	};
	for (const auto& Edge : BoxEdges)
	{
		AddLine(C[Edge[0]], C[Edge[1]], InColor);
	}
}

void FDebugDraw::AddAxes(const FVector& Origin, float Length)
{
	AddLine(Origin, Origin + FVector(Length, 0.0f, 0.0f), FLinearColor::Red);
	AddLine(Origin, Origin + FVector(0.0f, Length, 0.0f), FLinearColor::Green);
	AddLine(Origin, Origin + FVector(0.0f, 0.0f, Length), FLinearColor::Blue);
}

void FDebugDraw::AddAxes(const FTransform& Transform, float Length)
{
	const FVector Origin = Transform.GetLocation();
	AddLine(Origin, Origin + (Transform.GetUnitAxis(EAxis::X) * Length), FLinearColor::Red);
	AddLine(Origin, Origin + (Transform.GetUnitAxis(EAxis::Y) * Length), FLinearColor::Green);
	AddLine(Origin, Origin + (Transform.GetUnitAxis(EAxis::Z) * Length), FLinearColor::Blue);
}

void FDebugDraw::AddViewAxes(const FMatrix& View, float Extent)
{
	struct FViewAxis
	{
		FVector ViewDirection;
		FLinearColor Color;
	};
	FViewAxis Axes[3] = {
		{FVector(View.TransformVector(FVector(1.0f, 0.0f, 0.0f))), FLinearColor::Red},
		{FVector(View.TransformVector(FVector(0.0f, 1.0f, 0.0f))), FLinearColor::Green},
		{FVector(View.TransformVector(FVector(0.0f, 0.0f, 1.0f))), FLinearColor::Blue},
	};
	// View z is forward (away from the viewer): larger z is farther, so it is drawn first.
	StableSort(Axes, UE_ARRAY_COUNT(Axes),
		[](const FViewAxis& A, const FViewAxis& B) { return A.ViewDirection.Z > B.ViewDirection.Z; });
	for (const FViewAxis& Axis : Axes)
	{
		AddLine(FVector::ZeroVector, FVector(Axis.ViewDirection.X * Extent, Axis.ViewDirection.Y * Extent, 0.0f),
			Axis.Color);
	}
}

void FDebugDraw::AddLightFrustum(const FMatrix& LightSpace, const FLinearColor& InColor)
{
	const FMatrix Inv = LightSpace.Inverse();
	const FVector Ndc[8] = {
		FVector(-1.0f, -1.0f, -1.0f),
		FVector(1.0f, -1.0f, -1.0f),
		FVector(1.0f, 1.0f, -1.0f),
		FVector(-1.0f, 1.0f, -1.0f),
		FVector(-1.0f, -1.0f, 1.0f),
		FVector(1.0f, -1.0f, 1.0f),
		FVector(1.0f, 1.0f, 1.0f),
		FVector(-1.0f, 1.0f, 1.0f),
	};

	FVector World[8];
	for (int32 I = 0; I < 8; ++I)
	{
		FVector4 P = Inv.TransformFVector4(FVector4(Ndc[I], 1.0f));
		if (FMath::Abs(P.W) > 1e-6f)
		{
			P = P / P.W;
		}
		World[I] = FVector(P.X, P.Y, P.Z);
	}

	for (const auto& Edge : BoxEdges)
	{
		AddLine(World[Edge[0]], World[Edge[1]], InColor);
	}
}
