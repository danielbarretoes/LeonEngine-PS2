#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "TriangleCollision.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSegmentTriangleTest, "System.PhysicsCore.Triangle.SegmentTriangle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSegmentTriangleTest::RunTest(const FString& Parameters)
{
	// A vertical segment hits a 2 m floor triangle two thirds of the way down, with the normal facing up.
	const FVector V0(-100.0f, -100.0f, 0.0f);
	const FVector V1(100.0f, -100.0f, 0.0f);
	const FVector V2(0.0f, 100.0f, 0.0f);
	float T = 1.0f;
	FVector N = FVector::ZeroVector;
	TestTrue("Hit", SegmentTriangle(FVector(0.0f, 0.0f, 200.0f), FVector(0.0f, 0.0f, -100.0f), V0, V1, V2, T, N));
	TestEqual("Time", T, 2.0f / 3.0f, 1.0e-3f);
	TestTrue("Normal up", N.Z > 0.5f);
	return true;
}

namespace
{

	/** Every triangle's nearest hit, the last one at equal times: the mesh query without its tree (the reference). */
	bool SegmentEveryTriangle(const FVector& Start, const FVector& End, const FTriangleMeshCollision& Mesh,
		float Inflate, float& OutT, FVector& OutNormal)
	{
		bool bAny = false;
		float BestT = 1.0f;
		FVector BestN(0.0f, 0.0f, 1.0f);
		const uint32 VertexCount = static_cast<uint32>(Mesh.Positions.Num());
		for (int32 Tri = 0; Tri < Mesh.Indices.Num() / 3; ++Tri)
		{
			const uint32 I0 = Mesh.Indices[Tri * 3 + 0];
			const uint32 I1 = Mesh.Indices[Tri * 3 + 1];
			const uint32 I2 = Mesh.Indices[Tri * 3 + 2];
			if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
			{
				continue;
			}
			float HitT = 1.0f;
			FVector HitN = FVector::ZeroVector;
			const bool bOk = (Inflate > 1.0e-4f)
				? SegmentTriangleInflated(
					  Start, End, Mesh.Positions[I0], Mesh.Positions[I1], Mesh.Positions[I2], Inflate, HitT, HitN)
				: SegmentTriangle(Start, End, Mesh.Positions[I0], Mesh.Positions[I1], Mesh.Positions[I2], HitT, HitN);
			if (!bOk || HitT > BestT)
			{
				continue;
			}
			BestT = HitT;
			BestN = HitN;
			bAny = true;
		}
		if (bAny)
		{
			OutT = BestT;
			OutNormal = BestN;
		}
		return bAny;
	}

	FVector RandomOffset(const FRandomStream& Random, float Extent)
	{
		return FVector(
			Random.FRandRange(-Extent, Extent), Random.FRandRange(-Extent, Extent), Random.FRandRange(-Extent, Extent));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSegmentTriangleMeshTreeTest,
	"System.PhysicsCore.Triangle.MeshTreeMatchesEveryTriangle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSegmentTriangleMeshTreeTest::RunTest(const FString& Parameters)
{
	// Random meshes (large, tiny and sliver triangles, repeated triangles, a bad index) and segments aimed at them: the
	// tree's query gives the hit of testing every triangle, to the bit, with and without an inflate.
	FRandomStream Random(1616);
	int32 Hits = 0;
	int32 Mismatches = 0;
	for (int32 MeshIndex = 0; MeshIndex < 24; ++MeshIndex)
	{
		FTriangleMeshCollision Mesh;
		const float Size = MeshIndex % 3 == 0 ? 2.0f : (MeshIndex % 3 == 1 ? 60.0f : 600.0f);
		constexpr int32 NumVertices = 48;
		for (int32 Vertex = 0; Vertex < NumVertices; ++Vertex)
		{
			Mesh.Positions.Add(RandomOffset(Random, Size));
		}
		for (int32 Tri = 0; Tri < 40; ++Tri)
		{
			const uint32 I0 = static_cast<uint32>(Random.RandRange(0, NumVertices - 1));
			const uint32 I1 = static_cast<uint32>(Random.RandRange(0, NumVertices - 1));
			uint32 I2 = static_cast<uint32>(Random.RandRange(0, NumVertices - 1));
			if (Tri % 7 == 3)
			{
				// A sliver: the third vertex almost on the first edge.
				Mesh.Positions.Add(Mesh.Positions[static_cast<int32>(I0)] +
					((Mesh.Positions[static_cast<int32>(I1)] - Mesh.Positions[static_cast<int32>(I0)]) * 0.5f) +
					RandomOffset(Random, 1.0e-3f));
				I2 = static_cast<uint32>(Mesh.Positions.Num() - 1);
			}
			Mesh.Indices.Add(I0);
			Mesh.Indices.Add(I1);
			Mesh.Indices.Add(I2);
			if (Tri % 9 == 4)
			{
				// The same triangle again, the other way round (equal times).
				Mesh.Indices.Add(I2);
				Mesh.Indices.Add(I1);
				Mesh.Indices.Add(I0);
			}
		}
		// A bad index.
		Mesh.Indices.Add(0u);
		Mesh.Indices.Add(1u);
		Mesh.Indices.Add(999u);
		Mesh.BuildTree();

		for (int32 Query = 0; Query < 300; ++Query)
		{
			// Half aimed through a triangle's neighbourhood, half anywhere.
			const int32 Tri = Random.RandRange(0, (Mesh.Indices.Num() / 3) - 2);
			const FVector Centroid = (Mesh.Positions[static_cast<int32>(Mesh.Indices[Tri * 3])] +
										 Mesh.Positions[static_cast<int32>(Mesh.Indices[Tri * 3 + 1])] +
										 Mesh.Positions[static_cast<int32>(Mesh.Indices[Tri * 3 + 2])]) /
				3.0f;
			const FVector Aim =
				Query % 2 == 0 ? Centroid + RandomOffset(Random, Size * 0.5f) : RandomOffset(Random, Size);
			const FVector Dir = RandomOffset(Random, 1.0f).GetSafeNormal();
			const float Length = Size * Random.FRandRange(0.5f, 4.0f);
			const FVector Start = Aim - (Dir * Length);
			const FVector End = Aim + (Dir * (Length * Random.FRandRange(0.2f, 1.5f)));
			const float Inflate = Query % 4 == 1 ? Size * 0.05f : (Query % 4 == 3 ? Size * 0.3f : 0.0f);

			float TreeT = -1.0f;
			FVector TreeN = FVector::ZeroVector;
			float EveryT = -1.0f;
			FVector EveryN = FVector::ZeroVector;
			const bool bTree = SegmentTriangleMesh(Start, End, Mesh, Inflate, TreeT, TreeN);
			const bool bEvery = SegmentEveryTriangle(Start, End, Mesh, Inflate, EveryT, EveryN);
			Hits += bEvery ? 1 : 0;
			if (bTree != bEvery || TreeT != EveryT || TreeN != EveryN)
			{
				++Mismatches;
				if (Mismatches <= 5)
				{
					AddError(FString::Printf("Mesh %d query %d: tree %d t=%.9g, every triangle %d t=%.9g", MeshIndex,
						Query, bTree ? 1 : 0, static_cast<double>(TreeT), bEvery ? 1 : 0, static_cast<double>(EveryT)));
				}
			}

			// Capped at the hit's time the query finds the same hit.
			if (bEvery)
			{
				float CappedT = -1.0f;
				FVector CappedN = FVector::ZeroVector;
				const bool bCapped = SegmentTriangleMesh(Start, End, Mesh, Inflate, CappedT, CappedN, EveryT);
				if (!bCapped || CappedT != EveryT || CappedN != EveryN)
				{
					++Mismatches;
				}
			}
		}
	}
	TestEqual("Mismatches", Mismatches, 0);
	TestTrue("Enough hits to mean something", Hits > 1000);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
