#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterCharacter.h"
#include "ShooterCharacterMovement.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "Weapons/ShooterProjectile.h"
#include "Weapons/ShooterWeapon_Instant.h"
#include "Weapons/ShooterWeapon_Projectile.h"
#include "Weapons/ShooterWeapon_Sniper.h"

#if WITH_DEV_AUTOMATION_TESTS

// P18's tests: the weapons (fire, ammunition, reloads, the deterministic spread and recoil, the sniper's zoom, the
// grenade), the damage rules (headshots, Counter-Strike's armor, friendly fire), death, the dropped weapon and its
// pickup. They run with the project's config (DefaultGame.ini's weapon sections and DefaultWeapons).

namespace
{

	constexpr float FrameTime = 1.0f / 60.0f;

	/** Ticks the world Frames times at 60 Hz. */
	void TickFrames(UWorld& World, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(FrameTime);
		}
	}

	/** A floor under the origin: a blocking box 80 x 80 m, its top at Z = 0. */
	void SpawnFloor(UWorld& World)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
	}

	/** A wall: a blocking box of Scale metres centred at Center. */
	void SpawnWall(UWorld& World, const FVector& Center, const FVector& Scale)
	{
		(void)World.SpawnActor<ABlockingVolume>(
			ABlockingVolume::StaticClass(), FTransform(FQuat::Identity, Center, Scale));
	}

	/** A character on the floor at Feet facing Yaw, possessed by a brainless bot controller of Team (None: no team). */
	AShooterCharacter* SpawnShooter(UWorld& World, const FVector& Feet, float Yaw, EShooterTeam Team)
	{
		AShooterCharacter* Character = World.SpawnActor<AShooterCharacter>(Feet, FRotator(0.0f, Yaw, 0.0f));
		AShooterAIController* Controller = World.SpawnActor<AShooterAIController>();
		// The test drives the pawn: the bot's brain stays off.
		Controller->bCanEverTick = false;
		if (AShooterPlayerState* State = Controller->GetPlayerState<AShooterPlayerState>())
		{
			State->SetTeam(Team);
		}
		Controller->Possess(Character);
		Controller->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
		return Character;
	}

	/** Gives a weapon, draws it and waits out its equip time. */
	template <class T>
	T* GiveAndDraw(UWorld& World, AShooterCharacter& Character)
	{
		T* Weapon = Cast<T>(Character.GiveWeapon(T::StaticClass()));
		Character.EquipWeapon(Weapon);
		TickFrames(World, FMath::CeilToInt((Weapon->EquipDuration + 0.1f) / FrameTime));
		return Weapon;
	}

	/** No spread, no recoil: a shot goes where the pawn looks. */
	void MakeAccurate(AShooterWeapon_Instant& Weapon)
	{
		Weapon.WeaponSpread = 0.0f;
		Weapon.MovingSpread = 0.0f;
		Weapon.JumpingSpread = 0.0f;
		Weapon.FiringSpreadIncrement = 0.0f;
		Weapon.RecoilPitch = 0.0f;
		Weapon.RecoilPitchRandom = 0.0f;
		Weapon.RecoilYawRandom = 0.0f;
	}

	/** One press and release of the trigger, with a frame between. */
	void Shoot(UWorld& World, AShooterCharacter& Character)
	{
		Character.StartWeaponFire();
		TickFrames(World, 1);
		Character.StopWeaponFire();
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameDamageArmorTest, "ShooterGame.Damage.Armor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameDamageArmorTest::RunTest(const FString& Parameters)
{
	// Counter-Strike's armor: health takes ArmorRatio / 2 of the damage and the armor half the rest; an empty armor
	// lets the rest through; a head without a helmet and damage without a ratio ignore it.
	float Health = 0.0f;
	float Armor = 0.0f;
	AShooterCharacter::ComputeArmorDamage(100.0f, 1.55f, false, false, 0.0f, Health, Armor);
	TestTrue("No armor: all to health", Health == 100.0f && Armor == 0.0f);
	AShooterCharacter::ComputeArmorDamage(40.0f, 1.55f, false, false, 100.0f, Health, Armor);
	TestEqual("Rifle vs kevlar: health", Health, 31.0f, 1.0e-4f);
	TestEqual("Rifle vs kevlar: armor", Armor, 4.5f, 1.0e-4f);
	AShooterCharacter::ComputeArmorDamage(40.0f, 1.0f, false, false, 100.0f, Health, Armor);
	TestTrue("Pistol vs kevlar: halved", FMath::IsNearlyEqual(Health, 20.0f) && FMath::IsNearlyEqual(Armor, 10.0f));
	AShooterCharacter::ComputeArmorDamage(40.0f, 1.0f, false, false, 4.0f, Health, Armor);
	TestTrue("Worn out: 4 armor stops 8", FMath::IsNearlyEqual(Health, 32.0f) && FMath::IsNearlyEqual(Armor, 4.0f));
	AShooterCharacter::ComputeArmorDamage(144.0f, 1.55f, true, false, 100.0f, Health, Armor);
	TestTrue("Head, no helmet", Health == 144.0f && Armor == 0.0f);
	AShooterCharacter::ComputeArmorDamage(144.0f, 1.55f, true, true, 100.0f, Health, Armor);
	TestEqual("Head with a helmet", Health, 111.6f, 1.0e-3f);
	AShooterCharacter::ComputeArmorDamage(460.0f, 1.95f, false, false, 100.0f, Health, Armor);
	TestEqual("AWP: nearly all through", Health, 448.5f, 1.0e-3f);
	AShooterCharacter::ComputeArmorDamage(30.0f, -1.0f, false, false, 100.0f, Health, Armor);
	TestTrue("The world ignores armor", Health == 30.0f && Armor == 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameDamageHeadshotTest, "ShooterGame.Damage.HeadshotAndBody",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameDamageHeadshotTest::RunTest(const FString& Parameters)
{
	// A rifle 10 m away: level from the eyes is the head (x4); aimed at the chest the body. The damage falls off with
	// the distance (0.98 per 1270 cm) and the armor takes its share.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	(void)World.SetGameMode(AShooterGameMode::StaticClass());
	AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterCharacter* Victim = SpawnShooter(World, FVector(1000.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
	AShooterWeapon_Instant* Rifle = GiveAndDraw<AShooterWeapon_Rifle>(World, *Shooter);
	if (!TestNotNull("A rifle", Rifle))
	{
		return false;
	}
	MakeAccurate(*Rifle);
	TestTrue("Drawn", Shooter->GetWeapon() == Rifle && Rifle->CanFire());
	TestEqual("Full health", Victim->GetHealth(), 100.0f);

	// Level: the eyes are at 163 cm, the head from 155 (183 - 28).
	Shoot(World, *Shooter);
	TestTrue("It hit the victim", Rifle->GetLastHit().GetActor() == Victim);
	const float Distance = FVector::Dist(Rifle->GetLastShotStart(), Rifle->GetLastHit().ImpactPoint);
	const float HeadDamage = 36.0f * FMath::Pow(0.98f, Distance / 1270.0f) * 4.0f;
	TestEqual("Headshot: dead", Victim->GetHealth(), 0.0f);
	TestFalse("Not alive", Victim->IsAlive());
	TestTrue("A headshot kills", HeadDamage > 100.0f);

	// A fresh victim with kevlar, shot in the chest.
	AShooterCharacter* Armored = SpawnShooter(World, FVector(1000.0f, 300.0f, 0.0f), 180.0f, EShooterTeam::T);
	Armored->SetArmor(100.0f, false);
	const FVector Chest = Armored->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);
	const FVector Eyes = Shooter->GetFirstPersonCameraComponent()->GetComponentLocation();
	Shooter->GetController()->SetControlRotation((Chest - Eyes).Rotation());
	TickFrames(World, 12);
	Shoot(World, *Shooter);
	TestTrue("It hit the armored victim", Rifle->GetLastHit().GetActor() == Armored);
	TestTrue("In the body", Armored->GetHitGroup(Rifle->GetLastHit().ImpactPoint) == EShooterHitGroup::Body);
	const float BodyDistance = FVector::Dist(Rifle->GetLastShotStart(), Rifle->GetLastHit().ImpactPoint);
	const float BodyDamage = 36.0f * FMath::Pow(0.98f, BodyDistance / 1270.0f);
	TestEqual("Health takes 77.5 %", Armored->GetHealth(), 100.0f - (BodyDamage * 0.775f), 1.0e-3f);
	TestEqual("Armor takes half the rest", Armored->GetArmor(), 100.0f - (BodyDamage * 0.225f * 0.5f), 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameDamageFriendlyFireTest, "ShooterGame.Damage.FriendlyFire",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameDamageFriendlyFireTest::RunTest(const FString& Parameters)
{
	// A teammate's bullet does nothing (mp_friendlyfire 0); an enemy's does; bFriendlyFire lets the teammate's through.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
	AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterCharacter* Mate = SpawnShooter(World, FVector(500.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::CT);
	AShooterWeapon_Instant* Pistol =
		Cast<AShooterWeapon_Instant>(Shooter->GetWeaponInSlot(EShooterWeaponSlot::Secondary));
	if (!TestNotNull("The default pistol", Pistol))
	{
		return false;
	}
	MakeAccurate(*Pistol);
	const FVector Chest = Mate->GetActorLocation() + FVector(0.0f, 0.0f, 110.0f);
	Shooter->GetController()->SetControlRotation(
		(Chest - Shooter->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation());
	TickFrames(World, 70);
	Shoot(World, *Shooter);
	TestTrue("Hit the teammate", Pistol->GetLastHit().GetActor() == Mate);
	TestEqual("No friendly fire", Mate->GetHealth(), 100.0f);

	GameMode->bFriendlyFire = true;
	TickFrames(World, 12);
	Shoot(World, *Shooter);
	TestTrue("Friendly fire on", Mate->GetHealth() < 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameWeaponsDeterministicSpreadTest, "ShooterGame.Weapons.DeterministicSpread",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameWeaponsDeterministicSpreadTest::RunTest(const FString& Parameters)
{
	// Two rifles with the same seed fire the same shots (spread and recoil from their FRandomStream); another seed
	// fires others. The firing spread grows by FiringSpreadIncrement a shot, and the recoil climbs and comes back.
	TArray<FVector> Directions[2];
	TArray<float> Pitches[2];
	for (int32 Run = 0; Run < 2; ++Run)
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		SpawnFloor(World);
		AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
		AShooterWeapon_Instant* Rifle = GiveAndDraw<AShooterWeapon_Rifle>(World, *Shooter);
		// The first shot goes with the press, the next ones with the fire rate while the trigger is held.
		Shooter->StartWeaponFire();
		Directions[Run].Add(Rifle->GetLastShotDirection());
		Pitches[Run].Add(Shooter->GetController()->GetControlRotation().Pitch);
		for (int32 Frame = 0; Frame < 60 && Rifle->GetShotsFired() < 5; ++Frame)
		{
			const int32 Before = Rifle->GetShotsFired();
			TickFrames(World, 1);
			if (Rifle->GetShotsFired() != Before)
			{
				Directions[Run].Add(Rifle->GetLastShotDirection());
				Pitches[Run].Add(Shooter->GetController()->GetControlRotation().Pitch);
			}
		}
		TestEqual("Five shots", Rifle->GetShotsFired(), 5);
		TestEqual("Firing spread", Rifle->GetCurrentFiringSpread(), 5.0f * Rifle->FiringSpreadIncrement, 1.0e-4f);
		TestTrue("The recoil climbed", Rifle->GetRecoilToRecover() > 0.0f);
		Shooter->StopWeaponFire();
		TickFrames(World, 120);
		TestEqual("The recoil came back", Rifle->GetRecoilToRecover(), 0.0f);
		TestEqual("The accuracy came back", Rifle->GetCurrentFiringSpread(), 0.0f);
	}
	if (!TestEqual("Same shot count", Directions[0].Num(), Directions[1].Num()))
	{
		return false;
	}
	for (int32 Index = 0; Index < Directions[0].Num(); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Shot %d replays"), Index), Directions[0][Index] == Directions[1][Index]);
		TestTrue(*FString::Printf(TEXT("Kick %d replays"), Index), Pitches[0][Index] == Pitches[1][Index]);
	}
	TestTrue("The spread moves the shots", !Directions[0][1].Equals(Directions[0][2], 1.0e-6f));
	TestTrue("The recoil kicks up", Pitches[0][4] > Pitches[0][0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameWeaponsSpreadModelTest, "ShooterGame.Weapons.SpreadModel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameWeaponsSpreadModelTest::RunTest(const FString& Parameters)
{
	// The spread adds the movement's share and the jump's, and shrinks crouched.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterWeapon_Instant* Rifle = GiveAndDraw<AShooterWeapon_Rifle>(World, *Shooter);
	const float Standing = Rifle->GetCurrentSpread();
	TestEqual("Standing", Standing, Rifle->WeaponSpread, 1.0e-4f);
	UCharacterMovementComponent& Movement = Shooter->GetCharacterMovement();
	Movement.Velocity = FVector(Movement.MaxWalkSpeed * 0.5f, 0.0f, 0.0f);
	TestEqual("Half speed", Rifle->GetCurrentSpread(), Rifle->WeaponSpread + (Rifle->MovingSpread * 0.5f), 1.0e-4f);
	Movement.Velocity = FVector::ZeroVector;
	Shooter->SetMovementMode(EMovementMode::Falling);
	TestEqual("In the air", Rifle->GetCurrentSpread(), Rifle->WeaponSpread + Rifle->JumpingSpread, 1.0e-4f);
	Shooter->SetMovementMode(EMovementMode::Walking);
	Shooter->Crouch();
	TickFrames(World, 2);
	TestTrue("Crouched", Shooter->bIsCrouched);
	TestEqual("Crouched spread", Rifle->GetCurrentSpread(), Rifle->WeaponSpread * Rifle->CrouchingSpreadMod, 1.0e-4f);
	TestEqual("Damage at 0 m", Rifle->GetDamageAtDistance(0.0f), Rifle->HitDamage);
	TestEqual("Damage at 12.7 m", Rifle->GetDamageAtDistance(1270.0f), Rifle->HitDamage * 0.98f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameWeaponsAmmoAndReloadTest, "ShooterGame.Weapons.AmmoAndReload",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameWeaponsAmmoAndReloadTest::RunTest(const FString& Parameters)
{
	// The pistol: semi-automatic (a held trigger fires once), 12 rounds, an empty clip reloads on the next press, the
	// reload takes ReloadDuration and moves the rounds from the reserve; R reloads a partial clip.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterWeapon* Pistol = Shooter->GetWeapon();
	if (!TestNotNull("Spawned with the pistol", Pistol) || !TestEqual("usp", Pistol->WeaponName, FString(TEXT("usp"))))
	{
		return false;
	}
	TickFrames(World, 70);
	Shooter->StartWeaponFire();
	TickFrames(World, 60);
	Shooter->StopWeaponFire();
	TestEqual("Held: one shot", Pistol->GetShotsFired(), 1);
	while (Pistol->GetCurrentAmmoInClip() > 0)
	{
		TickFrames(World, 10);
		Shoot(World, *Shooter);
	}
	TestEqual("Twelve shots", Pistol->GetShotsFired(), 12);
	TickFrames(World, 10);
	Shoot(World, *Shooter);
	TestTrue("Empty: reloading", Pistol->GetCurrentState() == EShooterWeaponState::Reloading);
	TickFrames(World, FMath::CeilToInt(Pistol->ReloadDuration / FrameTime) + 1);
	TestEqual("Full clip", Pistol->GetCurrentAmmoInClip(), 12);
	TestEqual("Reserve", Pistol->GetCurrentAmmo(), Pistol->MaxAmmo - 12);

	Shoot(World, *Shooter);
	Shooter->ReloadWeapon();
	TestTrue("R reloads", Pistol->GetCurrentState() == EShooterWeaponState::Reloading);
	TickFrames(World, FMath::CeilToInt(Pistol->ReloadDuration / FrameTime) + 1);
	TestEqual("Topped up", Pistol->GetCurrentAmmoInClip(), 12);
	TestEqual("One more from the reserve", Pistol->GetCurrentAmmo(), Pistol->MaxAmmo - 13);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameWeaponsSniperTest, "ShooterGame.Weapons.SniperZoom",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameWeaponsSniperTest::RunTest(const FString& Parameters)
{
	// The AWP: the secondary button steps 30.5 -> 7.5 -> no zoom; unscoped it is inaccurate; scoped the carrier is
	// slower; a shot leaves the scope and the bolt brings it back.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterWeapon_Sniper* Sniper = GiveAndDraw<AShooterWeapon_Sniper>(World, *Shooter);
	UCameraComponent* Camera = Shooter->GetFirstPersonCameraComponent();
	UShooterCharacterMovement* Movement = Shooter->GetShooterCharacterMovement();
	TestEqual("Unscoped spread", Sniper->GetCurrentSpread(), Sniper->WeaponSpread + Sniper->UnscopedSpread, 1.0e-4f);
	TestEqual("AWP speed", Movement->GetMaxSpeed(), 635.0f * Sniper->SpeedModifier, 0.01f);

	Shooter->StartSecondaryFire();
	TestEqual("First zoom", Camera->FieldOfView(), 30.5f, 1.0e-4f);
	TestEqual("Scoped spread", Sniper->GetCurrentSpread(), Sniper->WeaponSpread, 1.0e-4f);
	TestEqual("Scoped speed", Movement->GetMaxSpeed(), 635.0f * Sniper->ScopedSpeedModifier, 0.01f);
	TestFalse("No view model scoped", Sniper->GetMesh1P()->IsVisible());
	Shooter->StartSecondaryFire();
	TestEqual("Second zoom", Camera->FieldOfView(), 7.5f, 1.0e-4f);

	Shoot(World, *Shooter);
	TestFalse("The bolt leaves the scope", Sniper->IsZoomed());
	TestEqual("Default view", Camera->FieldOfView(), AShooterCharacter::GetDefaultFieldOfView(), 1.0e-4f);
	TickFrames(World, FMath::CeilToInt(Sniper->TimeBetweenShots / FrameTime) + 1);
	TestEqual("Back to the zoom", Sniper->GetZoomLevel(), 2);
	Shooter->StartSecondaryFire();
	TestFalse("A third press: no zoom", Sniper->IsZoomed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameWeaponsGrenadeTest, "ShooterGame.Weapons.Grenade",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameWeaponsGrenadeTest::RunTest(const FString& Parameters)
{
	// The HE grenade: the throw spawns a projectile and the grenade leaves the inventory (the pistol is drawn); after
	// the fuse it explodes: a pawn in the open takes the falloff's damage, one behind a wall none.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterCharacter* Thrower = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterWeapon_Projectile* Grenade = GiveAndDraw<AShooterWeapon_Grenade>(World, *Thrower);
	if (!TestNotNull("A grenade", Grenade))
	{
		return false;
	}
	TestTrue("The grenade slot", Grenade->Slot == EShooterWeaponSlot::Grenade);
	Thrower->StartWeaponFire();
	AShooterProjectile* Projectile = Grenade->GetLastProjectile();
	Thrower->StopWeaponFire();
	if (!TestNotNull("Thrown", Projectile))
	{
		return false;
	}
	TestNull("Gone from the inventory", Thrower->GetWeaponInSlot(EShooterWeaponSlot::Grenade));
	TestTrue("The pistol is drawn", Thrower->GetWeapon() == Thrower->GetWeaponInSlot(EShooterWeaponSlot::Secondary));
	TestTrue("Flying forward", Projectile->GetMovementComponent()->Velocity.X > 1000.0f);

	// A second grenade, placed by hand between two victims: one 300 cm away in the open, one behind a wall.
	AShooterCharacter* Open = SpawnShooter(World, FVector(0.0f, 1300.0f, 0.0f), 0.0f, EShooterTeam::T);
	AShooterCharacter* Covered = SpawnShooter(World, FVector(0.0f, 700.0f, 0.0f), 0.0f, EShooterTeam::T);
	SpawnWall(World, FVector(0.0f, 850.0f, 150.0f), FVector(3.0f, 0.2f, 3.0f));
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Thrower;
	AShooterProjectile* Placed =
		World.SpawnActor<AShooterProjectile>(FVector(0.0f, 1000.0f, 5.0f), FRotator::ZeroRotator, SpawnInfo);
	Placed->Launch(FVector::ZeroVector, Thrower->GetController(), nullptr);
	TickFrames(World, 1);
	Placed->Explode();
	TestTrue("Exploded", Placed->HasExploded());
	const FVector Origin(0.0f, 1000.0f, 5.0f + 10.0f);
	const float Expected =
		98.0f * (1.0f - (FVector::Dist(Origin, Open->GetActorLocation() + FVector(0.0f, 0.0f, 91.5f)) / 889.0f));
	TestTrue("The open victim is hurt", Open->GetHealth() < 100.0f);
	TestEqual("The falloff", 100.0f - Open->GetHealth(), Expected, 8.0f);
	TestEqual("The wall shields", Covered->GetHealth(), 100.0f);

	// The thrown one explodes on its fuse.
	TickFrames(World, FMath::CeilToInt(Grenade->FuseTime / FrameTime) + 2);
	TestTrue("The fuse ran out", Projectile->HasExploded());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameCharacterDeathDropsWeaponTest, "ShooterGame.Character.DeathDropsWeapon",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameCharacterDeathDropsWeaponTest::RunTest(const FString& Parameters)
{
	// A pawn with a rifle and the pistol dies: the game mode counts the kill, the rifle lies on the floor with its
	// ammunition, the pistol is gone, the corpse no longer collides and the bot let it go. A pawn without a rifle that
	// walks over it after the pickup delay takes it; one that has a rifle does not.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
	AShooterCharacter* Victim = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::T);
	AShooterWeapon* Rifle = GiveAndDraw<AShooterWeapon_Rifle>(World, *Victim);
	AShooterWeapon* Pistol = Victim->GetWeaponInSlot(EShooterWeaponSlot::Secondary);
	Shoot(World, *Victim);
	const int32 ClipLeft = Rifle->GetCurrentAmmoInClip();
	TestEqual("Two weapons", Victim->GetInventory().Num(), 2);

	AController* VictimController = Victim->GetController();
	Victim->Suicide();
	TestFalse("Dead", Victim->IsAlive());
	TestEqual("The kill", GameMode->GetNumKills(), 1);
	TestTrue("The team stays with the corpse", Victim->GetTeam() == EShooterTeam::T);
	TestTrue("The rifle is on the floor", Rifle->IsDropped() && Rifle->GetPawnOwner() == nullptr);
	TestEqual("With its rounds", Rifle->GetCurrentAmmoInClip(), ClipLeft);
	TestTrue("The pistol is gone", Pistol->IsPendingKillPending());
	TestEqual("Empty inventory", Victim->GetInventory().Num(), 0);
	TestFalse("No collision", Victim->GetCapsuleComponent()->IsCollisionEnabled());
	TestNull("The bot let go", VictimController->GetPawn());
	TestEqual("On the floor", Rifle->GetActorLocation().Z, 0.0f, 0.5f);
	int32 NumCT = 0;
	int32 NumT = 0;
	GameMode->CountPawns(NumCT, NumT);
	TestEqual("No live T", NumT, 0);

	const FVector RiflePlace = Rifle->GetActorLocation();
	// Two pawns 50 cm to each side (clear of each other's capsule, within the pickup radius).
	AShooterCharacter* Armed = SpawnShooter(World, RiflePlace + FVector(0.0f, 50.0f, 0.0f), 0.0f, EShooterTeam::CT);
	AShooterWeapon* ArmedRifle = Armed->GiveWeapon(AShooterWeapon_Sniper::StaticClass());
	AShooterCharacter* Taker = SpawnShooter(World, RiflePlace + FVector(0.0f, -50.0f, 0.0f), 0.0f, EShooterTeam::CT);
	TickFrames(World, 1);
	TestTrue("Not before the delay", Rifle->IsDropped());
	TickFrames(World, 70);
	TestFalse("Picked up", Rifle->IsDropped());
	TestTrue("By the pawn without a primary", Rifle->GetPawnOwner() == Taker);
	TestTrue("The armed pawn kept its own", Armed->GetWeaponInSlot(EShooterWeaponSlot::Primary) == ArmedRifle);
	TestEqual("Still its rounds", Rifle->GetCurrentAmmoInClip(), ClipLeft);

	// G drops the weapon in hand and draws the next.
	Taker->EquipWeapon(Rifle);
	TestTrue("Dropped by hand", Taker->DropWeapon(Rifle) && Rifle->IsDropped());
	Taker->EquipBestWeapon();
	TestTrue("The pistol again", Taker->GetWeapon() == Taker->GetWeaponInSlot(EShooterWeaponSlot::Secondary));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
