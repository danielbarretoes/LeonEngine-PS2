#include "Components/StaticMeshComponent.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshSceneProxy.h"

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

bool UStaticMeshComponent::HasValidMesh() const
{
	return StaticMesh != nullptr && StaticMesh->HasValidRenderData();
}

FMaterial UStaticMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (HasOverrideMaterial(ElementIndex))
	{
		return OverrideMaterials[ElementIndex];
	}
	if (StaticMesh != nullptr && StaticMesh->GetStaticMaterials().IsValidIndex(ElementIndex))
	{
		const UMaterialInterface* Material = StaticMesh->GetMaterial(ElementIndex);
		return Material != nullptr ? Material->GetRenderProxy() : FMaterial();
	}
	return FMaterial();
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
	const TArray<FMeshSection>& Sections = StaticMesh->GetLODResources().Sections;
	const int32 NumSections = Sections.Num() == 0 ? 1 : Sections.Num();
	OutMaterials.Reserve(NumSections);
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		const int32 Slot = Sections.IsValidIndex(SectionIndex) ? Sections[SectionIndex].MaterialIndex : 0;
		OutMaterials.Add(GetMaterial(Slot));
	}
}

bool UStaticMeshComponent::HasShadowCastingMaterial() const
{
	const int32 NumSlots = FMath::Max(GetNumMaterials(), 1);
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		const FMaterial Material = GetMaterial(Slot);
		if (Material.bCastsShadows && !Material.IsTransparent() && Material.Shading != EMaterialLightingModel::Unlit)
		{
			return true;
		}
	}
	return false;
}

FPrimitiveSceneProxy* UStaticMeshComponent::CreateSceneProxy()
{
	return HasValidMesh() ? new FStaticMeshSceneProxy(this) : nullptr;
}
