#include "CoreMinimal.h"
#include "ImportCoordinateConversion.h"
#include "LegacyCoordinateConversion.h"
#include "Math/RandomStream.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "ObjImportPrivate.h"
#include "SkeletalAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Same bits (a negative zero is not a zero). */
	template <typename T>
	bool SameBits(const T& A, const T& B)
	{
		return FMemory::Memcmp(&A, &B, sizeof(T)) == 0;
	}

	FVector RandomVector(FRandomStream& Random, float Range)
	{
		return FVector(
			Random.FRandRange(-Range, Range), Random.FRandRange(-Range, Range), Random.FRandRange(-Range, Range));
	}

	FQuat RandomRotation(FRandomStream& Random)
	{
		return FQuat(Random.GetUnitVector(), Random.FRandRange(-PI, PI));
	}

	/** An affine matrix (row vectors): scale, then rotation, then translation. */
	FMatrix RandomAffine(FRandomStream& Random)
	{
		const FVector Scale(
			Random.FRandRange(0.5f, 2.0f), Random.FRandRange(0.5f, 2.0f), Random.FRandRange(0.5f, 2.0f));
		return FScaleMatrix(Scale) * FQuatRotationMatrix(RandomRotation(Random)) *
			FTranslationMatrix(RandomVector(Random, 5.0f));
	}

	/** The Cube fixture as the OBJ stores it (right-handed, Y up, metres). */
	FMeshData LoadCubeFixtureSourceSpace()
	{
		return LoadObjSourceSpace(
			FPaths::Combine(FPaths::EngineSourceDir(), "Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj"));
	}

	/** cross(E1, E2) of triangle Triangle. */
	FVector TriangleCross(const FMeshData& Data, int32 Triangle)
	{
		const FVector& P0 = Data.Vertices[static_cast<int32>(Data.Indices[Triangle * 3 + 0])].Position;
		const FVector& P1 = Data.Vertices[static_cast<int32>(Data.Indices[Triangle * 3 + 1])].Position;
		const FVector& P2 = Data.Vertices[static_cast<int32>(Data.Indices[Triangle * 3 + 2])].Position;
		return (P1 - P0) ^ (P2 - P0);
	}

	/** The stored normals of triangle Triangle, summed. */
	FVector TriangleNormal(const FMeshData& Data, int32 Triangle)
	{
		FVector Sum = FVector::ZeroVector;
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			Sum += Data.Vertices[static_cast<int32>(Data.Indices[Triangle * 3 + Corner])].Normal;
		}
		return Sum;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImportCoordinateConversionMatchesLegacyTest,
	"System.MeshUtilities.ImportCoordinateConversion.MatchesLegacy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FImportCoordinateConversionMatchesLegacyTest::RunTest(const FString& Parameters)
{
	// Right-handed Y up in metres is the legacy basis: every conversion gives FLegacyCoordinateConversion's bits.
	const FImportCoordinateConversion Conversion(EImportAxes::RightHandedYUp, FImportCoordinateConversion::CmPerMetre);
	TestEqual("Units", Conversion.GetUnitsToCm(), FLegacyCoordinateConversion::UnitsPerMetre);

	FRandomStream Random(0x5eed);
	TArray<FVector> Vectors = {FVector(0.0f, -0.0f, 0.0f), FVector(1.0f, 2.0f, 3.0f), FVector(-0.0f, 1.0e-30f, -7.5f)};
	for (int32 Index = 0; Index < 200; ++Index)
	{
		Vectors.Add(RandomVector(Random, 1000.0f));
	}

	int32 Mismatches = 0;
	for (const FVector& V : Vectors)
	{
		const float W = Random.FRandRange(-1.0f, 1.0f) < 0.0f ? -1.0f : 1.0f;
		const FVector4 Tangent(V.X, V.Y, V.Z, W);
		const FQuat Rotation(V.X, V.Y, V.Z, Random.FRandRange(-1.0f, 1.0f));
		Mismatches += SameBits(Conversion.ConvertPosition(V), FLegacyCoordinateConversion::ConvertPosition(V)) ? 0 : 1;
		Mismatches +=
			SameBits(Conversion.ConvertDirection(V), FLegacyCoordinateConversion::ConvertDirection(V)) ? 0 : 1;
		Mismatches +=
			SameBits(Conversion.ConvertTangent(Tangent), FLegacyCoordinateConversion::ConvertTangent(Tangent)) ? 0 : 1;
		Mismatches += SameBits(Conversion.ConvertScale(V), FLegacyCoordinateConversion::ConvertScale(V)) ? 0 : 1;
		Mismatches +=
			SameBits(Conversion.ConvertRotation(Rotation), FLegacyCoordinateConversion::ConvertRotation(Rotation)) ? 0
																												   : 1;
	}
	TestEqual("Position, direction, tangent, scale and rotation bits", Mismatches, 0);

	// Mesh data: every vertex, bit for bit; the index order is kept.
	FMeshData Import = LoadCubeFixtureSourceSpace();
	ComputeTangents(Import, EMeshDataBasis::LegacyYUp);
	for (FVertex& Vertex : Import.Vertices)
	{
		Vertex.TexCoord = FVector2D(Random.FRandRange(0.0f, 1.0f), Random.FRandRange(0.0f, 1.0f));
	}
	FMeshData Legacy = Import;
	Conversion.ConvertMeshData(Import);
	FLegacyCoordinateConversion::ConvertMeshData(Legacy);
	TestTrue("Mesh indices", Import.Indices == Legacy.Indices);
	TestTrue("Mesh vertices",
		Import.Vertices.Num() == Legacy.Vertices.Num() &&
			FMemory::Memcmp(Import.Vertices.GetData(), Legacy.Vertices.GetData(),
				sizeof(FVertex) * static_cast<SIZE_T>(Import.Vertices.Num())) == 0);

	// The legacy converter has no matrix: the converted matrix acts on converted points as the source one on source
	// points (the legacy position conversion), and it is B^-1 * M * B.
	const FMatrix Basis = Conversion.GetBasisMatrix();
	for (int32 Index = 0; Index < 20; ++Index)
	{
		const FMatrix Source = RandomAffine(Random);
		const FMatrix Converted = Conversion.ConvertMatrix(Source);
		const FVector Point = RandomVector(Random, 3.0f);
		const FVector Expected = FLegacyCoordinateConversion::ConvertPosition(FVector(Source.TransformPosition(Point)));
		const FVector Actual(Converted.TransformPosition(FLegacyCoordinateConversion::ConvertPosition(Point)));
		TestTrue(*FString::Printf("Matrix %d on points", Index), Actual.Equals(Expected, 1.0e-2f));
		TestTrue(*FString::Printf("Matrix %d is B^-1 M B", Index),
			Converted.Equals(Basis.Inverse() * Source * Basis, 1.0e-3f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImportCoordinateConversionRightHandedZUpTest,
	"System.MeshUtilities.ImportCoordinateConversion.RightHandedZUp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FImportCoordinateConversionRightHandedZUpTest::RunTest(const FString& Parameters)
{
	// UE's FBX mapping (FFbxDataConverter): Y flips, the unit scales. Up stays up, front (-Y) becomes +Y, right stays
	// +X.
	const FImportCoordinateConversion Conversion(EImportAxes::RightHandedZUp, 2.54f);
	TestTrue("Position",
		Conversion.ConvertPosition(FVector(1.0f, 2.0f, 3.0f)).Equals(FVector(2.54f, -5.08f, 7.62f), 1.0e-5f));
	TestTrue("Up", Conversion.ConvertDirection(FVector(0.0f, 0.0f, 1.0f)) == FVector(0.0f, 0.0f, 1.0f));
	TestTrue("Front", Conversion.ConvertDirection(FVector(0.0f, -1.0f, 0.0f)) == FVector(0.0f, 1.0f, 0.0f));
	TestTrue("Right", Conversion.ConvertDirection(FVector(1.0f, 0.0f, 0.0f)) == FVector(1.0f, 0.0f, 0.0f));
	TestTrue("Source up", Conversion.GetSourceUp() == FVector(0.0f, 0.0f, 1.0f));
	TestTrue("Scale", Conversion.ConvertScale(FVector(1.0f, 2.0f, 3.0f)) == FVector(1.0f, 2.0f, 3.0f));
	const FVector4 Tangent = Conversion.ConvertTangent(FVector4(0.0f, 1.0f, 0.0f, 1.0f));
	TestTrue("Tangent", FVector(Tangent) == FVector(0.0f, -1.0f, 0.0f) && Tangent.W == -1.0f);
	// FFbxDataConverter::ConvertRotToQuat gives (X, -Y, Z, -W): the same rotation as (-X, Y, -Z, W).
	const FQuat Rotation = Conversion.ConvertRotation(FQuat(0.1f, 0.2f, 0.3f, 0.9f));
	TestTrue("Rotation", Rotation.X == -0.1f && Rotation.Y == 0.2f && Rotation.Z == -0.3f && Rotation.W == 0.9f);

	// A right-handed Y-up point (x, y, z) is (x, -z, y) in this frame: both conversions put it in the same place.
	const FImportCoordinateConversion YUp(EImportAxes::RightHandedYUp, 2.54f);
	FRandomStream Random(0x2a);
	for (int32 Index = 0; Index < 50; ++Index)
	{
		const FVector P = RandomVector(Random, 100.0f);
		if (!SameBits(Conversion.ConvertPosition(FVector(P.X, -P.Z, P.Y)), YUp.ConvertPosition(P)))
		{
			AddError(FString::Printf("Point %d lands elsewhere through the Z-up frame", Index));
			break;
		}
	}

	// Rotations, tangent frames and matrices do to converted data what they did to the source.
	const FMatrix Basis = Conversion.GetBasisMatrix();
	for (int32 Index = 0; Index < 20; ++Index)
	{
		const FQuat Q = RandomRotation(Random);
		const FVector V = RandomVector(Random, 1.0f);
		TestTrue(*FString::Printf("Rotation %d", Index),
			Conversion.ConvertRotation(Q)
				.RotateVector(Conversion.ConvertDirection(V))
				.Equals(Conversion.ConvertDirection(Q.RotateVector(V)), 1.0e-5f));

		const FVector N = Random.GetUnitVector();
		const FVector T = (N ^ Random.GetUnitVector()).GetSafeNormal();
		const float W = (Index % 2 == 0) ? 1.0f : -1.0f;
		const FVector4 Converted = Conversion.ConvertTangent(FVector4(T.X, T.Y, T.Z, W));
		const FVector Bitangent = (Conversion.ConvertDirection(N) ^ FVector(Converted)) * Converted.W;
		TestTrue(*FString::Printf("Bitangent %d", Index),
			Bitangent.Equals(Conversion.ConvertDirection((N ^ T) * W), 1.0e-5f));

		const FMatrix M = RandomAffine(Random);
		const FVector P = RandomVector(Random, 3.0f);
		TestTrue(*FString::Printf("Matrix %d on points", Index),
			FVector(Conversion.ConvertMatrix(M).TransformPosition(Conversion.ConvertPosition(P)))
				.Equals(Conversion.ConvertPosition(FVector(M.TransformPosition(P))), 1.0e-3f));
		TestTrue(*FString::Printf("Matrix %d is B^-1 M B", Index),
			Conversion.ConvertMatrix(M).Equals(Basis.Inverse() * M * Basis, 1.0e-4f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImportCoordinateConversionWindingTest,
	"System.MeshUtilities.ImportCoordinateConversion.WindingAndNormals",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FImportCoordinateConversionWindingTest::RunTest(const FString& Parameters)
{
	// The rule: the index order is kept, and since det B = -1, cross(E1, E2) . N changes sign. The cube's triangles
	// are counter-clockwise around their outward normals in the source (cross along N); converted, cross(E1, E2) is
	// -B(cross) * UnitsToCm^2, against the converted normal, for every triangle and for both bases.
	const FMeshData Source = LoadCubeFixtureSourceSpace();
	if (!TestEqual("Cube triangles", Source.Indices.Num(), 36))
	{
		return false;
	}
	const int32 Triangles = Source.Indices.Num() / 3;
	for (int32 Triangle = 0; Triangle < Triangles; ++Triangle)
	{
		if ((TriangleCross(Source, Triangle) | TriangleNormal(Source, Triangle)) <= 0.0f)
		{
			AddError(FString::Printf("Source triangle %d is not counter-clockwise around its normal", Triangle));
			return false;
		}
	}

	const FImportCoordinateConversion Conversions[] = {FImportCoordinateConversion(EImportAxes::RightHandedYUp, 100.0f),
		FImportCoordinateConversion(EImportAxes::RightHandedZUp, 1.0f)};
	for (const FImportCoordinateConversion& Conversion : Conversions)
	{
		const TCHAR* Name = Conversion.GetSourceAxes() == EImportAxes::RightHandedYUp ? "Y up" : "Z up";
		FMeshData Converted = Source;
		Conversion.ConvertMeshData(Converted);
		TestTrue(*FString::Printf("%s: indices kept", Name), Converted.Indices == Source.Indices);
		const float AreaScale = Conversion.GetUnitsToCm() * Conversion.GetUnitsToCm();
		for (int32 Triangle = 0; Triangle < Triangles; ++Triangle)
		{
			const FVector Cross = TriangleCross(Converted, Triangle);
			const FVector Expected = -Conversion.ConvertDirection(TriangleCross(Source, Triangle)) * AreaScale;
			const float Before = TriangleCross(Source, Triangle) | TriangleNormal(Source, Triangle);
			const float After = Cross | TriangleNormal(Converted, Triangle);
			if (!Cross.Equals(Expected, 1.0e-3f * AreaScale) || FMath::Sign(After) != -FMath::Sign(Before))
			{
				AddError(FString::Printf("%s: triangle %d does not flip against its normal", Name, Triangle));
				break;
			}
		}
	}

	// Any triangle, not just the cube's: the sign of cross(E1, E2) . N flips.
	FRandomStream Random(7);
	for (const FImportCoordinateConversion& Conversion : Conversions)
	{
		for (int32 Index = 0; Index < 100; ++Index)
		{
			FMeshData Triangle;
			const FVector Normal = Random.GetUnitVector();
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				Triangle.Vertices.Add(FVertex(RandomVector(Random, 1.0f), Normal, FVector2D::ZeroVector));
			}
			Triangle.Indices = {0, 1, 2};
			const float Before = TriangleCross(Triangle, 0) | Normal;
			Conversion.ConvertMeshData(Triangle);
			const float After = TriangleCross(Triangle, 0) | Triangle.Vertices[0].Normal;
			if (FMath::Abs(Before) > 1.0e-3f && FMath::Sign(After) != -FMath::Sign(Before))
			{
				AddError(FString::Printf("Random triangle %d keeps its side", Index));
				break;
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImportCoordinateConversionSkeletalTest,
	"System.MeshUtilities.ImportCoordinateConversion.SkeletalConjugation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FImportCoordinateConversionSkeletalTest::RunTest(const FString& Parameters)
{
	// Skinned in the engine world (vertex * InverseBind * BoneWorld), a converted mesh lands on the converted source
	// skinning, for the bind pose and every frame of the clip; the bounds are the converted vertices' bounds.
	FRandomStream Random(99);
	const FImportCoordinateConversion Conversions[] = {FImportCoordinateConversion(EImportAxes::RightHandedYUp, 100.0f),
		FImportCoordinateConversion(EImportAxes::RightHandedZUp, 2.54f)};
	for (const FImportCoordinateConversion& Conversion : Conversions)
	{
		const TCHAR* Name = Conversion.GetSourceAxes() == EImportAxes::RightHandedYUp ? "Y up" : "Z up";
		constexpr int32 Bones = 3;
		constexpr int32 Frames = 4;
		FSkeletalMeshData Source;
		Source.Skeleton.BoneNames = {FName("Root"), FName("Spine"), FName("Head")};
		Source.Skeleton.ParentIndices = {INDEX_NONE, 0, 1};
		for (int32 Bone = 0; Bone < Bones; ++Bone)
		{
			Source.Skeleton.InverseBindPose.Add(RandomAffine(Random));
		}
		Source.EmbeddedAnim.LocalPoseFrames.SetNum(Frames);
		for (TArray<FMatrix>& Frame : Source.EmbeddedAnim.LocalPoseFrames)
		{
			for (int32 Bone = 0; Bone < Bones; ++Bone)
			{
				Frame.Add(RandomAffine(Random));
			}
		}
		Source.LocalMin = FVector(TNumericLimits<float>::Max());
		Source.LocalMax = FVector(TNumericLimits<float>::Lowest());
		for (int32 Index = 0; Index < 8; ++Index)
		{
			FSkeletalVertex Vertex;
			Vertex.Position = RandomVector(Random, 2.0f);
			Vertex.Normal = Random.GetUnitVector();
			Vertex.BoneIndices = FIntVector4(Index % Bones);
			Vertex.BoneWeights = FVector4(1.0f, 0.0f, 0.0f, 0.0f);
			Source.LocalMin = Source.LocalMin.ComponentMin(Vertex.Position);
			Source.LocalMax = Source.LocalMax.ComponentMax(Vertex.Position);
			Source.Vertices.Add(Vertex);
			Source.Indices.Add(static_cast<uint32>(Index));
		}

		FSkeletalMeshData Converted = Source;
		Conversion.ConvertSkeletalMeshData(Converted);

		FVector Min(TNumericLimits<float>::Max());
		FVector Max(TNumericLimits<float>::Lowest());
		for (const FSkeletalVertex& Vertex : Converted.Vertices)
		{
			Min = Min.ComponentMin(Vertex.Position);
			Max = Max.ComponentMax(Vertex.Position);
		}
		TestTrue(*FString::Printf("%s: bounds", Name),
			Converted.LocalMin.Equals(Min, 1.0e-3f) && Converted.LocalMax.Equals(Max, 1.0e-3f));

		const float Tolerance = 1.0e-4f * Conversion.GetUnitsToCm() * 100.0f;
		for (int32 Index = 0; Index < Source.Vertices.Num(); ++Index)
		{
			const int32 Bone = Source.Vertices[Index].BoneIndices.X;
			const FVector& SourcePosition = Source.Vertices[Index].Position;
			const FVector& ConvertedPosition = Converted.Vertices[Index].Position;
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				const FMatrix SourceSkin =
					Source.Skeleton.InverseBindPose[Bone] * Source.EmbeddedAnim.LocalPoseFrames[Frame][Bone];
				const FMatrix ConvertedSkin =
					Converted.Skeleton.InverseBindPose[Bone] * Converted.EmbeddedAnim.LocalPoseFrames[Frame][Bone];
				const FVector Expected =
					Conversion.ConvertPosition(FVector(SourceSkin.TransformPosition(SourcePosition)));
				const FVector Actual(ConvertedSkin.TransformPosition(ConvertedPosition));
				if (!Actual.Equals(Expected, Tolerance))
				{
					AddError(FString::Printf("%s: vertex %d, frame %d skins elsewhere", Name, Index, Frame));
					return false;
				}
			}
		}

		// A clip imported on its own converts the same way.
		UAnimSequence Clip = Source.EmbeddedAnim;
		Conversion.ConvertAnimSequence(Clip);
		TestTrue(*FString::Printf("%s: clip", Name),
			Clip.LocalPoseFrames[Frames - 1][Bones - 1] ==
				Converted.EmbeddedAnim.LocalPoseFrames[Frames - 1][Bones - 1]);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
