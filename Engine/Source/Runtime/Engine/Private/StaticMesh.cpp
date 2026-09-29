#include "Engine/StaticMesh.h"

#include "AssetBulkData.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "EngineLogs.h"
#include "HAL/LowLevelMemTracker.h"
#include "IMeshBuilderModule.h"
#include "Materials/MaterialInterface.h"
#include "MeshData.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "RendererInterface.h"

namespace
{

	/** The local bounding box of the source's positions. */
	FBox ComputeLocalBounds(const TArray<FVertex>& Vertices)
	{
		FVector Min(TNumericLimits<float>::Max());
		FVector Max(TNumericLimits<float>::Lowest());
		for (const FVertex& Vertex : Vertices)
		{
			Min = Min.ComponentMin(Vertex.Position);
			Max = Max.ComponentMax(Vertex.Position);
		}
		return FBox(Min, Max);
	}

} // namespace

void FTriMeshCollisionData::Serialize(FArchive& Ar)
{
	Ar << Vertices;
	Ar << Indices;
	// VER_LEON_COLLISION_MATERIAL_INDICES: a slot a triangle.
	Ar << MaterialIndices;
	if (Ar.IsLoading() && ((Indices.Num() % 3) != 0 || MaterialIndices.Num() != Indices.Num() / 3))
	{
		Ar.SetError();
		Vertices.Empty();
		Indices.Empty();
		MaterialIndices.Empty();
	}
}

IMeshBuilderModule* IMeshBuilderModule::GetForRunningPlatform()
{
	return FModuleManager::GetModulePtr<IMeshBuilderModule>("MeshUtilities");
}

UStaticMesh::UStaticMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStaticMesh::BuildFromMeshData(const FMeshData& Data)
{
	if (Data.IsEmpty())
	{
		return false;
	}
	IMeshBuilderModule* Builder = IMeshBuilderModule::GetForRunningPlatform();
	if (Builder == nullptr)
	{
		UE_LOG(LogEngine, Error, "UStaticMesh %s: no mesh builder in this program (MeshUtilities)", *GetPathName());
		return false;
	}
	FLPS2Mesh RenderData;
	FString Error;
	if (!Builder->BuildMesh(Data, RenderData, Error))
	{
		UE_LOG(LogEngine, Error, "UStaticMesh %s: %s", *GetPathName(), *Error);
		return false;
	}
	LODResources.Reset();
	LODResources.AddDefaulted_GetRef().RenderData = MoveTemp(RenderData);
	// The later LODs: LOD 0's source simplified to each one's share of the triangles (N15); a LOD the simplifier cannot
	// make draws the one before it.
	for (int32 LODIndex = 1; LODIndex < SourceModels.Num(); ++LODIndex)
	{
		FMeshData Simplified;
		FLPS2Mesh LODData;
		const float Percent = FMath::Clamp(SourceModels[LODIndex].ReductionSettings.PercentTriangles, 0.0f, 1.0f);
		if (!Builder->SimplifyMesh(Data, Percent, Simplified, Error) || !Builder->BuildMesh(Simplified, LODData, Error))
		{
			UE_LOG(LogEngine, Warning, "UStaticMesh %s: LOD %d: %s; it draws LOD %d", *GetPathName(), LODIndex, *Error,
				LODIndex - 1);
			LODData = LODResources.Last().RenderData;
		}
		LODResources.AddDefaulted_GetRef().RenderData = MoveTemp(LODData);
	}
	// The physics scene collides with the source's triangles at full precision.
	PhysicsTriMeshData.Vertices.SetNum(Data.Vertices.Num());
	for (int32 Index = 0; Index < Data.Vertices.Num(); ++Index)
	{
		PhysicsTriMeshData.Vertices[Index] = Data.Vertices[Index].Position;
	}
	PhysicsTriMeshData.Indices = Data.Indices;
	// Each triangle's slot: its section's (a triangle no section holds, and every one of a mesh without sections, the
	// first slot's).
	PhysicsTriMeshData.MaterialIndices.Init(0, Data.Indices.Num() / 3);
	for (const FMeshSection& Section : Data.Submeshes)
	{
		const int32 First = FMath::Max(Section.IndexOffset, 0) / 3;
		const int32 Last = FMath::Min(Section.IndexOffset + Section.IndexCount, Data.Indices.Num()) / 3;
		for (int32 Triangle = First; Triangle < Last; ++Triangle)
		{
			PhysicsTriMeshData.MaterialIndices[Triangle] =
				static_cast<uint16>(FMath::Clamp(Section.MaterialIndex, 0, 0xFFFF));
		}
	}
	BoundingBox = ComputeLocalBounds(Data.Vertices);
	CreateBodySetup();
	InitResources();
	return true;
}

const FStaticMeshLODResources& UStaticMesh::GetLODResources(int32 LODIndex) const
{
	static const FStaticMeshLODResources NoResources;
	if (LODResources.Num() == 0)
	{
		return NoResources;
	}
	return LODResources[FMath::Clamp(LODIndex, 0, LODResources.Num() - 1)];
}

float UStaticMesh::GetLODScreenSize(int32 LODIndex) const
{
	return LODIndex > 0 && SourceModels.IsValidIndex(LODIndex) ? SourceModels[LODIndex].ScreenSize : 1.0f;
}

void UStaticMesh::InitResources()
{
	// The next draw uploads the new geometry.
	ReleaseResources();
}

void UStaticMesh::ReleaseResources()
{
	ReleaseAssetRenderResources(this);
}

void UStaticMesh::PostLoad()
{
	Super::PostLoad();
	InitResources();
}

void UStaticMesh::BeginDestroy()
{
	ReleaseResources();
	Super::BeginDestroy();
}

void UStaticMesh::CreateBodySetup()
{
	if (BodySetup == nullptr)
	{
		// A fixed name keeps saves deterministic (D13); transient with a transient mesh.
		BodySetup =
			NewObject<UBodySetup>(this, TEXT("BodySetup"), HasAnyFlags(RF_Transient) ? RF_Transient : RF_NoFlags);
	}
}

UMaterialInterface* UStaticMesh::GetMaterial(int32 MaterialIndex) const
{
	return StaticMaterials.IsValidIndex(MaterialIndex) ? StaticMaterials[MaterialIndex].MaterialInterface : nullptr;
}

UStaticMeshSocket* UStaticMesh::FindSocket(FName InSocketName) const
{
	if (InSocketName.IsNone())
	{
		return nullptr;
	}
	for (UStaticMeshSocket* Socket : Sockets)
	{
		if (Socket != nullptr && Socket->SocketName == InSocketName)
		{
			return Socket;
		}
	}
	return nullptr;
}

UStaticMeshSocket::UStaticMeshSocket(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStaticMeshSocket::GetSocketTransform(FTransform& OutTransform, const UStaticMeshComponent* MeshComp) const
{
	if (MeshComp == nullptr)
	{
		return false;
	}
	OutTransform = GetSocketLocalTransform() * MeshComp->GetComponentTransform();
	return true;
}

void UStaticMesh::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Meshes);
	Super::Serialize(Ar);
	Ar << BoundingBox;
	// Cooked, a mesh whose body setup answers with its simple shapes everywhere keeps no collision triangles: the
	// physics scene would never read them (FPhysScene, UBodySetup::UsesComplexAsSimpleForStaticBodies; N23).
	const bool bStripTriangles = Ar.IsSaving() && Ar.IsFilterEditorOnly() && BodySetup != nullptr &&
		!BodySetup->UsesComplexAsSimpleForStaticBodies();
	// LOD 0, the collision triangles, then the later LODs (N15): SourceModels, a tagged property read before this
	// tail, says how many (none for a mesh of one LOD, whose bytes are those of before the LODs).
	const int32 NumLODs = FMath::Max(SourceModels.Num(), 1);
	if (LODResources.Num() != NumLODs)
	{
		LODResources.SetNum(NumLODs);
	}
	if (!SerializeBulkPayload(Ar, this, GeometryBulkData,
			[this, bStripTriangles, NumLODs](FArchive& PayloadAr)
			{
				LODResources[0].Serialize(PayloadAr);
				if (bStripTriangles)
				{
					FTriMeshCollisionData None;
					None.Serialize(PayloadAr);
				}
				else
				{
					PhysicsTriMeshData.Serialize(PayloadAr);
				}
				for (int32 LODIndex = 1; LODIndex < NumLODs; ++LODIndex)
				{
					LODResources[LODIndex].Serialize(PayloadAr);
				}
			}))
	{
		UE_LOG(LogEngine, Error, "UStaticMesh %s: damaged geometry", *GetPathName());
		LODResources.Reset();
		PhysicsTriMeshData = FTriMeshCollisionData();
	}
}
