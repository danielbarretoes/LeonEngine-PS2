#include "PrimitiveSceneProxy.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Frustum.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "SceneView.h"
#include "SkeletalMeshSceneProxy.h"
#include "StaticMeshSceneProxy.h"
#include "UObject/UObjectGlobals.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, EPrimitiveSceneProxyType InProxyType)
	: LocalToWorld(InComponent->GetComponentTransform().ToMatrixWithScale())
	, PreviousTransform(InComponent->GetComponentTransform())
	, CurrentTransform(InComponent->GetComponentTransform())
	, ProxyType(InProxyType)
	, bShown(InComponent->ShouldRender())
	, bCastDynamicShadow(InComponent->CastShadow && !InComponent->bRenderAsViewModel)
	, bOnlyOwnerSee(InComponent->bOnlyOwnerSee)
	, bOwnerNoSee(InComponent->bOwnerNoSee)
	, bRenderAsViewModel(InComponent->bRenderAsViewModel)
	, bStaticLighting(InComponent->Mobility == EComponentMobility::Static)
	, bCastBlobShadow(InComponent->bCastBlobShadow && !InComponent->bRenderAsViewModel)
{
	if (bOnlyOwnerSee || bOwnerNoSee)
	{
		// UE: the actors which directly or indirectly own the component.
		for (const AActor* Owner = InComponent->GetOwner(); Owner != nullptr; Owner = Owner->GetOwner())
		{
			Owners.Add(Owner);
		}
	}
}

void FPrimitiveSceneProxy::SetStepTransform(const FTransform& InTransform, uint32 Step)
{
	if (Step != CurrentStep)
	{
		PreviousTransform = CurrentTransform;
		CurrentStep = Step;
	}
	CurrentTransform = InTransform;
	// A jump (a respawn, a teleport) is drawn at once, not swept across the map.
	const FVector Moved = CurrentTransform.GetLocation() - PreviousTransform.GetLocation();
	if (Moved.SizeSquared() > FMath::Square(TeleportDistance))
	{
		PreviousTransform = CurrentTransform;
	}
	bInterpolate = !PreviousTransform.Equals(CurrentTransform, 0.0f);
	LocalToWorld = CurrentTransform.ToMatrixWithScale();
}

void FPrimitiveSceneProxy::InterpolateTransform(float Alpha)
{
	if (!bInterpolate)
	{
		return;
	}
	FTransform Blended;
	Blended.Blend(PreviousTransform, CurrentTransform, FMath::Clamp(Alpha, 0.0f, 1.0f));
	LocalToWorld = Blended.ToMatrixWithScale();
}

bool FPrimitiveSceneProxy::IsShown(const FSceneView* View) const
{
	if (!bShown)
	{
		return false;
	}
	if (View != nullptr && (bOnlyOwnerSee || bOwnerNoSee))
	{
		const bool bOwnedByViewer = View->ViewActor != nullptr && Owners.Contains(View->ViewActor);
		if ((bOnlyOwnerSee && !bOwnedByViewer) || (bOwnerNoSee && bOwnedByViewer))
		{
			return false;
		}
	}
	return true;
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(const UStaticMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, EPrimitiveSceneProxyType::StaticMesh)
	, StaticMesh(InComponent->GetStaticMesh())
{
	InComponent->GetSectionMaterials(SectionMaterials);
	if (HasStaticLighting() && InComponent->HasValidBakedVertexColors())
	{
		BakedVertexColors = InComponent->BakedVertexColors;
	}
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
	const FLPS2Mesh& RenderData = SkeletalMesh->GetRenderData();
	for (int32 Section = 0; Section < RenderData.GetNumSections(); ++Section)
	{
		const int32 Slot = int32(RenderData.GetSection(Section).MaterialIndex);
		const UMaterialInterface* SlotMaterial = InComponent->GetMaterial(Slot);
		if (SlotMaterial == nullptr)
		{
			SlotMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		SectionMaterials.Add(SlotMaterial != nullptr ? SlotMaterial->GetRenderProxy() : FMaterial());
	}
}

const FMaterial& FSkeletalMeshSceneProxy::GetSectionMaterial(int32 SectionIndex) const
{
	static const FMaterial MissingSectionMaterial;
	return SectionMaterials.IsValidIndex(SectionIndex) ? SectionMaterials[SectionIndex] : MissingSectionMaterial;
}

FBox FSkeletalMeshSceneProxy::GetWorldBounds() const
{
	const FBox& LocalBox = LocalBounds.IsValid ? LocalBounds : SkeletalMesh->GetBoundingBox();
	return TransformLocalBox(LocalBox.Min, LocalBox.Max, GetLocalToWorld());
}

void FSkeletalMeshSceneProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SkeletalMesh);
	for (FMaterial& Material : SectionMaterials)
	{
		Collector.AddReferencedObject(Material.AlbedoMap);
	}
}
