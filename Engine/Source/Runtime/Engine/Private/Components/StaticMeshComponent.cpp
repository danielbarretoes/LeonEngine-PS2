#include "Components/StaticMeshComponent.h"

#include "SceneRenderer.h"

UStaticMeshComponent::UStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStaticMeshComponent::SetStaticMesh(TSharedPtr<UStaticMesh> NewMesh)
{
	if (NewMesh == StaticMesh)
	{
		return false;
	}
	StaticMesh = MoveTemp(NewMesh);
	return true;
}

FMaterial UStaticMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (HasOverrideMaterial(ElementIndex))
	{
		return OverrideMaterials[ElementIndex];
	}
	if (StaticMesh != nullptr && StaticMesh->GetMaterials().IsValidIndex(ElementIndex))
	{
		return StaticMesh->GetMaterials()[ElementIndex];
	}
	return FMaterial();
}

int32 UStaticMeshComponent::GetNumMaterials() const
{
	const int32 MeshMaterials = StaticMesh != nullptr ? StaticMesh->GetMaterials().Num() : 0;
	return FMath::Max(MeshMaterials, Super::GetNumMaterials());
}

void UStaticMeshComponent::SubmitDraw(FSceneRenderer& Renderer) const
{
	if (!HasValidMesh())
	{
		return;
	}
	Renderer.SubmitStaticDraw(*StaticMesh, GetComponentTransform().ToMatrixWithScale(), GetMaterial(0));
}
