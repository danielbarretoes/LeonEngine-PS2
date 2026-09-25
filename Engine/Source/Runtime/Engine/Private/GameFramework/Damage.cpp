#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

float UGameplayStatics::ApplyPointDamage(
	ACharacter* DamagedActor, float BaseDamage, const FVector& HitFromDirection, ACharacter* /*DamageCauser*/)
{
	if (DamagedActor == nullptr || BaseDamage <= 0.0f)
	{
		return 0.0f;
	}
	(void)HitFromDirection;
	return DamagedActor->TakeDamage(BaseDamage);
}

float UGameplayStatics::ApplyRadialDamage(const TArray<ACharacter*>& Actors, float BaseDamage, const FVector& Origin,
	float DamageRadius, ACharacter* /*DamageCauser*/)
{
	if (BaseDamage <= 0.0f || DamageRadius <= 0.0f)
	{
		return 0.0f;
	}
	float TotalApplied = 0.0f;
	for (ACharacter* Actor : Actors)
	{
		if (Actor == nullptr || !Actor->IsAlive())
		{
			continue;
		}
		const float Dist = (Actor->GetActorLocation() - Origin).Size();
		if (Dist >= DamageRadius)
		{
			continue;
		}
		const float Falloff = 1.0f - (Dist / DamageRadius);
		TotalApplied += Actor->TakeDamage(BaseDamage * Falloff);
	}
	return TotalApplied;
}
