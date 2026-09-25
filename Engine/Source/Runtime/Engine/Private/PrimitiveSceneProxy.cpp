#include "PrimitiveSceneProxy.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Frustum.h"
#include "SkeletalMeshSceneProxy.h"
#include "StaticMeshSceneProxy.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, EPrimitiveSceneProxyType InProxyType)
	: LocalToWorld(InComponent->GetComponentTransform().ToMatrixWithScale())
	, ProxyType(InProxyType)
	, bShown(InComponent->ShouldRender())
	, bCastDynamicShadow(InComponent->CastShadow)
{
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(const UStaticMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, EPrimitiveSceneProxyType::StaticMesh)
	, StaticMesh(InComponent->GetStaticMeshShared())
	, bHasShadowCastingMaterial(InComponent->HasShadowCastingMaterial())
{
	InComponent->GetSectionMaterials(SectionMaterials);
}

const FMaterial& FStaticMeshSceneProxy::GetSectionMaterial(int32 SectionIndex) const
{
	static const FMaterial MissingSectionMaterial;
	return SectionMaterials.IsValidIndex(SectionIndex) ? SectionMaterials[SectionIndex] : MissingSectionMaterial;
}

FBox FStaticMeshSceneProxy::GetWorldBounds() const
{
	return TransformLocalBox(StaticMesh->GetLocalMin(), StaticMesh->GetLocalMax(), GetLocalToWorld());
}

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(const USkeletalMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, EPrimitiveSceneProxyType::SkeletalMesh)
	, SkeletalMesh(InComponent->GetSkeletalMeshShared())
{
}
