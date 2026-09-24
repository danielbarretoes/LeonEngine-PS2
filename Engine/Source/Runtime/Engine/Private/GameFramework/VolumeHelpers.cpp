#include "GameFramework/VolumeHelpers.h"

#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <limits>

namespace
{

	[[nodiscard]] bool PointInPainAabb(const glm::vec3& Point, const FPainCausingVolume& Vol)
	{
		const glm::vec3 Half = glm::abs(Vol.Transform.Scale) * 0.5f;
		const glm::vec3 Min = Vol.Transform.Position - Half;
		const glm::vec3 Max = Vol.Transform.Position + Half;
		return Point.x >= Min.x && Point.x <= Max.x && Point.y >= Min.y && Point.y <= Max.y && Point.z >= Min.z &&
			Point.z <= Max.z;
	}

	[[nodiscard]] float SmallestPositiveInterval(const std::vector<FPainCausingVolume>& Volumes)
	{
		float Best = (std::numeric_limits<float>::max)();
		for (const FPainCausingVolume& Vol : Volumes)
		{
			if (Vol.DamageInterval > 0.0f && Vol.DamageInterval < Best)
			{
				Best = Vol.DamageInterval;
			}
		}
		return Best;
	}

} // namespace

bool CharacterOverlapsPainVolume(const ACharacter& Ch, const FPainCausingVolume& Vol)
{
	return PointInPainAabb(Ch.GetActorLocation(), Vol);
}

void ApplyPainVolumeDamage(ACharacter& Ch, const FPainCausingVolume& Vol)
{
	const float Amount = Vol.DamagePerSecond * Vol.DamageInterval;
	if (Amount <= 0.0f)
	{
		return;
	}
	(void)UGameplayStatics::ApplyPointDamage(&Ch, Amount, glm::vec3{0.0f, -1.0f, 0.0f});
}

void TickPainCausingVolumes(const std::vector<FPainCausingVolume>& Volumes, std::span<ACharacter*> Characters,
	float DeltaTime, float& TickAccum)
{
	if (Volumes.empty() || Characters.empty() || DeltaTime <= 0.0f)
	{
		return;
	}
	const float Interval = SmallestPositiveInterval(Volumes);
	if (!(Interval < (std::numeric_limits<float>::max)()))
	{
		return;
	}

	TickAccum += DeltaTime;
	if (TickAccum < Interval)
	{
		return;
	}
	TickAccum = 0.0f;

	for (ACharacter* Ch : Characters)
	{
		if (Ch == nullptr || !Ch->IsAlive())
		{
			continue;
		}
		for (const FPainCausingVolume& Vol : Volumes)
		{
			if (!CharacterOverlapsPainVolume(*Ch, Vol))
			{
				continue;
			}
			ApplyPainVolumeDamage(*Ch, Vol);
			break;
		}
	}
}

std::size_t FindBestTriggerVolume(const std::vector<FTriggerVolume>& Volumes, const glm::vec3& Feet, float MaxDist)
{
	if (Volumes.empty() || MaxDist <= 0.0f)
	{
		return ULevel::Npos;
	}

	std::size_t Best = ULevel::Npos;
	float BestDist = MaxDist;
	for (std::size_t I = 0; I < Volumes.size(); ++I)
	{
		const FTriggerVolume& Vol = Volumes[I];
		const float Radius = Vol.InteractRadius > 0.0f ? Vol.InteractRadius : MaxDist;
		const float Limit = Radius < MaxDist ? Radius : MaxDist;
		const glm::vec3 Delta{Feet.x - Vol.Transform.Position.x, 0.0f, Feet.z - Vol.Transform.Position.z};
		const float Dist = glm::length(Delta);
		if (Dist < BestDist && Dist <= Limit)
		{
			BestDist = Dist;
			Best = I;
		}
	}
	return Best;
}

std::string FormatDefaultInteractPrompt(const FTriggerVolume& Volume)
{
	const std::string& Payload = Volume.Payload;
	const std::string CostSuffix =
		Volume.InteractCost > 0 ? (" [" + std::to_string(Volume.InteractCost) + "]") : std::string{};

	if (Payload.empty())
	{
		return "[F] Interact" + CostSuffix;
	}
	if (Payload == "Door")
	{
		return "[F] Open Door" + CostSuffix;
	}
	if (Payload.rfind("WallBuy:", 0) == 0)
	{
		return "[F] Buy " + Payload.substr(8) + CostSuffix;
	}
	if (Payload.rfind("Perk:", 0) == 0)
	{
		return "[F] " + Payload.substr(5) + CostSuffix;
	}
	if (Payload == "Ammo")
	{
		return "[F] Buy Ammo" + CostSuffix;
	}
	if (Payload == "PackAPunch")
	{
		return "[F] Pack-a-Punch" + CostSuffix;
	}
	return "[F] " + Payload + CostSuffix;
}
