#include "Engine/Level.h"

int32 UStaticMeshComponent::SubMeshCount() const
{
	if (Mesh == nullptr || !Mesh->Valid())
	{
		return 0;
	}
	return Mesh->GetSubmeshes().Num() == 0 ? 1 : Mesh->GetSubmeshes().Num();
}

const FMaterial& UStaticMeshComponent::MaterialForSubMesh(int32 SubMeshIndex) const
{
	int32 Slot = 0;
	if (Mesh != nullptr && SubMeshIndex >= 0 && SubMeshIndex < Mesh->GetSubmeshes().Num())
	{
		Slot = Mesh->GetSubmeshes()[SubMeshIndex].MaterialIndex;
	}

	if (Materials.IsValidIndex(Slot))
	{
		return Materials[Slot];
	}
	if (bMaterialOverride)
	{
		return Material;
	}
	if (Mesh != nullptr && Mesh->HasMaterials() && Mesh->GetMaterials().IsValidIndex(Slot))
	{
		return Mesh->GetMaterials()[Slot];
	}
	return Material;
}

bool UStaticMeshComponent::IsShadowCaster() const
{
	if (bHidden || Mesh == nullptr || !Mesh->Valid())
	{
		return false;
	}

	const auto CountsAsCaster = [](const FMaterial& Mat)
	{ return Mat.bCastsShadows && !Mat.IsTransparent() && Mat.Shading != EMaterialShadingModel::Unlit; };

	if (Materials.Num() > 0)
	{
		return Materials.ContainsByPredicate(CountsAsCaster);
	}
	if (bMaterialOverride)
	{
		return CountsAsCaster(Material);
	}
	if (Mesh->HasMaterials())
	{
		return Mesh->GetMaterials().ContainsByPredicate(CountsAsCaster);
	}
	return CountsAsCaster(Material);
}

UStaticMeshComponent& ULevel::AddStaticMesh(UStaticMeshComponent Component)
{
	return StaticMeshes.Add_GetRef(MoveTemp(Component));
}

FPlayerStart& ULevel::AddPlayerStart(FPlayerStart Start)
{
	return PlayerStarts.Add_GetRef(MoveTemp(Start));
}

FTriggerVolume& ULevel::AddTriggerVolume(FTriggerVolume Volume)
{
	return TriggerVolumes.Add_GetRef(MoveTemp(Volume));
}

FPainCausingVolume& ULevel::AddPainCausingVolume(FPainCausingVolume Volume)
{
	return PainCausingVolumes.Add_GetRef(MoveTemp(Volume));
}

FAISpawnPoint& ULevel::AddAISpawnPoint(FAISpawnPoint Point)
{
	return AiSpawnPoints.Add_GetRef(MoveTemp(Point));
}

const FPlayerStart* ULevel::FindPlayerStart() const
{
	return PlayerStarts.Num() > 0 ? &PlayerStarts[0] : nullptr;
}

SIZE_T ULevel::FindStaticMeshIndexByTag(const FString& InTag) const
{
	if (InTag.IsEmpty())
	{
		return Npos;
	}
	for (int32 I = 0; I < StaticMeshes.Num(); ++I)
	{
		if (StaticMeshes[I].Tag.Equals(InTag, ESearchCase::CaseSensitive))
		{
			return static_cast<SIZE_T>(I);
		}
	}
	return Npos;
}

void ULevel::ClearStaticMeshes()
{
	StaticMeshes.Reset();
}

void ULevel::ClearPlayerStarts()
{
	PlayerStarts.Reset();
}

void ULevel::ClearTriggerVolumes()
{
	TriggerVolumes.Reset();
}

void ULevel::ClearPainCausingVolumes()
{
	PainCausingVolumes.Reset();
}

void ULevel::ClearAISpawnPoints()
{
	AiSpawnPoints.Reset();
}

void ULevel::ClearLights()
{
	DirectionalLights.Reset();
	PointLights.Reset();
}

void ULevel::Clear()
{
	ClearStaticMeshes();
	ClearPlayerStarts();
	ClearTriggerVolumes();
	ClearPainCausingVolumes();
	ClearAISpawnPoints();
	ClearLights();
	Name.Empty();
	GameMode.Empty();
}
