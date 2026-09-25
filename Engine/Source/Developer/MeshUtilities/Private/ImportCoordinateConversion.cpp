#include "ImportCoordinateConversion.h"

#include "MeshData.h"
#include "SkeletalAnimation.h"

namespace
{
	float QuatComponent(const FQuat& Q, int32 Axis)
	{
		return Axis == 0 ? Q.X : (Axis == 1 ? Q.Y : Q.Z);
	}
} // namespace

FImportCoordinateConversion::FImportCoordinateConversion(EImportAxes InSourceAxes, float InUnitsToCm)
	: SourceAxes(InSourceAxes)
	, UnitsToCm(InUnitsToCm)
{
	if (SourceAxes == EImportAxes::RightHandedYUp)
	{
		// (X, Z, Y): Y and Z swap.
		SourceAxisOf[0] = 0;
		SourceAxisOf[1] = 2;
		SourceAxisOf[2] = 1;
		bNegateAxis[0] = bNegateAxis[1] = bNegateAxis[2] = false;
	}
	else
	{
		// (X, -Y, Z): Y flips.
		SourceAxisOf[0] = 0;
		SourceAxisOf[1] = 1;
		SourceAxisOf[2] = 2;
		bNegateAxis[0] = bNegateAxis[2] = false;
		bNegateAxis[1] = true;
	}

	// The determinant: the sign of the axis permutation times one -1 per negated axis.
	bool bOddPermutation = false;
	for (int32 I = 0; I < 3; ++I)
	{
		for (int32 J = I + 1; J < 3; ++J)
		{
			bOddPermutation ^= SourceAxisOf[I] > SourceAxisOf[J];
		}
	}
	bMirror = bOddPermutation ^ bNegateAxis[0] ^ bNegateAxis[1] ^ bNegateAxis[2];
}

float FImportCoordinateConversion::SignedAxis(const FVector& Source, int32 WorldAxis) const
{
	const float Value = Source[SourceAxisOf[WorldAxis]];
	return bNegateAxis[WorldAxis] ? -Value : Value;
}

FVector FImportCoordinateConversion::GetSourceUp() const
{
	return SourceAxes == EImportAxes::RightHandedYUp ? FVector(0.0f, 1.0f, 0.0f) : FVector(0.0f, 0.0f, 1.0f);
}

FVector FImportCoordinateConversion::ConvertPosition(const FVector& Source) const
{
	return ConvertDirection(Source) * UnitsToCm;
}

FVector FImportCoordinateConversion::ConvertDirection(const FVector& Source) const
{
	return FVector(SignedAxis(Source, 0), SignedAxis(Source, 1), SignedAxis(Source, 2));
}

FVector4 FImportCoordinateConversion::ConvertTangent(const FVector4& Source) const
{
	const FVector Direction = ConvertDirection(FVector(Source.X, Source.Y, Source.Z));
	return FVector4(Direction.X, Direction.Y, Direction.Z, bMirror ? -Source.W : Source.W);
}

FVector FImportCoordinateConversion::ConvertScale(const FVector& Source) const
{
	return FVector(Source[SourceAxisOf[0]], Source[SourceAxisOf[1]], Source[SourceAxisOf[2]]);
}

FQuat FImportCoordinateConversion::ConvertRotation(const FQuat& Source) const
{
	// B R B^-1 is the same angle about B * axis, turned the other way when det B = -1: the axis converts as a direction
	// and is negated under a mirror.
	float Axis[3];
	for (int32 I = 0; I < 3; ++I)
	{
		const float Value = QuatComponent(Source, SourceAxisOf[I]);
		Axis[I] = (bNegateAxis[I] != bMirror) ? -Value : Value;
	}
	return FQuat(Axis[0], Axis[1], Axis[2], Source.W);
}

FMatrix FImportCoordinateConversion::ConvertMatrix(const FMatrix& Source) const
{
	// B maps source axis SourceAxisOf[I] to +-UnitsToCm on world axis I, so (B^-1 * M * B)[I][J] is
	// M[SourceAxisOf[I]][SourceAxisOf[J]] with the two axis signs; the translation row picks up UnitsToCm and the
	// projective column its inverse.
	FMatrix Out;
	for (int32 I = 0; I < 3; ++I)
	{
		for (int32 J = 0; J < 3; ++J)
		{
			const float Value = Source.M[SourceAxisOf[I]][SourceAxisOf[J]];
			Out.M[I][J] = (bNegateAxis[I] != bNegateAxis[J]) ? -Value : Value;
		}
		const float Projective = Source.M[SourceAxisOf[I]][3];
		Out.M[I][3] = (bNegateAxis[I] ? -Projective : Projective) / UnitsToCm;
		const float Translation = Source.M[3][SourceAxisOf[I]];
		Out.M[3][I] = (bNegateAxis[I] ? -Translation : Translation) * UnitsToCm;
	}
	Out.M[3][3] = Source.M[3][3];
	return Out;
}

FMatrix FImportCoordinateConversion::GetBasisMatrix() const
{
	FMatrix Basis(ForceInitToZero);
	for (int32 I = 0; I < 3; ++I)
	{
		Basis.M[SourceAxisOf[I]][I] = bNegateAxis[I] ? -UnitsToCm : UnitsToCm;
	}
	Basis.M[3][3] = 1.0f;
	return Basis;
}

void FImportCoordinateConversion::ConvertMeshData(FMeshData& Data) const
{
	for (FVertex& Vertex : Data.Vertices)
	{
		Vertex.Position = ConvertPosition(Vertex.Position);
		Vertex.Normal = ConvertDirection(Vertex.Normal);
		Vertex.Tangent = ConvertTangent(Vertex.Tangent);
	}
}

void FImportCoordinateConversion::ConvertSkeletalMeshData(FSkeletalMeshData& Data) const
{
	for (FSkeletalVertex& Vertex : Data.Vertices)
	{
		Vertex.Position = ConvertPosition(Vertex.Position);
		Vertex.Normal = ConvertDirection(Vertex.Normal);
		Vertex.Tangent = ConvertTangent(Vertex.Tangent);
	}
	// A flipped axis swaps its minimum and maximum.
	const FVector Min = ConvertPosition(Data.LocalMin);
	const FVector Max = ConvertPosition(Data.LocalMax);
	Data.LocalMin = Min.ComponentMin(Max);
	Data.LocalMax = Min.ComponentMax(Max);
	for (FMatrix& InverseBind : Data.Skeleton.InverseBindPose)
	{
		InverseBind = ConvertMatrix(InverseBind);
	}
	ConvertAnimSequence(Data.EmbeddedAnim);
}

void FImportCoordinateConversion::ConvertAnimSequence(UAnimSequence& Sequence) const
{
	for (TArray<FMatrix>& Frame : Sequence.LocalPoseFrames)
	{
		for (FMatrix& BoneWorld : Frame)
		{
			BoneWorld = ConvertMatrix(BoneWorld);
		}
	}
}
