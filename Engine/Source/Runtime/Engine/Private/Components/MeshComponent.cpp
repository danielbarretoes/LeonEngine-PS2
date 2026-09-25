#include "Components/MeshComponent.h"

#include "Materials/MaterialInterface.h"

UMeshComponent::UMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	if (ElementIndex < 0)
	{
		return;
	}
	if (ElementIndex >= OverrideMaterials.Num())
	{
		OverrideMaterials.SetNumZeroed(ElementIndex + 1);
	}
	OverrideMaterials[ElementIndex] = Material;
	MarkRenderStateDirty();
}

UMaterialInterface* UMeshComponent::GetMaterial(int32 ElementIndex) const
{
	return OverrideMaterials.IsValidIndex(ElementIndex) ? OverrideMaterials[ElementIndex] : nullptr;
}

int32 UMeshComponent::GetNumMaterials() const
{
	return OverrideMaterials.Num();
}

bool UMeshComponent::HasOverrideMaterial(int32 ElementIndex) const
{
	return OverrideMaterials.IsValidIndex(ElementIndex) && OverrideMaterials[ElementIndex] != nullptr;
}

void UMeshComponent::EmptyOverrideMaterials()
{
	OverrideMaterials.Empty();
	MarkRenderStateDirty();
}
