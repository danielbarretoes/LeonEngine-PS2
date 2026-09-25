#include "PrimitiveSceneProxy.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Frustum.h"
#include "Materials/Material.h"
#include "SkeletalMeshSceneProxy.h"
#include "StaticMeshSceneProxy.h"
#include "UObject/UObjectGlobals.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, EPrimitiveSceneProxyType InProxyType)
	: LocalToWorld(InComponent->GetComponentTransform().ToMatrixWithScale())
	, ProxyType(InProxyType)
	, bShown(InComponent->ShouldRender())
	, bCastDynamicShadow(InComponent->CastShadow)
{
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(const UStaticMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, EPrimitiveSceneProxyType::StaticMesh)
	, StaticMesh(InComponent->GetStaticMesh())
	, bHasShadowCastingMaterial(InComponent->HasShadowCastingMaterial())
{
	InComponent->GetSectionMaterials(SectionMaterials);
}

const FMaterial& FStaticMeshSceneProxy::GetSectionMaterial(int32 SectionIndex) const
{
	static const FMaterial MissingSectionMaterial;
	return SectionMaterials.IsValidIndex(SectionIndex) ? SectionMaterials[SectionIndex] : MissingSectionMaterial;
}

void FStaticMeshSceneProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(StaticMesh);
	for (FMaterial& Material : SectionMaterials)
	{
		Collector.AddReferencedObject(Material.AlbedoMap);
		Collector.AddReferencedObject(Material.NormalMap);
	}
}

FBox FStaticMeshSceneProxy::GetWorldBounds() const
{
	const FBox& LocalBox = StaticMesh->GetBoundingBox();
	return TransformLocalBox(LocalBox.Min, LocalBox.Max, GetLocalToWorld());
}

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(const USkeletalMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, EPrimitiveSceneProxyType::SkeletalMesh)
	, SkeletalMesh(InComponent->GetSkeletalMesh())
{
	const UMaterialInterface* SlotMaterial = SkeletalMesh->GetMaterial(0);
	if (SlotMaterial == nullptr)
	{
		SlotMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	Material = SlotMaterial != nullptr ? SlotMaterial->GetRenderProxy() : FMaterial();
}

void FSkeletalMeshSceneProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SkeletalMesh);
	Collector.AddReferencedObject(Material.AlbedoMap);
	Collector.AddReferencedObject(Material.NormalMap);
}
