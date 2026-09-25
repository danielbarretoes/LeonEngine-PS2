#include "Engine/Level.h"

#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"

namespace
{

	/** The material MaterialForSubMesh returns for a section the snapshot does not have. */
	const FMaterial& MissingSectionMaterial()
	{
		static const FMaterial Material;
		return Material;
	}

	[[nodiscard]] bool IsLiveActor(const AActor* Actor)
	{
		return Actor != nullptr && !Actor->IsPendingKillPending();
	}

} // namespace

int32 FLevelStaticMesh::SubMeshCount() const
{
	if (Mesh == nullptr || !Mesh->Valid())
	{
		return 0;
	}
	return Mesh->GetSubmeshes().Num() == 0 ? 1 : Mesh->GetSubmeshes().Num();
}

const FMaterial& FLevelStaticMesh::MaterialForSubMesh(int32 SubMeshIndex) const
{
	return SectionMaterials.IsValidIndex(SubMeshIndex) ? SectionMaterials[SubMeshIndex] : MissingSectionMaterial();
}

bool FLevelStaticMesh::IsShadowCaster() const
{
	return !bHidden && Mesh != nullptr && Mesh->Valid() && bCastShadow;
}

void ULevel::GetStaticMeshSnapshots(TArray<FLevelStaticMesh>& OutMeshes) const
{
	OutMeshes.Reset();
	for (const AActor* Actor : Actors)
	{
		const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
		if (!IsLiveActor(MeshActor))
		{
			continue;
		}
		const UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent();
		if (Component == nullptr)
		{
			continue;
		}
		FLevelStaticMesh& Snapshot = OutMeshes.AddDefaulted_GetRef();
		Snapshot.Transform = Component->GetComponentTransform();
		Snapshot.Mesh = Component->GetStaticMeshShared();
		Component->GetSectionMaterials(Snapshot.SectionMaterials);
		Snapshot.bHidden = !Component->ShouldRender();
		Snapshot.bCastShadow = Component->CastShadow && Component->HasShadowCastingMaterial();
	}
}

void ULevel::GetLightSnapshots(TArray<FDirectionalLight>& OutDirectional, TArray<FPointLight>& OutPoint) const
{
	OutDirectional.Reset();
	OutPoint.Reset();
	for (const AActor* Actor : Actors)
	{
		if (!IsLiveActor(Actor) || Actor->IsHidden())
		{
			continue;
		}
		if (const ADirectionalLight* Directional = Cast<ADirectionalLight>(Actor))
		{
			const UDirectionalLightComponent* Component = Directional->GetDirectionalLightComponent();
			if (Component == nullptr || !Component->IsVisible())
			{
				continue;
			}
			FDirectionalLight& Light = OutDirectional.AddDefaulted_GetRef();
			Light.Transform = Component->GetComponentTransform();
			Light.LightColor = FVector(Component->LightColor.R, Component->LightColor.G, Component->LightColor.B);
			Light.Intensity = Component->Intensity;
			Light.bCastShadows = Component->CastShadows;
			Light.SourceAngle = Component->LightSourceAngle;
		}
		else if (const APointLight* Point = Cast<APointLight>(Actor))
		{
			const UPointLightComponent* Component = Point->GetPointLightComponent();
			if (Component == nullptr || !Component->IsVisible())
			{
				continue;
			}
			FPointLight& Light = OutPoint.AddDefaulted_GetRef();
			Light.Transform = Component->GetComponentTransform();
			Light.LightColor = FVector(Component->LightColor.R, Component->LightColor.G, Component->LightColor.B);
			Light.Intensity = Component->Intensity;
			Light.Range = Component->AttenuationRadius;
			Light.bCastShadows = Component->CastShadows;
		}
	}
}
