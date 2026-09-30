#include "Weapons/ShooterWeapon_Knife.h"

#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterPlayerController.h"

namespace
{

	/** CS: a cut is in the back when the attacker's direction and the victim's facing agree by more than this. */
	constexpr float BackstabDot = 0.8f;

	/** The mark a cut leaves on a surface: small and light. */
	constexpr float KnifeMarkSize = 4.0f;
	const FLinearColor KnifeMarkColor(0.1f, 0.09f, 0.08f, 0.6f);

	/** How early an attack may come, seconds (as the base weapon's shots). */
	constexpr float AttackTimeTolerance = 1.0e-3f;

} // namespace

AShooterWeapon_Knife::AShooterWeapon_Knife(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CS 1.6's knife; DefaultGame.ini's [/Script/ShooterGame.ShooterWeapon_Knife] tunes it.
	WeaponName = TEXT("knife");
	DisplayName = TEXT("Knife");
	Slot = EShooterWeaponSlot::Knife;
	bInfiniteClip = true;
	bAutomatic = true;
	AmmoPerClip = 0;
	MaxAmmo = 0;
	TimeBetweenShots = 0.4f;
	EquipDuration = 0.75f;
	ArmorRatio = 1.7f;
	FireNoiseLoudness = 0.2f;
	Price = 0;
}

float AShooterWeapon_Knife::GetTimeBetweenShots() const
{
	return bLastAttackStab ? StabCycleTime : TimeBetweenShots;
}

bool AShooterWeapon_Knife::IsBackstab(const FVector& AttackerLocation, const AShooterCharacter& Victim)
{
	const FVector ToVictim = (Victim.GetActorLocation() - AttackerLocation).GetSafeNormal2D();
	const FVector Facing = Victim.GetActorForwardVector().GetSafeNormal2D();
	return (ToVictim | Facing) > BackstabDot;
}

void AShooterWeapon_Knife::StartSecondaryFire()
{
	const UWorld* World = GetWorld();
	if (World == nullptr || !CanFire() ||
		World->GetTimeSeconds() + AttackTimeTolerance < LastFireTime + GetTimeBetweenShots())
	{
		return;
	}
	bStabbing = true;
	HandleFiring();
	bStabbing = false;
}

void AShooterWeapon_Knife::FireWeapon()
{
	bLastAttackStab = bStabbing;
	bLastAttackBackstab = false;
	LastHit = FHitResult();
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	FVector Start;
	FVector Direction;
	GetAim(Start, Direction);
	const FVector End = Start + (Direction * (bStabbing ? StabRange : SlashRange));
	FCollisionQueryParams Params(FName(TEXT("KnifeTrace")), true, GetInstigator());
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	if (!UGameplayStatics::LineTraceSingleByChannel(*World, Hit, Start, End, COLLISION_WEAPON, Params) &&
		!UGameplayStatics::SphereTraceSingleByChannel(*World, Hit, Start, End, HullRadius, COLLISION_WEAPON, Params))
	{
		return;
	}
	LastHit = Hit;
	AActor* HitActor = Hit.GetActor();
	AShooterCharacter* Victim = Cast<AShooterCharacter>(HitActor);
	if (Victim == nullptr)
	{
		(void)UGameplayStatics::SpawnImpactMark(this, Hit.ImpactPoint, Hit.ImpactNormal, KnifeMarkSize, KnifeMarkColor);
	}
	if (HitActor == nullptr || !HitActor->CanBeDamaged())
	{
		return;
	}
	float Damage = bStabbing ? StabDamage : SlashDamage;
	const FVector AttackerLocation = MyPawn != nullptr ? MyPawn->GetActorLocation() : Start;
	if (bStabbing && Victim != nullptr && IsBackstab(AttackerLocation, *Victim))
	{
		bLastAttackBackstab = true;
		Damage *= BackstabMultiplier;
	}
	const bool bWasAlive = Victim != nullptr && Victim->IsAlive();
	const float Taken = UGameplayStatics::ApplyPointDamage(
		HitActor, Damage, Direction, Hit, GetInstigatorController(), this, UDamageType::StaticClass());
	AShooterPlayerController* Player = Cast<AShooterPlayerController>(GetInstigatorController());
	if (Player != nullptr && Victim != nullptr && Taken > 0.0f)
	{
		Player->NotifyHitConfirmed(
			Victim->GetHitGroup(Hit.ImpactPoint) == EShooterHitGroup::Head, bWasAlive && !Victim->IsAlive());
	}
}

void AShooterWeapon_Knife::SimulateWeaponFire()
{
	MakeNoise(GetFireNoiseLoudness(), MyPawn, GetActorLocation());
	PlayWeaponSound(FireSound, GetFireVolume());
	PlayFireForceFeedback();
	(void)PlayWeaponAnimation(FireAnim);
}
