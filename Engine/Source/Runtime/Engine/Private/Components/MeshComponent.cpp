#include "Components/MeshComponent.h"

UMeshComponent::UMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMeshComponent::SetMaterial(int32 ElementIndex, const FMaterial& Material)
{
	if (ElementIndex < 0)
	{
		return;
	}
	if (ElementIndex >= OverrideMaterials.Num())
	{
		OverrideMaterials.SetNum(ElementIndex + 1);
		OverrideMaterialSet.SetNum(ElementIndex + 1);
	}
	OverrideMaterials[ElementIndex] = Material;
	OverrideMaterialSet[ElementIndex] = true;
}

FMaterial UMeshComponent::GetMaterial(int32 ElementIndex) const
{
	return HasOverrideMaterial(ElementIndex) ? OverrideMaterials[ElementIndex] : FMaterial();
}

int32 UMeshComponent::GetNumMaterials() const
{
	return OverrideMaterials.Num();
}

bool UMeshComponent::HasOverrideMaterial(int32 ElementIndex) const
{
	return OverrideMaterialSet.IsValidIndex(ElementIndex) && OverrideMaterialSet[ElementIndex];
}

void UMeshComponent::EmptyOverrideMaterials()
{
	OverrideMaterials.Empty();
	OverrideMaterialSet.Empty();
}
