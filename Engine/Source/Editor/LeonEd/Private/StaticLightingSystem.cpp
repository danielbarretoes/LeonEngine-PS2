#include "StaticLightingSystem.h"

#include "AabbTree.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "LPS2Mesh.h"
#include "LeonEdLog.h"
#include "Math/RandomStream.h"
#include "StaticMeshResources.h"

namespace
{

	/** The seed of the occlusion rays' directions: the same set for every vertex of every bake. */
	constexpr int32 OcclusionRaySeed = 0x4C4E3232;
	/**
	 * A ray starts this far off its vertex along the normal, cm: past the vertex's own faces, and above a decal-thin
	 * piece lying on it (de_leon's 1 cm floor pads), which would otherwise shut out its sky.
	 */
	constexpr float RayStartOffset = 2.0f;
	/**
	 * A hit nearer the ray's start than this does not count, cm: a vertex resting on another surface (a wall's foot on
	 * the ground) starts its rays on that surface's plane, and the rays leaving it must not hit it. The occluders are
	 * closed meshes, so a ray into the ground meets its far side instead.
	 */
	constexpr float MinHitDistance = 0.05f;
	/** A shadow ray to a point light stops this short of it, cm. */
	constexpr float LightEndOffset = 1.0f;
	/** A triangle's box in the tree grows by this, cm (the intersection is exact; the box only culls). */
	constexpr float TriangleBoxMargin = 0.01f;
	/** Most occlusion rays a vertex casts. */
	constexpr int32 MaxOcclusionRays = 1024;

	/** A light the bake adds, in world space. */
	struct FBakeLight
	{
		bool bDirectional = true;
		bool bCastShadows = true;
		/** Directional: the unit direction it travels. */
		FVector Direction = FVector::ZeroVector;
		/** Point: where it is and how far it reaches (cm). */
		FVector Position = FVector::ZeroVector;
		float Radius = 0.0f;
		/** Colour times intensity. */
		FVector Color = FVector::ZeroVector;
	};

	/** The static geometry's triangles in world space and their tree. */
	class FOccluders
	{
	public:
		void AddMesh(const FTriMeshCollisionData& Data, const FTransform& LocalToWorld)
		{
			const int32 FirstVertex = Positions.Num();
			for (const FVector& Vertex : Data.Vertices)
			{
				Positions.Add(LocalToWorld.TransformPosition(Vertex));
			}
			for (int32 Index = 0; Index + 2 < Data.Indices.Num(); Index += 3)
			{
				const int32 A = int32(Data.Indices[Index]);
				const int32 B = int32(Data.Indices[Index + 1]);
				const int32 C = int32(Data.Indices[Index + 2]);
				if (!Data.Vertices.IsValidIndex(A) || !Data.Vertices.IsValidIndex(B) || !Data.Vertices.IsValidIndex(C))
				{
					continue;
				}
				Triangles.Add(FirstVertex + A);
				Triangles.Add(FirstVertex + B);
				Triangles.Add(FirstVertex + C);
			}
		}

		void Build()
		{
			const int32 NumTriangles = GetNumTriangles();
			TArray<FBox> Bounds;
			TArray<int32> Ids;
			Bounds.Reserve(NumTriangles);
			Ids.Reserve(NumTriangles);
			Extent = FBox(ForceInit);
			for (int32 Triangle = 0; Triangle < NumTriangles; ++Triangle)
			{
				FBox Box(ForceInit);
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					Box += Positions[Triangles[(Triangle * 3) + Corner]];
				}
				Extent += Box;
				Bounds.Add(Box.ExpandBy(TriangleBoxMargin));
				Ids.Add(Triangle);
			}
			Tree.Build(Bounds.GetData(), Ids.GetData(), NumTriangles);
		}

		[[nodiscard]] int32 GetNumTriangles() const
		{
			return Triangles.Num() / 3;
		}

		/** The size of the geometry's box, cm: a directional light's shadow ray is this long. */
		[[nodiscard]] float GetExtentSize() const
		{
			return Extent.IsValid ? Extent.GetSize().Size() : 0.0f;
		}

		/** Whether the segment [Start, End] meets a triangle (either side). */
		[[nodiscard]] bool IsOccluded(const FVector& Start, const FVector& End) const
		{
			const FVector Dir = End - Start;
			const float Length = Dir.Size();
			const float MinT = Length > 0.0f ? MinHitDistance / Length : 1.0f;
			bool bHit = false;
			Tree.ForEachSegmentHit(FAabbTreeSegment(Start, End), FVector::ZeroVector, 1.0f,
				[this, &Start, &Dir, MinT, &bHit](int32 Triangle, float MaxT)
				{
					if (bHit)
					{
						return -1.0f;
					}
					const int32 Base = Triangle * 3;
					if (SegmentHitsTriangle(Start, Dir, MinT, Positions[Triangles[Base]],
							Positions[Triangles[Base + 1]], Positions[Triangles[Base + 2]]))
					{
						bHit = true;
						return -1.0f;
					}
					return MaxT;
				});
			return bHit;
		}

	private:
		/** Moller-Trumbore, both sides, T in (MinT, 1] along Dir; a triangle without area is never hit. */
		[[nodiscard]] static bool SegmentHitsTriangle(const FVector& Start, const FVector& Dir, float MinT,
			const FVector& V0, const FVector& V1, const FVector& V2)
		{
			constexpr float MinDeterminant = 1.0e-8f;
			const FVector Edge1 = V1 - V0;
			const FVector Edge2 = V2 - V0;
			const FVector P = Dir ^ Edge2;
			const float Determinant = Edge1 | P;
			if (FMath::Abs(Determinant) < MinDeterminant)
			{
				return false;
			}
			const float Inverse = 1.0f / Determinant;
			const FVector ToStart = Start - V0;
			const float U = (ToStart | P) * Inverse;
			if (U < 0.0f || U > 1.0f)
			{
				return false;
			}
			const FVector Q = ToStart ^ Edge1;
			const float V = (Dir | Q) * Inverse;
			if (V < 0.0f || U + V > 1.0f)
			{
				return false;
			}
			const float T = (Edge2 | Q) * Inverse;
			return T > MinT && T <= 1.0f;
		}

		TArray<FVector> Positions;
		TArray<int32> Triangles;
		FAabbTree Tree;
		FBox Extent = FBox(ForceInit);
	};

	/**
	 * NumRays cosine-weighted directions about +Z: stratified on the height's square, jittered and turned by a fixed
	 * seed's stream.
	 */
	void MakeOcclusionDirections(int32 NumRays, TArray<FVector>& OutDirections)
	{
		OutDirections.Reset();
		FRandomStream Stream(OcclusionRaySeed);
		for (int32 Ray = 0; Ray < NumRays; ++Ray)
		{
			const float U1 = (float(Ray) + Stream.GetFraction()) / float(NumRays);
			const float U2 = Stream.GetFraction();
			const float Radius = FMath::Sqrt(U1);
			const float Phi = 2.0f * PI * U2;
			OutDirections.Add(FVector(Radius * FMath::Cos(Phi), Radius * FMath::Sin(Phi), FMath::Sqrt(1.0f - U1)));
		}
	}

	/** The lights of the level that are not Movable, in the level's order. */
	void GatherLights(const ULevel& Level, TArray<FBakeLight>& OutLights)
	{
		for (const AActor* Actor : Level.Actors)
		{
			if (Actor == nullptr)
			{
				continue;
			}
			for (const UActorComponent* Component : Actor->GetComponents())
			{
				const ULightComponent* Light = Cast<ULightComponent>(Component);
				if (Light == nullptr || Light->Mobility == EComponentMobility::Movable || !Light->IsVisible())
				{
					continue;
				}
				FBakeLight& Baked = OutLights.AddDefaulted_GetRef();
				Baked.bCastShadows = Light->CastShadows != 0;
				Baked.Color = FVector(Light->LightColor.R, Light->LightColor.G, Light->LightColor.B) * Light->Intensity;
				if (const ULocalLightComponent* Local = Cast<ULocalLightComponent>(Light))
				{
					Baked.bDirectional = false;
					Baked.Position = Local->GetComponentTransform().GetLocation();
					Baked.Radius = FMath::Max(Local->AttenuationRadius, 0.1f);
				}
				else
				{
					Baked.Direction = Light->GetDirection().GetSafeNormal();
				}
			}
		}
	}

	/** The Static mesh components of the level, in the level's order and each actor's. */
	void GatherMeshComponents(const ULevel& Level, TArray<UStaticMeshComponent*>& OutComponents)
	{
		for (const AActor* Actor : Level.Actors)
		{
			if (Actor == nullptr)
			{
				continue;
			}
			for (UActorComponent* Component : Actor->GetComponents())
			{
				UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
				if (Mesh != nullptr && FStaticLightingSystem::ReceivesStaticLighting(*Mesh))
				{
					OutComponents.Add(Mesh);
				}
			}
		}
	}

	[[nodiscard]] uint8 ToByte(float Value)
	{
		return uint8(FMath::Clamp(FMath::RoundToInt(Value * 255.0f), 0, 255));
	}

} // namespace

bool FStaticLightingSystem::ReceivesStaticLighting(const UStaticMeshComponent& Component)
{
	return Component.Mobility == EComponentMobility::Static && Component.HasValidMesh() && Component.ShouldRender();
}

FStaticLightingStats FStaticLightingSystem::Build(UWorld& World)
{
	FStaticLightingStats Stats;
	if (World.PersistentLevel == nullptr)
	{
		return Stats;
	}
	const uint64 StartCycles = FPlatformTime::Cycles64();
	const ULevel& Level = *World.PersistentLevel;
	const AWorldSettings* WorldSettings = World.GetWorldSettings();
	const FLightmassWorldInfoSettings Settings =
		WorldSettings != nullptr ? WorldSettings->LightmassSettings : FLightmassWorldInfoSettings();
	const FVector Environment = Settings.GetEnvironmentLight();
	const int32 NumOcclusionRays =
		Settings.bUseAmbientOcclusion ? FMath::Clamp(Settings.NumOcclusionRays, 1, MaxOcclusionRays) : 0;
	const float OcclusionDistance = FMath::Max(Settings.MaxOcclusionDistance, 0.0f);

	TArray<UStaticMeshComponent*> Components;
	GatherMeshComponents(Level, Components);
	TArray<FBakeLight> Lights;
	GatherLights(Level, Lights);

	// The occluders: what receives the light, when it casts shadows.
	FOccluders Occluders;
	for (const UStaticMeshComponent* Component : Components)
	{
		if (Component->CastShadow)
		{
			Occluders.AddMesh(Component->GetStaticMesh()->GetPhysicsTriMeshData(), Component->GetComponentTransform());
		}
	}
	Occluders.Build();
	const float ShadowRayLength = Occluders.GetExtentSize() + 100.0f;

	TArray<FVector> Directions;
	MakeOcclusionDirections(NumOcclusionRays, Directions);

	for (UStaticMeshComponent* Component : Components)
	{
		const FLPS2Mesh& Mesh = Component->GetStaticMesh()->GetLODResources().RenderData;
		const FMatrix LocalToWorld = Component->GetComponentTransform().ToMatrixWithScale();
		const FMatrix NormalMatrix = LocalToWorld.TransposeAdjoint();
		const float NormalSign = LocalToWorld.Determinant() < 0.0f ? -1.0f : 1.0f;
		FLPS2ColorStreams Colors;
		Colors.Init(Mesh);
		for (int32 BatchIndex = 0; BatchIndex < Mesh.GetNumBatches(); ++BatchIndex)
		{
			const FLPS2Batch& Batch = Mesh.GetBatch(BatchIndex);
			const int16* Positions = Mesh.GetPositions(Batch);
			const int8* Normals = Mesh.GetNormals(Batch);
			const uint8* MeshColors = Mesh.GetColors(Batch);
			uint8* Baked = Colors.GetBatchColors(Mesh, BatchIndex);
			for (int32 Index = 0; Index < int32(Batch.NumVertices); ++Index)
			{
				const FVector4 World4 =
					LocalToWorld.TransformPosition(Mesh.DequantizePosition(Positions + (Index * 3)));
				const FVector Position(World4.X, World4.Y, World4.Z);
				const FVector4 Normal4 =
					NormalMatrix.TransformVector(FLPS2Mesh::DequantizeNormal(Normals + (Index * 4)));
				const FVector Normal = (FVector(Normal4.X, Normal4.Y, Normal4.Z) * NormalSign).GetSafeNormal();
				const FVector Start = Position + (Normal * RayStartOffset);

				// The sky, occluded within the distance.
				float Visibility = 1.0f;
				if (NumOcclusionRays > 0)
				{
					FVector TangentX;
					FVector TangentY;
					Normal.FindBestAxisVectors(TangentX, TangentY);
					int32 NumOpen = 0;
					for (const FVector& Local : Directions)
					{
						const FVector Direction = (TangentX * Local.X) + (TangentY * Local.Y) + (Normal * Local.Z);
						NumOpen += Occluders.IsOccluded(Start, Start + (Direction * OcclusionDistance)) ? 0 : 1;
					}
					Stats.NumRays += NumOcclusionRays;
					Visibility = float(NumOpen) / float(NumOcclusionRays);
				}
				FVector Light = Environment * Visibility;

				// The lights, shadowed by the static geometry.
				for (const FBakeLight& Source : Lights)
				{
					if (Source.bDirectional)
					{
						const float NdL = Normal | -Source.Direction;
						if (NdL <= 0.0f)
						{
							continue;
						}
						if (Source.bCastShadows)
						{
							++Stats.NumRays;
							if (Occluders.IsOccluded(Start, Start - (Source.Direction * ShadowRayLength)))
							{
								continue;
							}
						}
						Light += Source.Color * NdL;
						continue;
					}
					const FVector ToLight = Source.Position - Position;
					const float Distance = ToLight.Size();
					if (Distance >= Source.Radius || Distance <= 0.0f)
					{
						continue;
					}
					const float NdL = Normal | (ToLight / Distance);
					if (NdL <= 0.0f)
					{
						continue;
					}
					if (Source.bCastShadows && Distance > LightEndOffset)
					{
						++Stats.NumRays;
						const FVector End = Source.Position - ((ToLight / Distance) * LightEndOffset);
						if (Occluders.IsOccluded(Start, End))
						{
							continue;
						}
					}
					Light += Source.Color * (NdL * FMath::Square(1.0f - (Distance / Source.Radius)));
				}

				// The mesh's own colour, lit; its alpha as it is.
				const uint8* Own = MeshColors + (Index * 4);
				uint8* Out = Baked + (Index * 4);
				constexpr float InverseByte = 1.0f / 255.0f;
				Out[0] = ToByte(Light.X * float(Own[0]) * InverseByte);
				Out[1] = ToByte(Light.Y * float(Own[1]) * InverseByte);
				Out[2] = ToByte(Light.Z * float(Own[2]) * InverseByte);
				Out[3] = Own[3];
			}
			Stats.NumVertices += int32(Batch.NumVertices);
		}
		Component->SetBakedVertexColors(MoveTemp(Colors));
		++Stats.NumMeshes;
	}
	Stats.NumOccluders = Occluders.GetNumTriangles();
	Stats.NumLights = Lights.Num();
	UE_LOG(LogLeonEd, Log,
		"StaticLighting: %s: %d meshes, %d vertices, %d occluding triangles, %d lights, %lld rays in %.2f s",
		*World.GetName(), Stats.NumMeshes, Stats.NumVertices, Stats.NumOccluders, Stats.NumLights,
		static_cast<long long>(Stats.NumRays),
		double(FPlatformTime::Cycles64() - StartCycles) * FPlatformTime::GetSecondsPerCycle64());
	return Stats;
}
