#include "GameFramework/VolumeHelpers.h"

#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

namespace
{

	[[nodiscard]] bool PointInPainAabb(const FVector& Point, const FPainCausingVolume& Vol)
	{
		const FVector Half = Vol.Transform.Scale.GetAbs() * 0.5f;
		const FVector Min = Vol.Transform.Position - Half;
		const FVector Max = Vol.Transform.Position + Half;
		return Point.X >= Min.X && Point.X <= Max.X && Point.Y >= Min.Y && Point.Y <= Max.Y && Point.Z >= Min.Z &&
			Point.Z <= Max.Z;
	}

	[[nodiscard]] float SmallestPositiveInterval(const TArray<FPainCausingVolume>& Volumes)
	{
		float Best = TNumericLimits<float>::Max();
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
	(void)UGameplayStatics::ApplyPointDamage(&Ch, Amount, FVector(0.0f, -1.0f, 0.0f));
}

void TickPainCausingVolumes(
	const TArray<FPainCausingVolume>& Volumes, TArrayView<ACharacter*> Characters, float DeltaTime, float& TickAccum)
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

SIZE_T FindBestTriggerVolume(const TArray<FTriggerVolume>& Volumes, const FVector& Feet, float MaxDist)
{
	if (Volumes.Num() == 0 || MaxDist <= 0.0f)
	{
		return ULevel::Npos;
	}

	SIZE_T Best = ULevel::Npos;
	float BestDist = MaxDist;
	for (int32 I = 0; I < Volumes.Num(); ++I)
	{
		const FTriggerVolume& Vol = Volumes[I];
		const float Radius = Vol.InteractRadius > 0.0f ? Vol.InteractRadius : MaxDist;
		const float Limit = Radius < MaxDist ? Radius : MaxDist;
		const FVector Delta = FVector(Feet.X - Vol.Transform.Position.X, 0.0f, Feet.Z - Vol.Transform.Position.Z);
		const float Dist = Delta.Size();
		if (Dist < BestDist && Dist <= Limit)
		{
			BestDist = Dist;
			Best = static_cast<SIZE_T>(I);
		}
	}
	return Best;
}

FString FormatDefaultInteractPrompt(const FTriggerVolume& Volume)
{
	const FString& Payload = Volume.Payload;
	const FString CostSuffix = Volume.InteractCost > 0 ? FString::Printf(" [%d]", Volume.InteractCost) : FString();

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
