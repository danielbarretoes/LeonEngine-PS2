#include "CoreMinimal.h"
#include "ImportCoordinateConversion.h"
#include "Math/RandomStream.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Tests/GltfTestCube.h"
#include "Tests/LegacyCoordinateConversion.h"

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

	/** The Cube fixture as the glTF stores it (right-handed, Y up, metres). */
	FMeshData LoadCubeFixtureSourceSpace()
	{
		return MakeGltfTestCubeSourceSpace();
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
		const FQuat Rotation(V.X, V.Y, V.Z, Random.FRandRange(-1.0f, 1.0f));
		Mismatches += SameBits(Conversion.ConvertPosition(V), FLegacyCoordinateConversion::ConvertPosition(V)) ? 0 : 1;
		Mismatches +=
			SameBits(Conversion.ConvertDirection(V), FLegacyCoordinateConversion::ConvertDirection(V)) ? 0 : 1;
		Mismatches += SameBits(Conversion.ConvertScale(V), FLegacyCoordinateConversion::ConvertScale(V)) ? 0 : 1;
		Mismatches +=
			SameBits(Conversion.ConvertRotation(Rotation), FLegacyCoordinateConversion::ConvertRotation(Rotation)) ? 0
																												   : 1;
	}
	TestEqual("Position, direction, scale and rotation bits", Mismatches, 0);

	// Mesh data: every vertex, bit for bit; the index order is kept.
	FMeshData Import = LoadCubeFixtureSourceSpace();
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImportCoordinateConversionWindingTest,
	"System.MeshUtilities.ImportCoordinateConversion.WindingAndNormals",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FImportCoordinateConversionWindingTest::RunTest(const FString& Parameters)
{
	// The rule: the index order is kept, and since det B = -1, cross(E1, E2) . N changes sign. The cube's triangles
	// are counter-clockwise around their outward normals in the source (cross along N); converted, cross(E1, E2) is
	// -B(cross) * UnitsToCm^2, against the converted normal, for every triangle and every unit.
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
		FImportCoordinateConversion(EImportAxes::RightHandedYUp, 1.0f)};
	for (const FImportCoordinateConversion& Conversion : Conversions)
	{
		const TCHAR* Name = Conversion.GetUnitsToCm() > 1.0f ? "metres" : "centimetres";
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImportCoordinateConversionTransformTest,
	"System.MeshUtilities.ImportCoordinateConversion.TransformsAsMatrices",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FImportCoordinateConversionTransformTest::RunTest(const FString& Parameters)
{
	// A bone's local transform converted component by component is its matrix converted (B^-1 M B), so a chain of
	// converted transforms is the converted chain: a skinned vertex lands where the source's would, converted.
	const FImportCoordinateConversion Conversion(EImportAxes::RightHandedYUp, FImportCoordinateConversion::CmPerMetre);
	FRandomStream Random(99);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		const FTransform Parent(RandomRotation(Random), RandomVector(Random, 2.0f), FVector(1.0f));
		const FTransform Child(RandomRotation(Random), RandomVector(Random, 2.0f), FVector(1.0f));
		const FMatrix Converted = Conversion.ConvertTransform(Child).ToMatrixWithScale();
		TestTrue(*FString::Printf("Transform %d as its matrix", Index),
			Converted.Equals(Conversion.ConvertMatrix(Child.ToMatrixWithScale()), 1.0e-3f));
		const FVector Point = RandomVector(Random, 1.0f);
		const FVector Expected = Conversion.ConvertPosition((Child * Parent).TransformPosition(Point));
		const FVector Actual = (Conversion.ConvertTransform(Child) * Conversion.ConvertTransform(Parent))
								   .TransformPosition(Conversion.ConvertPosition(Point));
		TestTrue(*FString::Printf("Chain %d", Index), Actual.Equals(Expected, 1.0e-2f));
	}
	const FTransform Scaled(FQuat::Identity, FVector(1.0f, 2.0f, 3.0f), FVector(1.0f, 2.0f, 3.0f));
	const FTransform ScaledConverted = Conversion.ConvertTransform(Scaled);
	TestTrue("Translation in cm, (x, z, y)", ScaledConverted.GetTranslation().Equals(FVector(100.0f, 300.0f, 200.0f)));
	TestTrue("Scale reordered", ScaledConverted.GetScale3D().Equals(FVector(1.0f, 3.0f, 2.0f)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
