#include "GameFramework/VolumeHelpers.h"

#include "Engine/TriggerVolume.h"
#include "GameFramework/Character.h"
#include "GameFramework/PainCausingVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Level/LegacyLevelDataComponent.h"

namespace
{

	[[nodiscard]] float SmallestPositiveInterval(TArrayView<APainCausingVolume* const> Volumes)
	{
		float Best = TNumericLimits<float>::Max();
		for (const APainCausingVolume* Vol : Volumes)
		{
			if (Vol != nullptr && Vol->bPainCausing && Vol->PainInterval > 0.0f && Vol->PainInterval < Best)
			{
				Best = Vol->PainInterval;
			}
		}
		return Best;
	}

	/** The volume's `.llev` trigger data, or the record defaults. */
	[[nodiscard]] const ULegacyLevelDataComponent& TriggerData(const ATriggerVolume& Volume)
	{
		if (const ULegacyLevelDataComponent* Data = Volume.FindComponentByClass<ULegacyLevelDataComponent>())
		{
			return *Data;
		}
		return *GetDefault<ULegacyLevelDataComponent>();
	}

} // namespace

bool CharacterOverlapsPainVolume(const ACharacter& Ch, const APainCausingVolume& Vol)
{
	return Vol.EncompassesPoint(Ch.GetActorLocation());
}

void ApplyPainVolumeDamage(ACharacter& Ch, const APainCausingVolume& Vol)
{
	const float Amount = Vol.DamagePerSec * Vol.PainInterval;
	if (Amount <= 0.0f)
	{
		return;
	}
	(void)UGameplayStatics::ApplyPointDamage(&Ch, Amount, FVector(0.0f, 0.0f, -1.0f));
}

void TickPainCausingVolumes(TArrayView<APainCausingVolume* const> Volumes, TArrayView<ACharacter*> Characters,
	float DeltaTime, float& TickAccum)
{
	if (Volumes.Num() == 0 || Characters.Num() == 0 || DeltaTime <= 0.0f)
	{
		return;
	}
	const float Interval = SmallestPositiveInterval(Volumes);
	if (!(Interval < TNumericLimits<float>::Max()))
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
		for (const APainCausingVolume* Vol : Volumes)
		{
			if (Vol == nullptr || !Vol->bPainCausing || !CharacterOverlapsPainVolume(*Ch, *Vol))
			{
				continue;
			}
			ApplyPainVolumeDamage(*Ch, *Vol);
			break;
		}
	}
}

ATriggerVolume* FindBestTriggerVolume(TArrayView<ATriggerVolume* const> Volumes, const FVector& Feet, float MaxDist)
{
	if (Volumes.Num() == 0 || MaxDist <= 0.0f)
	{
		return nullptr;
	}

	ATriggerVolume* Best = nullptr;
	float BestDist = MaxDist;
	for (ATriggerVolume* Vol : Volumes)
	{
		if (Vol == nullptr)
		{
			continue;
		}
		const float InteractRadius = TriggerData(*Vol).InteractRadius;
		const float Radius = InteractRadius > 0.0f ? InteractRadius : MaxDist;
		const float Limit = Radius < MaxDist ? Radius : MaxDist;
		const FVector VolumeLocation = Vol->GetActorLocation();
		const FVector Delta = FVector(Feet.X - VolumeLocation.X, Feet.Y - VolumeLocation.Y, 0.0f);
		const float Dist = Delta.Size();
		if (Dist < BestDist && Dist <= Limit)
		{
			BestDist = Dist;
			Best = Vol;
		}
	}
	return Best;
}

FString FormatDefaultInteractPrompt(const ATriggerVolume& Volume)
{
	const ULegacyLevelDataComponent& Data = TriggerData(Volume);
	const FString& Payload = Data.Payload;
	const FString CostSuffix = Data.InteractCost > 0 ? FString::Printf(" [%d]", Data.InteractCost) : FString();

	if (Payload.IsEmpty())
	{
		return "[F] Interact" + CostSuffix;
	}
	if (Payload.Equals("Door", ESearchCase::CaseSensitive))
	{
		return "[F] Open Door" + CostSuffix;
	}
	if (Payload.StartsWith("WallBuy:", ESearchCase::CaseSensitive))
	{
		return "[F] Buy " + Payload.Mid(8) + CostSuffix;
	}
	if (Payload.StartsWith("Perk:", ESearchCase::CaseSensitive))
	{
		return "[F] " + Payload.Mid(5) + CostSuffix;
	}
	if (Payload.Equals("Ammo", ESearchCase::CaseSensitive))
	{
		return "[F] Buy Ammo" + CostSuffix;
	}
	if (Payload.Equals("PackAPunch", ESearchCase::CaseSensitive))
	{
		return "[F] Pack-a-Punch" + CostSuffix;
	}
	return "[F] " + Payload + CostSuffix;
}
