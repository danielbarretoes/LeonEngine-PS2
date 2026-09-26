#include "Engine/StaticMesh.h"

#include "AssetBulkData.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "EngineLogs.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "RendererInterface.h"

namespace
{

	void ComputeLocalBounds(const TArray<FVertex>& Vertices, FVector& OutMin, FVector& OutMax)
	{
		OutMin = FVector(TNumericLimits<float>::Max());
		OutMax = FVector(TNumericLimits<float>::Lowest());
		for (const FVertex& Vertex : Vertices)
		{
			OutMin = OutMin.ComponentMin(Vertex.Position);
			OutMax = OutMax.ComponentMax(Vertex.Position);
		}
	}

	/** An element count, resized on load (a negative count is an error). */
	template <typename ElementType>
	bool SerializeCount(FArchive& Ar, TArray<ElementType>& Array)
	{
		int32 Num = Array.Num();
		Ar << Num;
		if (Ar.IsLoading())
		{
			if (Num < 0 || Ar.IsError())
			{
				Ar.SetCriticalError();
				Array.Empty();
				return false;
			}
			Array.SetNum(Num);
		}
		return !Ar.IsError();
	}

} // namespace

void FStaticMeshLODResources::ToMeshData(FMeshData& OutData) const
{
	OutData = FMeshData();
	OutData.Vertices = Vertices;
	OutData.Indices = Indices;
	OutData.Submeshes = Sections;
}

void FStaticMeshLODResources::Serialize(FArchive& Ar)
{
	if (SerializeCount(Ar, Vertices))
	{
		for (FVertex& Vertex : Vertices)
		{
			Ar << Vertex.Position << Vertex.Normal << Vertex.TexCoord << Vertex.Tangent;
		}
	}
	Ar << Indices;
	if (SerializeCount(Ar, Sections))
	{
		for (FMeshSection& Section : Sections)
		{
			Ar << Section.IndexOffset << Section.IndexCount << Section.MaterialIndex;
		}
	}
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
	LODResources.Vertices = Data.Vertices;
	LODResources.Indices = Data.Indices;
	LODResources.Sections = Data.Submeshes;
	if (LODResources.Sections.Num() == 0)
	{
		LODResources.Sections.Add(FMeshSection{0, LODResources.Indices.Num(), 0});
	}
	FVector Min;
	FVector Max;
	ComputeLocalBounds(LODResources.Vertices, Min, Max);
	BoundingBox = FBox(Min, Max);
	CreateBodySetup();
	InitResources();
	return true;
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
	Super::Serialize(Ar);
	Ar << BoundingBox;
	if (!SerializeBulkPayload(
			Ar, this, GeometryBulkData, [this](FArchive& PayloadAr) { LODResources.Serialize(PayloadAr); }))
	{
		UE_LOG(LogEngine, Error, "UStaticMesh %s: damaged geometry", *GetPathName());
		LODResources = FStaticMeshLODResources();
	}
}
