#include "AnimationRuntime.h"

#include "AnimCompression.h"

namespace
{

	/** A transform's rotation, translation and scale scaled from the identity toward it by Alpha. */
	FTransform ScaleFromIdentity(const FTransform& Delta, float Alpha)
	{
		return FTransform(FAnimCompression::InterpolateRotation(FQuat::Identity, Delta.GetRotation(), Alpha),
			Delta.GetTranslation() * Alpha, FVector::OneVector + ((Delta.GetScale3D() - FVector::OneVector) * Alpha));
	}

	/** Twice the signed area of the triangle A, B, C (positive when counter-clockwise). */
	float SignedArea2(const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		return FVector2D::CrossProduct(B - A, C - A);
	}

	/** Point is strictly inside the circle through A, B and C (a triangle of any winding). */
	bool IsInsideCircumcircle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& Point)
	{
		const float Ax = A.X - Point.X;
		const float Ay = A.Y - Point.Y;
		const float Bx = B.X - Point.X;
		const float By = B.Y - Point.Y;
		const float Cx = C.X - Point.X;
		const float Cy = C.Y - Point.Y;
		const float Determinant = ((Ax * Ax) + (Ay * Ay)) * ((Bx * Cy) - (Cx * By)) -
			((Bx * Bx) + (By * By)) * ((Ax * Cy) - (Cx * Ay)) + ((Cx * Cx) + (Cy * Cy)) * ((Ax * By) - (Bx * Ay));
		// The determinant's sign follows the winding.
		const float Orientation = SignedArea2(A, B, C) > 0.0f ? 1.0f : -1.0f;
		return Determinant * Orientation > 1.0e-6f;
	}

	/** The barycentric weights of Point in the triangle A, B, C (any may be negative outside it). */
	void GetBarycentric(
		const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& Point, float OutWeights[3])
	{
		const float Area = SignedArea2(A, B, C);
		OutWeights[0] = SignedArea2(Point, B, C) / Area;
		OutWeights[1] = SignedArea2(A, Point, C) / Area;
		OutWeights[2] = 1.0f - OutWeights[0] - OutWeights[1];
	}

	/** Point is inside the triangle, or on its edges within Tolerance (in barycentric units). */
	bool IsInsideTriangle(
		const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& Point, float Tolerance)
	{
		float Weights[3];
		GetBarycentric(A, B, C, Point, Weights);
		return Weights[0] >= -Tolerance && Weights[1] >= -Tolerance && Weights[2] >= -Tolerance;
	}

	/** The segments A-B and C-D cross at a point inside both (touching ends do not count). */
	bool DoSegmentsCross(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		constexpr float Epsilon = 1.0e-6f;
		const float D1 = SignedArea2(A, B, C);
		const float D2 = SignedArea2(A, B, D);
		const float D3 = SignedArea2(C, D, A);
		const float D4 = SignedArea2(C, D, B);
		return ((D1 > Epsilon && D2 < -Epsilon) || (D1 < -Epsilon && D2 > Epsilon)) &&
			((D3 > Epsilon && D4 < -Epsilon) || (D3 < -Epsilon && D4 > Epsilon));
	}

	/** The interiors of two triangles overlap: an edge of one crosses an edge of the other, or one's centre is inside
	 * the other. */
	bool DoTrianglesOverlap(
		TArrayView<const FVector2D> Points, const FBlendSpaceTriangle& T0, const FBlendSpaceTriangle& T1)
	{
		for (int32 EdgeA = 0; EdgeA < 3; ++EdgeA)
		{
			const FVector2D& A0 = Points[T0.Indices[EdgeA]];
			const FVector2D& A1 = Points[T0.Indices[(EdgeA + 1) % 3]];
			for (int32 EdgeB = 0; EdgeB < 3; ++EdgeB)
			{
				if (DoSegmentsCross(A0, A1, Points[T1.Indices[EdgeB]], Points[T1.Indices[(EdgeB + 1) % 3]]))
				{
					return true;
				}
			}
		}
		const FVector2D Centre0 =
			(Points[T0.Indices[0]] + Points[T0.Indices[1]] + Points[T0.Indices[2]]) * (1.0f / 3.0f);
		const FVector2D Centre1 =
			(Points[T1.Indices[0]] + Points[T1.Indices[1]] + Points[T1.Indices[2]]) * (1.0f / 3.0f);
		return IsInsideTriangle(
				   Points[T1.Indices[0]], Points[T1.Indices[1]], Points[T1.Indices[2]], Centre0, -1.0e-4f) ||
			IsInsideTriangle(Points[T0.Indices[0]], Points[T0.Indices[1]], Points[T0.Indices[2]], Centre1, -1.0e-4f);
	}

	/** The parameter (0 to 1) of the point of segment A-B nearest Point. */
	float GetSegmentParameter(const FVector2D& A, const FVector2D& B, const FVector2D& Point)
	{
		const FVector2D Edge = B - A;
		const float LengthSquared = Edge.SizeSquared();
		return LengthSquared > 1.0e-12f
			? FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / LengthSquared, 0.0f, 1.0f)
			: 0.0f;
	}

	/** Two samples of a segment by the parameter T (from A to B) into OutSamples; the count kept. */
	int32 SegmentWeights(int32 IndexA, int32 IndexB, float T, FBlendSampleData OutSamples[3])
	{
		int32 Count = 0;
		if (1.0f - T >= 1.0e-5f)
		{
			OutSamples[Count++] = {IndexA, 1.0f - T};
		}
		if (T >= 1.0e-5f)
		{
			OutSamples[Count++] = {IndexB, T};
		}
		const float Sum = Count == 2 ? 1.0f : OutSamples[0].TotalWeight;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutSamples[Index].TotalWeight /= Sum;
		}
		return Count;
	}

} // namespace

void FAnimationRuntime::BlendTwoPosesTogether(
	TArrayView<const FTransform> A, TArrayView<const FTransform> B, float WeightOfB, TArrayView<FTransform> OutPose)
{
	check(A.Num() == B.Num() && OutPose.Num() == A.Num());
	const float WeightOfA = 1.0f - WeightOfB;
	for (int32 Bone = 0; Bone < A.Num(); ++Bone)
	{
		const FTransform& PoseA = A[Bone];
		const FTransform& PoseB = B[Bone];
		OutPose[Bone] =
			FTransform(FAnimCompression::InterpolateRotation(PoseA.GetRotation(), PoseB.GetRotation(), WeightOfB),
				(PoseA.GetTranslation() * WeightOfA) + (PoseB.GetTranslation() * WeightOfB),
				(PoseA.GetScale3D() * WeightOfA) + (PoseB.GetScale3D() * WeightOfB));
	}
}

void FAnimationRuntime::BlendPosesTogether(
	TArrayView<const TArrayView<const FTransform>> SourcePoses, const float* Weights, TArrayView<FTransform> OutPose)
{
	const int32 NumPoses = SourcePoses.Num();
	if (NumPoses == 0)
	{
		return;
	}
	const int32 NumBones = OutPose.Num();
	for (int32 Bone = 0; Bone < NumBones; ++Bone)
	{
		const FQuat First = SourcePoses[0][Bone].GetRotation();
		FQuat Rotation(0.0f, 0.0f, 0.0f, 0.0f);
		FVector Translation = FVector::ZeroVector;
		FVector Scale = FVector::ZeroVector;
		for (int32 Pose = 0; Pose < NumPoses; ++Pose)
		{
			const FTransform& Source = SourcePoses[Pose][Bone];
			const float Weight = Weights[Pose];
			const FQuat Q = Source.GetRotation();
			// On the first pose's hemisphere, so opposite quaternions of one rotation do not cancel.
			const float Sign = (Q | First) < 0.0f ? -Weight : Weight;
			Rotation = FQuat(Rotation.X + (Q.X * Sign), Rotation.Y + (Q.Y * Sign), Rotation.Z + (Q.Z * Sign),
				Rotation.W + (Q.W * Sign));
			Translation += Source.GetTranslation() * Weight;
			Scale += Source.GetScale3D() * Weight;
		}
		OutPose[Bone] = FTransform(Rotation.GetNormalized(), Translation, Scale);
	}
}

void FAnimationRuntime::BlendPosesPerBoneFilter(TArrayView<FTransform> InOutBasePose,
	TArrayView<const FTransform> BlendPose, TArrayView<const float> BoneWeights, float Alpha)
{
	check(InOutBasePose.Num() == BlendPose.Num() && BoneWeights.Num() == InOutBasePose.Num());
	for (int32 Bone = 0; Bone < InOutBasePose.Num(); ++Bone)
	{
		const float Weight = BoneWeights[Bone] * Alpha;
		if (Weight <= 0.0f)
		{
			continue;
		}
		const FTransform& Base = InOutBasePose[Bone];
		const FTransform& Blend = BlendPose[Bone];
		if (Weight >= 1.0f)
		{
			InOutBasePose[Bone] = Blend;
			continue;
		}
		InOutBasePose[Bone] =
			FTransform(FAnimCompression::InterpolateRotation(Base.GetRotation(), Blend.GetRotation(), Weight),
				FMath::Lerp(Base.GetTranslation(), Blend.GetTranslation(), Weight),
				FMath::Lerp(Base.GetScale3D(), Blend.GetScale3D(), Weight));
	}
}

void FAnimationRuntime::FillBranchBoneWeights(
	const FReferenceSkeleton& Skeleton, int32 BranchBoneIndex, TArrayView<float> OutBoneWeights)
{
	check(OutBoneWeights.Num() == Skeleton.GetNum());
	for (int32 Bone = 0; Bone < OutBoneWeights.Num(); ++Bone)
	{
		// A parent comes before its child, so its weight is final.
		const int32 Parent = Skeleton.GetParentIndex(Bone);
		const bool bInBranch =
			Bone == BranchBoneIndex || (Parent != INDEX_NONE && Parent < Bone && OutBoneWeights[Parent] > 0.0f);
		OutBoneWeights[Bone] = BranchBoneIndex != INDEX_NONE && bInBranch ? 1.0f : 0.0f;
	}
}

void FAnimationRuntime::ConvertPoseToAdditive(
	TArrayView<FTransform> InOutTargetPose, TArrayView<const FTransform> BasePose)
{
	check(InOutTargetPose.Num() == BasePose.Num());
	for (int32 Bone = 0; Bone < InOutTargetPose.Num(); ++Bone)
	{
		const FTransform& Target = InOutTargetPose[Bone];
		const FTransform& Base = BasePose[Bone];
		const FVector BaseScale = Base.GetScale3D();
		const FVector SafeBaseScale(FMath::Abs(BaseScale.X) > 1.0e-6f ? BaseScale.X : 1.0f,
			FMath::Abs(BaseScale.Y) > 1.0e-6f ? BaseScale.Y : 1.0f,
			FMath::Abs(BaseScale.Z) > 1.0e-6f ? BaseScale.Z : 1.0f);
		InOutTargetPose[Bone] = FTransform((Target.GetRotation() * Base.GetRotation().Inverse()).GetNormalized(),
			Target.GetTranslation() - Base.GetTranslation(), Target.GetScale3D() / SafeBaseScale);
	}
}

void FAnimationRuntime::AccumulateAdditivePose(TArrayView<FTransform> InOutBasePose,
	TArrayView<const FTransform> AdditivePose, float Weight, TArrayView<const float> BoneWeights)
{
	check(InOutBasePose.Num() == AdditivePose.Num());
	check(BoneWeights.Num() == 0 || BoneWeights.Num() == InOutBasePose.Num());
	for (int32 Bone = 0; Bone < InOutBasePose.Num(); ++Bone)
	{
		const float BoneWeight = BoneWeights.Num() > 0 ? Weight * BoneWeights[Bone] : Weight;
		if (BoneWeight <= 0.0f)
		{
			continue;
		}
		const FTransform Delta =
			BoneWeight >= 1.0f ? AdditivePose[Bone] : ScaleFromIdentity(AdditivePose[Bone], BoneWeight);
		const FTransform& Base = InOutBasePose[Bone];
		InOutBasePose[Bone] = FTransform((Delta.GetRotation() * Base.GetRotation()).GetNormalized(),
			Base.GetTranslation() + Delta.GetTranslation(), Base.GetScale3D() * Delta.GetScale3D());
	}
}

void FAnimationRuntime::FillUpComponentSpaceTransforms(
	const FReferenceSkeleton& Skeleton, TArrayView<const FTransform> LocalPose, TArray<FMatrix>& OutComponentSpace)
{
	const int32 NumBones = FMath::Min(Skeleton.GetNum(), LocalPose.Num());
	OutComponentSpace.SetNum(NumBones, false);
	for (int32 Bone = 0; Bone < NumBones; ++Bone)
	{
		const int32 Parent = Skeleton.GetParentIndex(Bone);
		const FMatrix Local = LocalPose[Bone].ToMatrixWithScale();
		// A parent comes before its child, so its matrix is final.
		OutComponentSpace[Bone] = Parent != INDEX_NONE && Parent < Bone ? Local * OutComponentSpace[Parent] : Local;
	}
}

void FAnimationRuntime::GetSkinMatrices(
	const FReferenceSkeleton& Skeleton, TArrayView<const FMatrix> ComponentSpace, TArray<FMatrix>& OutSkin)
{
	const int32 NumBones = FMath::Min(ComponentSpace.Num(), Skeleton.InverseBindPose.Num());
	OutSkin.SetNum(NumBones, false);
	for (int32 Bone = 0; Bone < NumBones; ++Bone)
	{
		// Row vectors: the inverse bind first, then the bone's pose.
		OutSkin[Bone] = Skeleton.InverseBindPose[Bone] * ComponentSpace[Bone];
	}
}

void FBlendSpaceTriangulation::Triangulate(
	TArrayView<const FVector2D> Points, TArray<FBlendSpaceTriangle>& OutTriangles)
{
	OutTriangles.Reset();
	const int32 NumPoints = Points.Num();
	constexpr float MinArea2 = 1.0e-6f;
	for (int32 I = 0; I < NumPoints; ++I)
	{
		for (int32 J = I + 1; J < NumPoints; ++J)
		{
			for (int32 K = J + 1; K < NumPoints; ++K)
			{
				const FVector2D& A = Points[I];
				const FVector2D& B = Points[J];
				const FVector2D& C = Points[K];
				if (FMath::Abs(SignedArea2(A, B, C)) <= MinArea2)
				{
					continue;
				}
				bool bEmpty = true;
				for (int32 Other = 0; Other < NumPoints && bEmpty; ++Other)
				{
					bEmpty = Other == I || Other == J || Other == K || !IsInsideCircumcircle(A, B, C, Points[Other]);
				}
				if (!bEmpty)
				{
					continue;
				}
				FBlendSpaceTriangle Triangle;
				Triangle.Indices[0] = I;
				Triangle.Indices[1] = J;
				Triangle.Indices[2] = K;
				bool bOverlaps = false;
				for (const FBlendSpaceTriangle& Taken : OutTriangles)
				{
					if (DoTrianglesOverlap(Points, Taken, Triangle))
					{
						bOverlaps = true;
						break;
					}
				}
				if (!bOverlaps)
				{
					OutTriangles.Add(Triangle);
				}
			}
		}
	}
}

int32 FBlendSpaceTriangulation::GetWeights(TArrayView<const FVector2D> Points,
	TArrayView<const FBlendSpaceTriangle> Triangles, const FVector2D& Query, FBlendSampleData OutSamples[3])
{
	for (int32 Index = 0; Index < 3; ++Index)
	{
		OutSamples[Index] = FBlendSampleData();
	}
	if (Points.Num() == 0)
	{
		return 0;
	}
	if (Points.Num() == 1)
	{
		OutSamples[0] = {0, 1.0f};
		return 1;
	}

	// Inside a triangle: its barycentric weights.
	constexpr float Tolerance = 1.0e-4f;
	for (const FBlendSpaceTriangle& Triangle : Triangles)
	{
		const FVector2D& A = Points[Triangle.Indices[0]];
		const FVector2D& B = Points[Triangle.Indices[1]];
		const FVector2D& C = Points[Triangle.Indices[2]];
		if (!IsInsideTriangle(A, B, C, Query, Tolerance))
		{
			continue;
		}
		float Weights[3];
		GetBarycentric(A, B, C, Query, Weights);
		int32 Count = 0;
		float Sum = 0.0f;
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const float Weight = FMath::Max(Weights[Corner], 0.0f);
			if (Weight >= 1.0e-5f)
			{
				OutSamples[Count++] = {Triangle.Indices[Corner], Weight};
				Sum += Weight;
			}
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutSamples[Index].TotalWeight /= Sum;
		}
		return Count;
	}

	// Outside: the nearest point on an edge of the triangles, or of the points in order without triangles.
	float BestDistanceSquared = TNumericLimits<float>::Max();
	int32 BestA = 0;
	int32 BestB = 0;
	float BestT = 0.0f;
	auto ConsiderEdge = [&](int32 IndexA, int32 IndexB)
	{
		const float T = GetSegmentParameter(Points[IndexA], Points[IndexB], Query);
		const FVector2D Nearest = Points[IndexA] + ((Points[IndexB] - Points[IndexA]) * T);
		const float DistanceSquared = FVector2D::DistSquared(Nearest, Query);
		if (DistanceSquared < BestDistanceSquared - 1.0e-9f)
		{
			BestDistanceSquared = DistanceSquared;
			BestA = IndexA;
			BestB = IndexB;
			BestT = T;
		}
	};
	if (Triangles.Num() > 0)
	{
		for (const FBlendSpaceTriangle& Triangle : Triangles)
		{
			for (int32 Edge = 0; Edge < 3; ++Edge)
			{
				ConsiderEdge(Triangle.Indices[Edge], Triangle.Indices[(Edge + 1) % 3]);
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			ConsiderEdge(Index, Index + 1);
		}
	}
	return SegmentWeights(BestA, BestB, BestT, OutSamples);
}
