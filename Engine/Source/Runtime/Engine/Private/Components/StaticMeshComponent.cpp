#include "Components/StaticMeshComponent.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "EngineLogs.h"
#include "Materials/Material.h"
#include "StaticMeshSceneProxy.h"

namespace
{

	/** What a slot draws with: its material, else the engine's default material. */
	FMaterial GetSlotRenderProxy(const UMaterialInterface* Material)
	{
		if (Material == nullptr)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		return Material != nullptr ? Material->GetRenderProxy() : FMaterial();
	}

} // namespace

UStaticMeshComponent::UStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStaticMeshComponent::SetStaticMesh(UStaticMesh* NewMesh)
{
	if (NewMesh == StaticMesh)
	{
		return false;
	}
	StaticMesh = NewMesh;
	MarkRenderStateDirty();
	RecreatePhysicsState();
	return true;
}

bool UStaticMeshComponent::HasValidBakedVertexColors() const
{
	return StaticMesh != nullptr && BakedVertexColors.Matches(StaticMesh->GetLODResources().RenderData);
}

void UStaticMeshComponent::SetBakedVertexColors(FLPS2ColorStreams&& InColors)
{
	BakedVertexColors = MoveTemp(InColors);
	MarkRenderStateDirty();
}

void UStaticMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	BakedVertexColors.Serialize(Ar);
	if (Ar.IsLoading() && Ar.IsError())
	{
		UE_LOG(LogEngine, Error, "UStaticMeshComponent %s: damaged baked vertex colours", *GetPathName());
	}
}

bool UStaticMeshComponent::HasValidMesh() const
{
	return StaticMesh != nullptr && StaticMesh->HasValidRenderData();
}

UMaterialInterface* UStaticMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (HasOverrideMaterial(ElementIndex))
	{
		return OverrideMaterials[ElementIndex];
	}
	return StaticMesh != nullptr ? StaticMesh->GetMaterial(ElementIndex) : nullptr;
}

UMaterialInterface* UStaticMeshComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
{
	SectionIndex = INDEX_NONE;
	const TArray<uint16>* Slots =
		StaticMesh != nullptr ? &StaticMesh->GetPhysicsTriMeshData().MaterialIndices : nullptr;
	if (Slots == nullptr || !Slots->IsValidIndex(FaceIndex))
	{
		return nullptr;
	}
	// Leon's sections are the material slots (UE maps the face to a render section, then its material).
	SectionIndex = (*Slots)[FaceIndex];
	return GetMaterial(SectionIndex);
}

int32 UStaticMeshComponent::GetNumMaterials() const
{
	const int32 MeshMaterials = StaticMesh != nullptr ? StaticMesh->GetStaticMaterials().Num() : 0;
	return FMath::Max(MeshMaterials, Super::GetNumMaterials());
}

void UStaticMeshComponent::GetSectionMaterials(TArray<FMaterial>& OutMaterials) const
{
	OutMaterials.Reset();
	if (!HasValidMesh())
	{
		return;
	}
	const FStaticMeshLODResources& Resources = StaticMesh->GetLODResources();
	const int32 NumSections = FMath::Max(Resources.GetNumSections(), 1);
	OutMaterials.Reserve(NumSections);
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		OutMaterials.Add(GetSlotRenderProxy(GetMaterial(Resources.GetSectionMaterialIndex(SectionIndex))));
	}
}

FPrimitiveSceneProxy* UStaticMeshComponent::CreateSceneProxy()
{
	return HasValidMesh() ? new FStaticMeshSceneProxy(this) : nullptr;
}

FTransform UStaticMeshComponent::GetSocketTransform(FName InSocketName) const
{
	const UStaticMeshSocket* Socket = StaticMesh != nullptr ? StaticMesh->FindSocket(InSocketName) : nullptr;
	FTransform SocketTransform;
	if (Socket != nullptr && Socket->GetSocketTransform(SocketTransform, this))
	{
		return SocketTransform;
	}
	return Super::GetSocketTransform(InSocketName);
}

bool UStaticMeshComponent::DoesSocketExist(FName InSocketName) const
{
	return StaticMesh != nullptr && StaticMesh->FindSocket(InSocketName) != nullptr;
}
