#include "Engine/Level.h"

#include <algorithm>
#include <string_view>
#include <utility>

std::size_t UStaticMeshComponent::SubMeshCount() const
{
	if (Mesh == nullptr || !Mesh->Valid())
	{
		return 0;
	}
	return Mesh->GetSubmeshes().empty() ? 1 : Mesh->GetSubmeshes().size();
}

const FMaterial& UStaticMeshComponent::MaterialForSubMesh(std::size_t SubMeshIndex) const
{
	int Slot = 0;
	if (Mesh != nullptr && SubMeshIndex < Mesh->GetSubmeshes().size())
	{
		Slot = Mesh->GetSubmeshes()[SubMeshIndex].MaterialIndex;
	}

	if (Slot >= 0 && static_cast<std::size_t>(Slot) < Materials.size())
	{
		return Materials[static_cast<std::size_t>(Slot)];
	}
	if (bMaterialOverride)
	{
		return Material;
	}
	if (Mesh != nullptr && Mesh->HasMaterials() && Slot >= 0 &&
		static_cast<std::size_t>(Slot) < Mesh->GetMaterials().size())
	{
		return Mesh->GetMaterials()[static_cast<std::size_t>(Slot)];
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

	if (!Materials.empty())
	{
		return std::any_of(Materials.begin(), Materials.end(), CountsAsCaster);
	}
	if (bMaterialOverride)
	{
		return CountsAsCaster(Material);
	}
	if (Mesh->HasMaterials())
	{
		const auto& Mats = Mesh->GetMaterials();
		return std::any_of(Mats.begin(), Mats.end(), CountsAsCaster);
	}
	return CountsAsCaster(Material);
}

UStaticMeshComponent& ULevel::AddStaticMesh(UStaticMeshComponent Component)
{
	StaticMeshes.push_back(std::move(Component));
	return StaticMeshes.back();
}

FPlayerStart& ULevel::AddPlayerStart(FPlayerStart Start)
{
	PlayerStarts.push_back(std::move(Start));
	return PlayerStarts.back();
}

FTriggerVolume& ULevel::AddTriggerVolume(FTriggerVolume Volume)
{
	TriggerVolumes.push_back(std::move(Volume));
	return TriggerVolumes.back();
}

FPainCausingVolume& ULevel::AddPainCausingVolume(FPainCausingVolume Volume)
{
	PainCausingVolumes.push_back(std::move(Volume));
	return PainCausingVolumes.back();
}

FAISpawnPoint& ULevel::AddAISpawnPoint(FAISpawnPoint Point)
{
	AiSpawnPoints.push_back(std::move(Point));
	return AiSpawnPoints.back();
}

const FPlayerStart* ULevel::FindPlayerStart() const
{
	if (PlayerStarts.empty())
	{
		return nullptr;
	}
	return &PlayerStarts.front();
}

std::size_t ULevel::FindStaticMeshIndexByTag(std::string_view InTag) const
{
	if (InTag.empty())
	{
		return Npos;
	}
	for (std::size_t I = 0; I < StaticMeshes.size(); ++I)
	{
		if (StaticMeshes[I].Tag == InTag)
		{
			return I;
		}
	}
	return Npos;
}

void ULevel::ClearStaticMeshes()
{
	StaticMeshes.clear();
}

void ULevel::ClearPlayerStarts()
{
	PlayerStarts.clear();
}

void ULevel::ClearTriggerVolumes()
{
	TriggerVolumes.clear();
}

void ULevel::ClearPainCausingVolumes()
{
	PainCausingVolumes.clear();
}

void ULevel::ClearAISpawnPoints()
{
	AiSpawnPoints.clear();
}

void ULevel::ClearLights()
{
	DirectionalLights.clear();
	PointLights.clear();
}

void ULevel::Clear()
{
	ClearStaticMeshes();
	ClearPlayerStarts();
	ClearTriggerVolumes();
	ClearPainCausingVolumes();
	ClearAISpawnPoints();
	ClearLights();
	Name.clear();
	GameMode.clear();
}
