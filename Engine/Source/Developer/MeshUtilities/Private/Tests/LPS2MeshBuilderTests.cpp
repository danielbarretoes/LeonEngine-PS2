#include "CoreMinimal.h"
#include "LPS2Mesh.h"
#include "LPS2MeshBuilder.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "Primitives.h"
#include "RHIDeferredRelease.h"

#if WITH_DEV_AUTOMATION_TESTS

// The LPS2 v2 build (Docs/ASSET_FORMATS.md, Docs/PLANS/ps2-shipping.md N12): quantization, strips that draw exactly
// the source's triangles with their winding, batches within VU1's budget, and the same bytes every time.

namespace
{

	/** A corner as LPS2 v2 quantizes it: position, normal and 4.12 texture coordinates (without a batch's offset). */
	struct FCornerKey
	{
		int32 Values[8] = {0, 0, 0, 0, 0, 0, 0, 0};

		[[nodiscard]] bool operator==(const FCornerKey& Other) const
		{
			return FMemory::Memcmp(Values, Other.Values, sizeof(Values)) == 0;
		}
		[[nodiscard]] bool operator<(const FCornerKey& Other) const
		{
			for (int32 Index = 0; Index < 8; ++Index)
			{
				if (Values[Index] != Other.Values[Index])
				{
					return Values[Index] < Other.Values[Index];
				}
			}
			return false;
		}
	};

	/** A triangle as its three corners in its winding, starting from the smallest (the same triangle, the same key). */
	struct FTriangleKey
	{
		FCornerKey Corners[3];

		FTriangleKey(const FCornerKey& A, const FCornerKey& B, const FCornerKey& C)
		{
			const FCornerKey Input[3] = {A, B, C};
			int32 First = 0;
			for (int32 Index = 1; Index < 3; ++Index)
			{
				First = Input[Index] < Input[First] ? Index : First;
			}
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Corners[Index] = Input[(First + Index) % 3];
			}
		}

		[[nodiscard]] bool operator==(const FTriangleKey& Other) const
		{
			return Corners[0] == Other.Corners[0] && Corners[1] == Other.Corners[1] && Corners[2] == Other.Corners[2];
		}
		[[nodiscard]] bool operator<(const FTriangleKey& Other) const
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (!(Corners[Index] == Other.Corners[Index]))
				{
					return Corners[Index] < Other.Corners[Index];
				}
			}
			return false;
		}
	};

	/** A source corner quantized as the format says, with the mesh's scale and bias. */
	FCornerKey QuantizeSource(const FVertex& Vertex, const FLPS2MeshHeader& Header)
	{
		FCornerKey Key;
		const FVector Normal = Vertex.Normal.GetSafeNormal();
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Key.Values[Axis] = FMath::Clamp(
				FMath::RoundToInt((Vertex.Position[Axis] - Header.PositionBias[Axis]) / Header.PositionScale[Axis]),
				-FLPS2Mesh::PositionRange, FLPS2Mesh::PositionRange);
			Key.Values[3 + Axis] = FMath::Clamp(FMath::RoundToInt(Normal[Axis] * float(FLPS2Mesh::NormalRange)),
				-FLPS2Mesh::NormalRange, FLPS2Mesh::NormalRange);
		}
		Key.Values[6] = FMath::RoundToInt(Vertex.TexCoord.X * float(FLPS2Mesh::TexCoordOne));
		Key.Values[7] = FMath::RoundToInt(Vertex.TexCoord.Y * float(FLPS2Mesh::TexCoordOne));
		return Key;
	}

	/** The source's triangles of a section, less those whose corners quantize to the same vertex. */
	TArray<FTriangleKey> SourceTriangles(const FMeshData& Source, const FMeshSection& Section, const FLPS2Mesh& Mesh)
	{
		TArray<FTriangleKey> Triangles;
		for (int32 Index = Section.IndexOffset; Index + 2 < Section.IndexOffset + Section.IndexCount; Index += 3)
		{
			const FCornerKey A = QuantizeSource(Source.Vertices[int32(Source.Indices[Index])], Mesh.GetHeader());
			const FCornerKey B = QuantizeSource(Source.Vertices[int32(Source.Indices[Index + 1])], Mesh.GetHeader());
			const FCornerKey C = QuantizeSource(Source.Vertices[int32(Source.Indices[Index + 2])], Mesh.GetHeader());
			if (!(A == B) && !(B == C) && !(A == C))
			{
				Triangles.Add(FTriangleKey(A, B, C));
			}
		}
		Triangles.Sort();
		return Triangles;
	}

	/** A batch vertex's key: the saved values, the texture coordinates with the batch's offset. */
	FCornerKey DrawnCorner(const FLPS2Mesh& Mesh, const FLPS2Batch& Batch, int32 Vertex)
	{
		FCornerKey Key;
		const int16* Position = Mesh.GetPositions(Batch) + (Vertex * 3);
		const int8* Normal = Mesh.GetNormals(Batch) + (Vertex * 4);
		const int16* TexCoord = Mesh.GetTexCoords(Batch) + (Vertex * 2);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Key.Values[Axis] = Position[Axis];
			Key.Values[3 + Axis] = Normal[Axis];
		}
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			Key.Values[6 + Axis] = int32(TexCoord[Axis]) + (int32(Batch.TexCoordOffset[Axis]) * FLPS2Mesh::TexCoordOne);
		}
		return Key;
	}

	/** The triangles a section's strips draw, as the GS and the emitter read them. */
	TArray<FTriangleKey> DrawnTriangles(const FLPS2Mesh& Mesh, const FLPS2Section& Section)
	{
		TArray<FTriangleKey> Triangles;
		for (uint32 BatchIndex = Section.FirstBatch; BatchIndex < Section.FirstBatch + Section.NumBatches; ++BatchIndex)
		{
			const FLPS2Batch& Batch = Mesh.GetBatch(int32(BatchIndex));
			const int8* Normals = Mesh.GetNormals(Batch);
			for (int32 Vertex = 2; Vertex < int32(Batch.NumVertices); ++Vertex)
			{
				const uint8 Flags = uint8(Normals[(Vertex * 4) + 3]);
				if ((Flags & FLPS2Mesh::FlagNoKick) != 0)
				{
					continue;
				}
				const bool bReversed = (Flags & FLPS2Mesh::FlagReversed) != 0;
				Triangles.Add(FTriangleKey(DrawnCorner(Mesh, Batch, bReversed ? Vertex - 1 : Vertex - 2),
					DrawnCorner(Mesh, Batch, bReversed ? Vertex - 2 : Vertex - 1), DrawnCorner(Mesh, Batch, Vertex)));
			}
		}
		Triangles.Sort();
		return Triangles;
	}

	/** A Size x Size grid of quads in the XY plane, 10 cm each, its texture repeating once a quad. */
	FMeshData MakeGrid(int32 Size)
	{
		FMeshData Data;
		for (int32 Y = 0; Y <= Size; ++Y)
		{
			for (int32 X = 0; X <= Size; ++X)
			{
				Data.Vertices.Add(FVertex(FVector(float(X) * 10.0f, float(Y) * 10.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f),
					FVector2D(float(X), float(Y))));
			}
		}
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const uint32 Corner = uint32((Y * (Size + 1)) + X);
				const uint32 Right = Corner + 1;
				const uint32 Up = Corner + uint32(Size + 1);
				Data.Indices.Append({Corner, Right, Up + 1, Corner, Up + 1, Up});
			}
		}
		return Data;
	}

	/** Two sections (the cube's first two faces and the rest) with slots 1 and 0. */
	FMeshData MakeTwoSectionCube()
	{
		FMeshData Data = MakeCube();
		Data.Submeshes.Add(FMeshSection{0, 12, 1});
		Data.Submeshes.Add(FMeshSection{12, Data.Indices.Num() - 12, 0});
		return Data;
	}

	/** The meshes the tests build: curved, long strips with texture coordinates over 40 repeats, two sections. */
	TArray<FMeshData> MakeTestMeshes()
	{
		TArray<FMeshData> Meshes;
		Meshes.Add(MakeSphere(24, 16));
		Meshes.Add(MakeGrid(40));
		Meshes.Add(MakeTwoSectionCube());
		return Meshes;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLPS2StripsCoverTheSourceTest, "System.MeshUtilities.LPS2.StripsCoverTheSource",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLPS2StripsCoverTheSourceTest::RunTest(const FString& Parameters)
{
	// Each section's strips draw the source's triangles, each once and wound as in the source (back face culling
	// depends on it), with the material slot of the source's section.
	for (const FMeshData& Source : MakeTestMeshes())
	{
		FLPS2Mesh Mesh;
		FString Error;
		if (!TestTrue("Built", FLPS2MeshBuilder::Build(Source, Mesh, Error)))
		{
			AddError(Error);
			continue;
		}
		TArray<FMeshSection> Sections = Source.Submeshes;
		if (Sections.Num() == 0)
		{
			Sections.Add(FMeshSection{0, Source.Indices.Num(), 0});
		}
		if (!TestEqual("Sections", Mesh.GetNumSections(), Sections.Num()))
		{
			continue;
		}
		int32 NumTriangles = 0;
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			const FLPS2Section& Section = Mesh.GetSection(SectionIndex);
			const TArray<FTriangleKey> Expected = SourceTriangles(Source, Sections[SectionIndex], Mesh);
			const TArray<FTriangleKey> Drawn = DrawnTriangles(Mesh, Section);
			TestEqual("The section's triangles", Drawn.Num(), Expected.Num());
			TestTrue("The same triangles, the same winding", Drawn == Expected);
			TestEqual("The section's count", int32(Section.NumTriangles), Expected.Num());
			TestEqual("The section's slot", int32(Section.MaterialIndex), Sections[SectionIndex].MaterialIndex);
			NumTriangles += Expected.Num();
		}
		TestEqual("The mesh's triangles", Mesh.GetNumTriangles(), NumTriangles);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLPS2BatchBudgetTest, "System.MeshUtilities.LPS2.BatchesWithinTheVUBudget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLPS2BatchBudgetTest::RunTest(const FString& Parameters)
{
	// A batch fits half of VU1's data memory with its GIF output (FLPS2Mesh::MaxBatchVertices), starts a strip (its
	// first two vertices close nothing), and its streams start on quadwords, as VIF UNPACK takes them.
	constexpr int32 BufferQuadwords = 496;
	for (const FMeshData& Source : MakeTestMeshes())
	{
		FLPS2Mesh Mesh;
		FString Error;
		if (!TestTrue("Built", FLPS2MeshBuilder::Build(Source, Mesh, Error)))
		{
			continue;
		}
		TestTrue("A whole blob", FLPS2Mesh::IsValidBlob(Mesh.GetData()));
		for (int32 BatchIndex = 0; BatchIndex < Mesh.GetNumBatches(); ++BatchIndex)
		{
			const FLPS2Batch& Batch = Mesh.GetBatch(BatchIndex);
			const int32 NumVertices = int32(Batch.NumVertices);
			TestTrue("3 to 64 vertices", NumVertices >= 3 && NumVertices <= FLPS2Mesh::MaxBatchVertices);
			TestTrue("Input and output in a VU1 buffer", 2 + 1 + (NumVertices * (4 + 3)) <= BufferQuadwords);
			TestTrue("On a quadword", (Batch.DataOffset % 16) == 0);
			const int8* Normals = Mesh.GetNormals(Batch);
			TestTrue("The batch starts a strip",
				(uint8(Normals[3]) & FLPS2Mesh::FlagNoKick) != 0 && (uint8(Normals[7]) & FLPS2Mesh::FlagNoKick) != 0);
			TestTrue("A whole offset",
				Batch.TexCoordOffset[0] == FMath::RoundToFloat(Batch.TexCoordOffset[0]) &&
					Batch.TexCoordOffset[1] == FMath::RoundToFloat(Batch.TexCoordOffset[1]));
			// Every vertex inside the batch's sphere.
			const FVector Center(Batch.BoundsCenter[0], Batch.BoundsCenter[1], Batch.BoundsCenter[2]);
			for (int32 Vertex = 0; Vertex < NumVertices; ++Vertex)
			{
				const FVector Position = Mesh.DequantizePosition(Mesh.GetPositions(Batch) + (Vertex * 3));
				if (FVector::Dist(Position, Center) > Batch.BoundsRadius)
				{
					AddError(FString::Printf("Batch %d: vertex %d outside its sphere", BatchIndex, Vertex));
					break;
				}
			}
		}
	}
	// The 40 x 40 grid: its strips are longer than a batch and its texture coordinates span 40 repeats.
	FLPS2Mesh Grid;
	FString Error;
	if (TestTrue("The grid", FLPS2MeshBuilder::Build(MakeGrid(40), Grid, Error)))
	{
		TestTrue("Several batches", Grid.GetNumBatches() > 3);
		for (int32 BatchIndex = 0; BatchIndex < Grid.GetNumBatches(); ++BatchIndex)
		{
			const FLPS2Batch& Batch = Grid.GetBatch(BatchIndex);
			const int16* TexCoords = Grid.GetTexCoords(Batch);
			for (uint32 Vertex = 0; Vertex < Batch.NumVertices * 2; ++Vertex)
			{
				if (FMath::Abs(int32(TexCoords[Vertex])) > 8 * FLPS2Mesh::TexCoordOne)
				{
					AddError(FString::Printf("Batch %d: a texture coordinate beyond +-8 repeats", BatchIndex));
					break;
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLPS2QuantizationTest, "System.MeshUtilities.LPS2.QuantizationError",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLPS2QuantizationTest::RunTest(const FString& Parameters)
{
	// A mesh 655.34 m long (the most int16 spreads at 1 cm a step), and irregular: every drawn position within 0.5 cm
	// of its source vertex, every normal within 1/127 a component, the texture coordinates within 1/8192.
	FMeshData Data;
	uint32 Seed = 12345;
	const auto Random = [&Seed]()
	{
		Seed = (Seed * 1664525u) + 1013904223u;
		return float(Seed >> 8) / float(1u << 24);
	};
	for (int32 Index = 0; Index < 300; ++Index)
	{
		const FVector Position((Random() - 0.5f) * 65534.0f, (Random() - 0.5f) * 4000.0f, Random() * 300.0f);
		const FVector Normal = FVector(Random() - 0.5f, Random() - 0.5f, Random() - 0.5f).GetSafeNormal();
		Data.Vertices.Add(FVertex(Position, Normal, FVector2D(Random() * 4.0f, Random() * 4.0f)));
	}
	Data.Vertices[0].Position.X = -32767.0f;
	Data.Vertices[1].Position.X = 32767.0f;
	for (uint32 Index = 0; Index + 2 < 300; Index += 3)
	{
		Data.Indices.Append({Index, Index + 1, Index + 2});
	}
	FLPS2Mesh Mesh;
	FString Error;
	if (!TestTrue("Built", FLPS2MeshBuilder::Build(Data, Mesh, Error)))
	{
		return false;
	}
	TestEqual("A step of 1 cm along X", Mesh.GetHeader().PositionScale[0], 1.0f, 1.0e-6f);
	float WorstPosition = 0.0f;
	float WorstNormal = 0.0f;
	float WorstTexCoord = 0.0f;
	for (int32 BatchIndex = 0; BatchIndex < Mesh.GetNumBatches(); ++BatchIndex)
	{
		const FLPS2Batch& Batch = Mesh.GetBatch(BatchIndex);
		for (int32 Vertex = 0; Vertex < int32(Batch.NumVertices); ++Vertex)
		{
			const FVector Position = Mesh.DequantizePosition(Mesh.GetPositions(Batch) + (Vertex * 3));
			const FVector Normal = FLPS2Mesh::DequantizeNormal(Mesh.GetNormals(Batch) + (Vertex * 4));
			const FVector2D TexCoord = FLPS2Mesh::DequantizeTexCoord(Mesh.GetTexCoords(Batch) + (Vertex * 2), Batch);
			// Its source: the nearest vertex (the random ones are far apart).
			const FVertex* Source = nullptr;
			for (const FVertex& Candidate : Data.Vertices)
			{
				if (Source == nullptr ||
					FVector::DistSquared(Candidate.Position, Position) <
						FVector::DistSquared(Source->Position, Position))
				{
					Source = &Candidate;
				}
			}
			const FVector Delta = (Position - Source->Position).GetAbs();
			WorstPosition = FMath::Max(WorstPosition, FMath::Max3(Delta.X, Delta.Y, Delta.Z));
			const FVector NormalDelta = (Normal - Source->Normal).GetAbs();
			WorstNormal = FMath::Max(WorstNormal, FMath::Max3(NormalDelta.X, NormalDelta.Y, NormalDelta.Z));
			WorstTexCoord = FMath::Max(WorstTexCoord,
				FMath::Max(FMath::Abs(TexCoord.X - Source->TexCoord.X), FMath::Abs(TexCoord.Y - Source->TexCoord.Y)));
		}
	}
	UE_LOG(LogTemp, Display, "%s",
		*FString::Printf("LPS2 v2 quantization: position %.4f cm, normal %.5f, texture coordinate %.6f",
			double(WorstPosition), double(WorstNormal), double(WorstTexCoord)));
	TestTrue("Positions within 0.5 cm", WorstPosition <= 0.5f + 0.004f);
	TestTrue("Normals within half of 1/127", WorstNormal <= (0.5f / 127.0f) + 1.0e-5f);
	TestTrue("Texture coordinates within half of 1/4096", WorstTexCoord <= (0.5f / 4096.0f) + 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLPS2DeterministicTest, "System.MeshUtilities.LPS2.Deterministic",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLPS2DeterministicTest::RunTest(const FString& Parameters)
{
	// The same source gives the same bytes (gate G5 reimports rely on it), and the cube gives the bytes it gave when
	// LPS2 v2 was made: 6 strips of 4 vertices in one batch.
	for (const FMeshData& Source : MakeTestMeshes())
	{
		FLPS2Mesh First;
		FLPS2Mesh Second;
		FString Error;
		TestTrue("Built twice",
			FLPS2MeshBuilder::Build(Source, First, Error) && FLPS2MeshBuilder::Build(Source, Second, Error));
		TestTrue("The same bytes", First.GetData() == Second.GetData());
	}
	FLPS2Mesh Cube;
	FString Error;
	if (TestTrue("The cube", FLPS2MeshBuilder::Build(MakeCube(), Cube, Error)))
	{
		TestEqual("One batch", Cube.GetNumBatches(), 1);
		TestEqual("24 vertices", Cube.GetNumVertices(), 24);
		TestEqual("Its bytes", Cube.GetData().Num(), 48 + 16 + 32 + FLPS2Mesh::BatchBytes(24));
		TestEqual("Its MD5", FMD5::HashBytes(Cube.GetData().GetData(), uint64(Cube.GetData().Num())),
			FString(TEXT("4e928c0d97f3babd921057c6ac845086")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLPS2SimplifiedLODsTest, "System.MeshUtilities.LPS2.SimplifiedLODs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLPS2SimplifiedLODsTest::RunTest(const FString& Parameters)
{
	// A LOD's source (N15): meshopt_simplify takes each section down to about its share of the triangles, keeps the
	// sections and their slots and the vertices, and gives the same result every time; its LPS2 v2 build is smaller.
	const FMeshData Sphere = MakeSphere(24, 16);
	FMeshData Half;
	FMeshData Quarter;
	FMeshData Again;
	FString Error;
	TestTrue("Simplified",
		FLPS2MeshBuilder::Simplify(Sphere, 0.5f, Half, Error) &&
			FLPS2MeshBuilder::Simplify(Sphere, 0.25f, Quarter, Error) &&
			FLPS2MeshBuilder::Simplify(Sphere, 0.25f, Again, Error));
	const int32 Triangles = Sphere.Indices.Num() / 3;
	TestTrue("About half", Half.Indices.Num() / 3 <= (Triangles / 2) + 8 && Half.Indices.Num() / 3 >= Triangles / 4);
	TestTrue("Fewer still", Quarter.Indices.Num() < Half.Indices.Num());
	TestTrue("The same every time", Quarter.Indices == Again.Indices);
	TestEqual("The vertices kept", Quarter.Vertices.Num(), Sphere.Vertices.Num());
	TestEqual("One section", Quarter.Submeshes.Num(), 1);
	FLPS2Mesh Full;
	FLPS2Mesh Lod;
	TestTrue("Built", FLPS2MeshBuilder::Build(Sphere, Full, Error) && FLPS2MeshBuilder::Build(Quarter, Lod, Error));
	TestTrue("A smaller blob", Lod.GetData().Num() < Full.GetData().Num());
	TestFalse("Nothing to simplify", FLPS2MeshBuilder::Simplify(FMeshData(), 0.5f, Half, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLPS2SkinnedTest, "System.MeshUtilities.LPS2.SkinnedBatchesAndPalettes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLPS2SkinnedTest::RunTest(const FString& Parameters)
{
	// A skinned mesh's blob: batches of at most 48 vertices with a palette of at most 24 bones (VU1's budget with the
	// palette's matrices: 3 + 72 + 8 x 48 <= 496), every vertex's two palette indices naming its own bones and weights,
	// and the strips still drawing the source's triangles. A grid skinned to 40 bones needs several palettes.
	FMeshData Grid = MakeGrid(24);
	TArray<FSkinWeightInfo> Weights;
	for (int32 Index = 0; Index < Grid.Vertices.Num(); ++Index)
	{
		FSkinWeightInfo& Info = Weights.AddDefaulted_GetRef();
		Info.InfluenceBones[0] = uint8((Index / 7) % 40);
		Info.InfluenceBones[1] = uint8(((Index / 7) + 1) % 40);
		Info.InfluenceWeights[0] = uint8(200 - (Index % 50));
		Info.InfluenceWeights[1] = uint8(255 - Info.InfluenceWeights[0]);
	}
	FLPS2Mesh Mesh;
	FString Error;
	if (!TestTrue("Built", FLPS2MeshBuilder::BuildSkinned(Grid, Weights, Mesh, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue("Skinned", Mesh.IsSkinned() && FLPS2Mesh::IsValidBlob(Mesh.GetData()));
	TestTrue("Several palettes", Mesh.GetNumBatches() > 3);
	// Each source vertex's bones and weights, by its quantized corner (the grid's corners are all different).
	TMap<int32, int32> SourceOfCorner;
	for (int32 Index = 0; Index < Grid.Vertices.Num(); ++Index)
	{
		const FCornerKey Key = QuantizeSource(Grid.Vertices[Index], Mesh.GetHeader());
		SourceOfCorner.Add(
			(Key.Values[6] / FLPS2Mesh::TexCoordOne) * 1000 + (Key.Values[7] / FLPS2Mesh::TexCoordOne), Index);
	}
	constexpr int32 BufferQuadwords = 496;
	for (int32 BatchIndex = 0; BatchIndex < Mesh.GetNumBatches(); ++BatchIndex)
	{
		const FLPS2Batch& Batch = Mesh.GetBatch(BatchIndex);
		const int32 NumVertices = int32(Batch.NumVertices);
		const FLPS2SkinPalette& Palette = Mesh.GetPalette(Batch);
		TestTrue("At most 48 vertices", NumVertices >= 3 && NumVertices <= FLPS2Mesh::MaxSkinnedBatchVertices);
		TestTrue("At most 24 bones", Palette.NumBones >= 1 && Palette.NumBones <= uint32(FLPS2Mesh::MaxPaletteBones));
		TestTrue(
			"Within a VU1 buffer", 2 + 1 + (3 * int32(Palette.NumBones)) + (NumVertices * (5 + 3)) <= BufferQuadwords);
		const uint8* Skin = Mesh.GetSkin(Batch);
		for (int32 Vertex = 0; Vertex < NumVertices; ++Vertex)
		{
			const FCornerKey Key = DrawnCorner(Mesh, Batch, Vertex);
			const int32* Source = SourceOfCorner.Find(
				(Key.Values[6] / FLPS2Mesh::TexCoordOne) * 1000 + (Key.Values[7] / FLPS2Mesh::TexCoordOne));
			const uint8* Influence = Skin + (Vertex * 4);
			if (Source == nullptr || Influence[0] >= Palette.NumBones || Influence[1] >= Palette.NumBones ||
				Palette.Bones[Influence[0]] != Weights[*Source].InfluenceBones[0] ||
				Palette.Bones[Influence[1]] != Weights[*Source].InfluenceBones[1] ||
				Influence[2] != Weights[*Source].InfluenceWeights[0] ||
				Influence[3] != Weights[*Source].InfluenceWeights[1])
			{
				AddError(FString::Printf("Batch %d, vertex %d: its skin is not its source's", BatchIndex, Vertex));
				return false;
			}
		}
	}
	TestTrue("The source's triangles",
		DrawnTriangles(Mesh, Mesh.GetSection(0)) ==
			SourceTriangles(Grid, FMeshSection{0, Grid.Indices.Num(), 0}, Mesh));
	FLPS2Mesh Again;
	TestTrue("The same bytes twice",
		FLPS2MeshBuilder::BuildSkinned(Grid, Weights, Again, Error) && Again.GetData() == Mesh.GetData());

	// Weights that do not add up to 255 are refused; so is a weight a vertex short.
	Weights[3].InfluenceWeights[0] = 7;
	TestFalse("Weights not adding up", FLPS2MeshBuilder::BuildSkinned(Grid, Weights, Mesh, Error));
	Weights.Pop();
	TestFalse("A vertex without weights", FLPS2MeshBuilder::BuildSkinned(Grid, Weights, Mesh, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLPS2ErrorsTest, "System.MeshUtilities.LPS2.Errors",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLPS2ErrorsTest::RunTest(const FString& Parameters)
{
	// Nothing to draw or an index out of range builds nothing; a damaged blob is refused.
	FLPS2Mesh Mesh;
	FString Error;
	TestFalse("An empty source", FLPS2MeshBuilder::Build(FMeshData(), Mesh, Error));
	FMeshData OutOfRange = MakeCube();
	OutOfRange.Indices[4] = 999;
	TestFalse("An index out of range", FLPS2MeshBuilder::Build(OutOfRange, Mesh, Error));
	TestTrue("Says so", Error.Contains(TEXT("999")));
	FMeshData Degenerate;
	Degenerate.Vertices.Add(FVertex(FVector::ZeroVector, FVector::UpVector, FVector2D::ZeroVector));
	Degenerate.Indices.Append({0u, 0u, 0u});
	TestFalse("Only degenerate triangles", FLPS2MeshBuilder::Build(Degenerate, Mesh, Error));
	// A face whose texture coordinates span 20 repeats: more than a batch's int16 4.12 coordinates hold (N29). The
	// build fails and says why (it made a blob that is not valid, and LeonCook crashed on it).
	FMeshData WideUVs = MakeCube();
	for (FVertex& Vertex : WideUVs.Vertices)
	{
		Vertex.TexCoord *= 20.0f;
	}
	FLPS2Mesh Wide;
	TestFalse("Texture coordinates over 14 repeats", FLPS2MeshBuilder::Build(WideUVs, Wide, Error));
	TestTrue("Says so", Error.Contains(TEXT("repeats")));
	TestTrue("Builds nothing", Wide.IsEmpty());

	TestTrue("The cube", FLPS2MeshBuilder::Build(MakeCube(), Mesh, Error));
	TArray<uint8> Damaged = Mesh.GetData();
	Damaged[0] = 'X';
	TestFalse("A wrong magic", FLPS2Mesh::IsValidBlob(Damaged));
	Damaged = Mesh.GetData();
	Damaged.SetNum(Damaged.Num() - 16);
	TestFalse("A truncated batch", FLPS2Mesh::IsValidBlob(Damaged));
	FLPS2Mesh Refused;
	TestFalse("SetData refuses it", Refused.SetData(MoveTemp(Damaged)));
	TestTrue("And stays empty", Refused.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLPS2DeferredReleaseTest, "System.MeshUtilities.LPS2.ReleasedAfterTheFramesInFlight",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLPS2DeferredReleaseTest::RunTest(const FString& Parameters)
{
	// The PS2's frame reads a static mesh's blob in place a frame after it is recorded (ps2-shipping N14): a blob a
	// mesh gives up while frames are in flight waits for the frame being recorded to complete (FRHIDeferredRelease);
	// with no frame in flight (the desktop) it goes at once.
	FString Error;
	const TArray<FMeshData> Sources = MakeTestMeshes();
	const auto MakeMesh = [this, &Sources, &Error](FLPS2Mesh& OutMesh)
	{ return TestTrue("Built", FLPS2MeshBuilder::Build(Sources[0], OutMesh, Error)); };
	{
		FLPS2Mesh Idle;
		if (!MakeMesh(Idle))
		{
			return false;
		}
	}
	TestEqual("Nothing in flight: freed at once", FRHIDeferredRelease::GetNumPending(), 0);

	// Frame 4 in flight, frame 5 recorded: a mesh destroyed, one loaded over and one assigned over, and an instance's
	// baked colours reset (N22's colour streams go to VU1 by reference too).
	FRHIDeferredRelease::SetFrameCounters(5, 3);
	FLPS2Mesh Assigned;
	FLPS2Mesh Rebuilt;
	{
		FLPS2Mesh Destroyed;
		if (!MakeMesh(Destroyed) || !MakeMesh(Assigned) || !MakeMesh(Rebuilt))
		{
			FRHIDeferredRelease::SetFrameCounters(0, 0);
			return false;
		}
		const TArray<uint8> Blob = Rebuilt.GetData();
		TestTrue("Its data again", Rebuilt.SetData(TArray<uint8>(Blob)));
		Assigned = FLPS2Mesh();
		FLPS2ColorStreams Colors;
		Colors.Init(Rebuilt);
		TestTrue("Colours for the mesh", Colors.Matches(Rebuilt));
		Colors.Reset();
	}
	TestEqual("Four buffers wait", FRHIDeferredRelease::GetNumPending(), 4);
	TestFalse("The mesh loaded over has its new blob", Rebuilt.IsEmpty());
	TestTrue("The assigned one is empty", Assigned.IsEmpty());
	FRHIDeferredRelease::SetFrameCounters(6, 4);
	TestEqual("Frame 4 done: frame 5's still wait", FRHIDeferredRelease::GetNumPending(), 4);
	FRHIDeferredRelease::SetFrameCounters(6, 5);
	TestEqual("Frame 5 done: freed", FRHIDeferredRelease::GetNumPending(), 0);
	FRHIDeferredRelease::SetFrameCounters(0, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
