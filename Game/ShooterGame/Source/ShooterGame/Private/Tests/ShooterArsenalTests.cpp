#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ShooterAIController.h"
#include "ShooterCharacter.h"
#include "ShooterCharacterMovement.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/UObjectGlobals.h"
#include "Weapons/ShooterWeapon_AWP.h"
#include "Weapons/ShooterWeapon_Instant.h"
#include "Weapons/ShooterWeapon_Knife.h"
#include "Weapons/ShooterWeapon_Projectile.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-shipping N30a's tests: Counter-Strike 1.6's arsenal (each weapon's stats, the silencers, the Glock's burst, the
// knife and its backstab), the bullets through walls by material (CS's penetration) and the hit groups with and
// without armor. They run with the project's config (DefaultGame.ini).

namespace
{

	constexpr float FrameTime = 1.0f / 60.0f;

	/** CS's unit, cm. */
	constexpr float Unit = 2.54f;

	void TickFrames(UWorld& World, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(FrameTime);
		}
	}

	void TickSeconds(UWorld& World, float Seconds)
	{
		TickFrames(World, FMath::CeilToInt(Seconds / FrameTime) + 1);
	}

	/** A floor under the origin: a blocking box 80 x 80 m, its top at Z = 0. */
	void SpawnFloor(UWorld& World)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
	}

	/** A character on the floor at Feet facing Yaw, possessed by a brainless bot controller of Team. */
	AShooterCharacter* SpawnShooter(UWorld& World, const FVector& Feet, float Yaw, EShooterTeam Team)
	{
		AShooterCharacter* Character = World.SpawnActor<AShooterCharacter>(Feet, FRotator(0.0f, Yaw, 0.0f));
		AShooterAIController* Controller = World.SpawnActor<AShooterAIController>();
		Controller->SetActorTickEnabled(false);
		if (AShooterPlayerState* State = Controller->GetPlayerState<AShooterPlayerState>())
		{
			State->SetTeam(Team);
		}
		Controller->Possess(Character);
		Controller->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
		return Character;
	}

	/** Gives a weapon, draws it and waits out its draw. */
	AShooterWeapon* GiveAndDraw(UWorld& World, AShooterCharacter& Character, UClass* WeaponClass)
	{
		AShooterWeapon* Weapon = Character.GiveWeapon(WeaponClass);
		Character.EquipWeapon(Weapon);
		TickSeconds(World, 1.5f);
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
		if (AShooterWeapon_AWP* Awp = Cast<AShooterWeapon_AWP>(&Weapon))
		{
			Awp->UnscopedSpread = 0.0f;
		}
	}

	/** Turns the shooter's view to a point. */
	void AimAt(AShooterCharacter& Shooter, const FVector& Target)
	{
		const FVector Eyes = Shooter.GetFirstPersonCameraComponent()->GetComponentLocation();
		Shooter.GetController()->SetControlRotation((Target - Eyes).Rotation());
	}

	/** One press and release of the trigger, with a frame between. */
	void Shoot(UWorld& World, AShooterCharacter& Character)
	{
		Character.StartWeaponFire();
		TickFrames(World, 1);
		Character.StopWeaponFire();
	}

	/** The chest of a standing pawn: 130 cm above its feet (the chest band: 116 to 155). */
	FVector GetChest(const AShooterCharacter& Pawn)
	{
		return Pawn.GetActorLocation() + FVector(0.0f, 0.0f, 130.0f);
	}

	/**
	 * A wall across the X axis: the engine's cube as a static mesh actor, Thickness cm along X from its centre at X,
	 * 4 m wide and 3 m tall on the floor, its material's physical material the surface's (as de_leon's, N30f).
	 */
	AStaticMeshActor* SpawnSurfaceWall(UWorld& World, float X, float Thickness, EPhysicalSurface Surface)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		AStaticMeshActor* Wall = World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
			FTransform(FQuat::Identity, FVector(X, 0.0f, 150.0f), FVector(Thickness / 100.0f, 4.0f, 3.0f)));
		if (Wall == nullptr || Cube == nullptr)
		{
			return nullptr;
		}
		// A map's static mesh: its triangles block the traces (as the map import sets them up).
		UStaticMeshComponent* Mesh = Wall->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Static);
		(void)Mesh->SetStaticMesh(Cube);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		UMaterial* Material = NewObject<UMaterial>(Wall, FName(TEXT("M_TestWall")));
		if (Surface != SHOOTER_SURFACE_Default)
		{
			UPhysicalMaterial* PhysMaterial = NewObject<UPhysicalMaterial>(Material, FName(TEXT("PM_TestWall")));
			PhysMaterial->SurfaceType = Surface;
			Material->PhysMaterial = PhysMaterial;
		}
		Mesh->SetMaterial(0, Material);
		return Wall;
	}

	/** The surfaces the walls are made of (ShooterGame.h's SHOOTER_SURFACE_*). */
	namespace TestSurfaces
	{
		constexpr EPhysicalSurface Wood = SHOOTER_SURFACE_Wood;
		constexpr EPhysicalSurface Concrete = SHOOTER_SURFACE_Concrete;
		constexpr EPhysicalSurface Metal = SHOOTER_SURFACE_Metal;
		constexpr EPhysicalSurface Glass = SHOOTER_SURFACE_Glass;
		constexpr EPhysicalSurface Tile = SHOOTER_SURFACE_Tile;
		constexpr EPhysicalSurface Default = SHOOTER_SURFACE_Default;
	} // namespace TestSurfaces

	/** A wall to shoot through: its surface, its thickness (cm) and where its centre is along X (cm). */
	struct FTestWall
	{
		EPhysicalSurface Surface = SHOOTER_SURFACE_Default;
		float Thickness = 10.0f;
		float X = 400.0f;
	};

	/** What a shot through walls did to a victim behind them. */
	struct FPenetrationResult
	{
		/** The victim's health lost and what the model expects of the walls' damage shares. */
		float Taken = 0.0f;
		float Expected = 0.0f;
		/** The shot reached the victim. */
		bool bReached = false;
		int32 Penetrations = 0;
	};

	/**
	 * A CT at the origin shoots WeaponClass (made accurate) at the chest of a terrorist VictimX cm ahead, through
	 * Walls. Expected is the weapon's damage over the path outside the walls, times each wall's damage share (CS's).
	 */
	FPenetrationResult ShootThroughWalls(UClass* WeaponClass, TArrayView<const FTestWall> Walls, float VictimX = 800.0f)
	{
		FPenetrationResult Result;
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		SpawnFloor(World);
		for (const FTestWall& Wall : Walls)
		{
			(void)SpawnSurfaceWall(World, Wall.X, Wall.Thickness, Wall.Surface);
		}
		AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
		AShooterCharacter* Victim = SpawnShooter(World, FVector(VictimX, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
		AShooterWeapon_Instant* Weapon = Cast<AShooterWeapon_Instant>(GiveAndDraw(World, *Shooter, WeaponClass));
		if (Weapon == nullptr)
		{
			return Result;
		}
		MakeAccurate(*Weapon);
		AimAt(*Shooter, GetChest(*Victim));
		TickFrames(World, 1);
		Shoot(World, *Shooter);
		Result.Taken = Victim->GetMaxHealth() - Victim->GetHealth();
		Result.Penetrations = Weapon->GetLastShotPenetrations();
		Result.bReached = Weapon->GetLastShotEnd().X > VictimX - 60.0f;
		// The path outside the walls (each wall's thickness along the shot left out) and the walls' shares.
		const FVector Direction = Weapon->GetLastShotDirection();
		// The shot meets the victim's capsule, straight ahead, a radius before its axis.
		const float VictimFront = VictimX - Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
		float Outside = (VictimFront - Weapon->GetLastShotStart().X) / FMath::Max(Direction.X, 0.01f);
		float Share = 1.0f;
		for (const FTestWall& Wall : Walls)
		{
			float PowerScale = 1.0f;
			float DamageScale = 1.0f;
			AShooterWeapon_Instant::GetSurfacePenetration(Wall.Surface, PowerScale, DamageScale);
			Outside -= (Wall.Thickness / FMath::Max(Direction.X, 0.01f)) + 0.5f;
			Share *= DamageScale;
		}
		Result.Expected =
			Weapon->HitDamage * FMath::Pow(Weapon->RangeModifier, Outside / Weapon->RangeModifierDistance) * Share;
		return Result;
	}

	/** One row of the arsenal's table: CS 1.6's values (units converted at 2.54 cm). */
	struct FWeaponRow
	{
		const TCHAR* Name;
		UClass* Class;
		EShooterWeaponSlot Slot;
		EShooterTeam BuyTeam;
		int32 Price;
		int32 AmmoPerClip;
		int32 MaxAmmo;
		float TimeBetweenShots;
		float Damage;
		float RangeModifier;
		float ArmorRatio;
		/** CS's speed with it drawn, units a second (the run's 250 is 1). */
		float Speed;
		int32 PenetrationCount;
		/** CS's penetration power and distance, units. */
		float PenetrationPower;
		float PenetrationDistance;
		int32 AmmoBoxRounds;
		int32 AmmoBoxPrice;
		float ReloadDuration;
	};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameArsenalStatsTableTest, "ShooterGame.Arsenal.StatsTable",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameArsenalStatsTableTest::RunTest(const FString& Parameters)
{
	// Every weapon is Counter-Strike 1.6's (the class defaults with the project's config): the table below, and no
	// other weapon class (the generic pistol, rifle and sniper are gone).
	using S = EShooterWeaponSlot;
	using T = EShooterTeam;
	const FWeaponRow Rows[] = {
		{TEXT("glock"), AShooterWeapon_Glock::StaticClass(), S::Secondary, T::None, 400, 20, 120, 0.2f, 25.0f, 0.75f,
			1.05f, 250.0f, 1, 21.0f, 800.0f, 30, 20, 2.2f},
		{TEXT("usp"), AShooterWeapon_USP::StaticClass(), S::Secondary, T::None, 500, 12, 100, 0.225f, 34.0f, 0.79f,
			1.0f, 250.0f, 1, 15.0f, 500.0f, 12, 25, 2.7f},
		{TEXT("deagle"), AShooterWeapon_Deagle::StaticClass(), S::Secondary, T::None, 650, 7, 35, 0.3f, 54.0f, 0.81f,
			1.5f, 250.0f, 2, 30.0f, 1000.0f, 7, 40, 2.2f},
		{TEXT("mp5"), AShooterWeapon_MP5::StaticClass(), S::Primary, T::None, 1500, 30, 120, 0.075f, 26.0f, 0.84f, 1.0f,
			250.0f, 1, 21.0f, 800.0f, 30, 20, 2.63f},
		{TEXT("ak47"), AShooterWeapon_AK47::StaticClass(), S::Primary, T::T, 2500, 30, 90, 0.0955f, 36.0f, 0.98f, 1.55f,
			221.0f, 2, 39.0f, 5000.0f, 30, 80, 2.5f},
		{TEXT("m4a1"), AShooterWeapon_M4A1::StaticClass(), S::Primary, T::CT, 3100, 30, 90, 0.0875f, 32.0f, 0.97f, 1.4f,
			230.0f, 2, 35.0f, 4000.0f, 30, 60, 3.05f},
		{TEXT("awp"), AShooterWeapon_AWP::StaticClass(), S::Primary, T::None, 4750, 10, 30, 1.45f, 115.0f, 0.99f, 1.95f,
			210.0f, 3, 45.0f, 8000.0f, 10, 125, 3.7f},
	};
	TArray<UClass*> Listed;
	for (const FWeaponRow& Row : Rows)
	{
		const UClass* Found = AShooterWeapon::FindWeaponClass(Row.Name);
		if (!TestTrue(*FString::Printf(TEXT("%s: its class"), Row.Name), Found == Row.Class))
		{
			continue;
		}
		Listed.Add(Row.Class);
		const AShooterWeapon_Instant* W = Row.Class->GetDefaultObject<AShooterWeapon_Instant>();
		const FString Name(Row.Name);
		TestTrue(*(Name + TEXT(": slot and team")), W->Slot == Row.Slot && W->BuyTeam == Row.BuyTeam);
		TestEqual(*(Name + TEXT(": price")), W->Price, Row.Price);
		TestTrue(*(Name + TEXT(": clip and reserve")), W->AmmoPerClip == Row.AmmoPerClip && W->MaxAmmo == Row.MaxAmmo);
		TestEqual(*(Name + TEXT(": cycle")), W->TimeBetweenShots, Row.TimeBetweenShots, 1.0e-4f);
		TestEqual(*(Name + TEXT(": damage")), W->HitDamage, Row.Damage, 1.0e-4f);
		TestEqual(*(Name + TEXT(": range modifier")), W->RangeModifier, Row.RangeModifier, 1.0e-4f);
		TestEqual(*(Name + TEXT(": armor ratio")), W->ArmorRatio, Row.ArmorRatio, 1.0e-4f);
		TestEqual(*(Name + TEXT(": speed")), W->SpeedModifier * 250.0f, Row.Speed, 1.0f);
		TestEqual(*(Name + TEXT(": penetrations")), W->PenetrationCount, Row.PenetrationCount);
		TestEqual(*(Name + TEXT(": penetration power")), W->PenetrationPower, Row.PenetrationPower * Unit, 0.01f);
		TestEqual(
			*(Name + TEXT(": penetration distance")), W->PenetrationDistance, Row.PenetrationDistance * Unit, 0.01f);
		TestTrue(*(Name + TEXT(": its ammunition box")),
			W->AmmoBoxRounds == Row.AmmoBoxRounds && W->AmmoBoxPrice == Row.AmmoBoxPrice);
		TestEqual(*(Name + TEXT(": reload")), W->ReloadDuration, Row.ReloadDuration, 1.0e-4f);
		TestEqual(*(Name + TEXT(": kill reward (CS 1.6: $300)")), W->KillReward, 300);
		TestEqual(*(Name + TEXT(": head x4")), W->HeadshotMultiplier, 4.0f);
		TestTrue(*(Name + TEXT(": its models")), !W->MeshName.IsNull() && !W->FirstPersonMeshName.IsNull());
	}

	// The knife: every player's, not for sale, never out of ammunition; 15 a slash, 65 a stab, three times in the back.
	const AShooterWeapon_Knife* Knife = GetDefault<AShooterWeapon_Knife>();
	TestTrue("The knife", AShooterWeapon::FindWeaponClass(TEXT("knife")) == AShooterWeapon_Knife::StaticClass());
	TestTrue("Its slot, no price, no ammunition",
		Knife->Slot == S::Knife && !Knife->CanBeBoughtBy(T::CT) && !Knife->CanBeBoughtBy(T::T) && Knife->bInfiniteClip);
	TestTrue("Slash and stab (CS: 15 and 65 within 48 and 32 units)",
		Knife->SlashDamage == 15.0f && Knife->StabDamage == 65.0f && Knife->BackstabMultiplier == 3.0f &&
			FMath::IsNearlyEqual(Knife->SlashRange, 48.0f * Unit) &&
			FMath::IsNearlyEqual(Knife->StabRange, 32.0f * Unit));
	TestEqual("The knife's armor ratio (CS: 1.7)", Knife->ArmorRatio, 1.7f);
	Listed.Add(AShooterWeapon_Knife::StaticClass());
	// The grenades (N30b).
	Listed.Add(AShooterWeapon::FindWeaponClass(TEXT("hegrenade")));
	Listed.Add(AShooterWeapon::FindWeaponClass(TEXT("flashbang")));
	Listed.Add(AShooterWeapon::FindWeaponClass(TEXT("smokegrenade")));

	TArray<UClass*> Classes;
	AShooterWeapon::GetWeaponClasses(Classes);
	for (UClass* Class : Classes)
	{
		TestTrue(*FString::Printf(TEXT("%s is in the table"), *Class->GetName()), Listed.Contains(Class));
	}
	TestEqual("No other weapon", Classes.Num(), Listed.Num());

	// A spawned weapon: a full clip and an empty reserve (CS 1.6); a pawn's first pistol has two clips more (the game
	// mode's player states hold the teams).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	(void)World.SetGameMode(AShooterGameMode::StaticClass());
	AShooterCharacter* CT = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterCharacter* Terrorist = SpawnShooter(World, FVector(300.0f, 0.0f, 0.0f), 0.0f, EShooterTeam::T);
	const AShooterWeapon* Usp = CT->GetWeaponInSlot(S::Secondary);
	const AShooterWeapon* Glock = Terrorist->GetWeaponInSlot(S::Secondary);
	TestTrue("The CT's USP 12/24",
		Usp != nullptr && Usp->WeaponName == TEXT("usp") && Usp->GetCurrentAmmoInClip() == 12 &&
			Usp->GetCurrentAmmo() == 24);
	TestTrue("The T's Glock 20/40",
		Glock != nullptr && Glock->WeaponName == TEXT("glock") && Glock->GetCurrentAmmoInClip() == 20 &&
			Glock->GetCurrentAmmo() == 40);
	TestTrue("Both drawn, both with a knife",
		CT->GetWeapon() == Usp && Terrorist->GetWeapon() == Glock && CT->GetWeaponInSlot(S::Knife) != nullptr &&
			Terrorist->GetWeaponInSlot(S::Knife) != nullptr);
	AShooterWeapon* Bought = CT->GiveWeapon(AShooterWeapon_M4A1::StaticClass());
	if (!TestNotNull("An M4A1", Bought))
	{
		return false;
	}
	TestTrue("A new M4A1: 30/0", Bought->GetCurrentAmmoInClip() == 30 && Bought->GetCurrentAmmo() == 0);
	CT->EquipWeapon(Bought);
	const UShooterCharacterMovement* Movement = CT->GetShooterCharacterMovement();
	TestTrue("With the M4A1 drawn: 230 units a second",
		Movement != nullptr && FMath::IsNearlyEqual(Movement->GetMaxSpeed(), 230.0f * Unit, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameArsenalSilencerAndBurstTest, "ShooterGame.Arsenal.SilencerAndBurst",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameArsenalSilencerAndBurstTest::RunTest(const FString& Parameters)
{
	// The secondary button: the USP's silencer (3 s with no shot, then 30 damage, quieter to the bots and the ear),
	// the M4A1's (2 s; 33 damage falling off faster, a quarter more spread), the Glock's burst (three rounds a press,
	// 0.1 s apart, the next press 0.5 s later, five times the spread).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	(void)World.SetGameMode(AShooterGameMode::StaticClass());
	AShooterCharacter* CT = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	TickSeconds(World, 1.2f);
	AShooterWeapon_Instant* Usp = Cast<AShooterWeapon_Instant>(CT->GetWeapon());
	if (!TestTrue("The USP drawn", Usp != nullptr && Usp->WeaponName == TEXT("usp")))
	{
		return false;
	}
	TestTrue("Unsilenced: 34 damage, heard at 1",
		!Usp->IsSilenced() && Usp->GetDamageAtDistance(0.0f) == 34.0f && Usp->GetFireNoiseLoudness() == 1.0f);
	CT->StartSecondaryFire();
	TestTrue("The silencer goes on", Usp->IsSilenced() && Usp->GetCurrentState() == EShooterWeaponState::Equipping);
	Shoot(World, *CT);
	TestEqual("No shot while it goes on", Usp->GetShotsFired(), 0);
	TickSeconds(World, Usp->SilencerDuration - 0.2f);
	Shoot(World, *CT);
	TestEqual("Still not", Usp->GetShotsFired(), 0);
	TickSeconds(World, 0.3f);
	Shoot(World, *CT);
	TestEqual("Then it fires", Usp->GetShotsFired(), 1);
	TestTrue("Silenced: 30 damage, much quieter",
		Usp->GetDamageAtDistance(0.0f) == 30.0f && Usp->GetFireNoiseLoudness() < 0.5f * Usp->FireNoiseLoudness);
	TickSeconds(World, 0.3f);
	CT->StartSecondaryFire();
	TickSeconds(World, Usp->SilencerDuration + 0.1f);
	TestTrue("And off again", !Usp->IsSilenced() && Usp->GetDamageAtDistance(0.0f) == 34.0f);

	AShooterWeapon_Instant* M4 =
		Cast<AShooterWeapon_Instant>(GiveAndDraw(World, *CT, AShooterWeapon_M4A1::StaticClass()));
	if (!TestNotNull("An M4A1", M4))
	{
		return false;
	}
	const float Spread = M4->GetCurrentSpread();
	TestEqual("Unsilenced: 32 at 12.7 m x 0.97", M4->GetDamageAtDistance(1270.0f), 32.0f * 0.97f, 1.0e-3f);
	CT->StartSecondaryFire();
	TickSeconds(World, M4->SilencerDuration + 0.1f);
	TestTrue("Silenced in 2 s", M4->IsSilenced() && M4->CanFire() && M4->SilencerDuration == 2.0f);
	TestEqual("Silenced: 33 x 0.95", M4->GetDamageAtDistance(1270.0f), 33.0f * 0.95f, 1.0e-3f);
	TestEqual("A quarter more spread", M4->GetCurrentSpread(), Spread * 1.25f, 1.0e-4f);

	AShooterCharacter* Terrorist = SpawnShooter(World, FVector(0.0f, 500.0f, 0.0f), 0.0f, EShooterTeam::T);
	TickSeconds(World, 1.2f);
	AShooterWeapon_Instant* Glock = Cast<AShooterWeapon_Instant>(Terrorist->GetWeapon());
	if (!TestTrue("The Glock drawn", Glock != nullptr && Glock->WeaponName == TEXT("glock")))
	{
		return false;
	}
	const float SemiSpread = Glock->GetCurrentSpread();
	Terrorist->StartSecondaryFire();
	TestTrue("Burst mode", Glock->IsBurstMode() && Glock->CanFire());
	TestEqual("Five times the spread", Glock->GetCurrentSpread(), SemiSpread * 5.0f, 1.0e-4f);
	Shoot(World, *Terrorist);
	TestEqual("A press fires at once", Glock->GetShotsFired(), 1);
	TickSeconds(World, 0.25f);
	TestEqual("Three rounds, the trigger released", Glock->GetShotsFired(), 3);
	Shoot(World, *Terrorist);
	TestEqual("The next press waits", Glock->GetShotsFired(), 3);
	TickSeconds(World, 0.3f);
	Shoot(World, *Terrorist);
	TickSeconds(World, 0.3f);
	TestEqual("A second burst 0.5 s after the first", Glock->GetShotsFired(), 6);
	TestEqual("Six rounds used", Glock->GetCurrentAmmoInClip(), 14);
	Terrorist->StartSecondaryFire();
	TestFalse("Semi-automatic again", Glock->IsBurstMode());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameArsenalKnifeTest, "ShooterGame.Arsenal.KnifeBackstab",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameArsenalKnifeTest::RunTest(const FString& Parameters)
{
	// The knife (key 3): a slash cuts 15 in the chest, a stab 65, a stab in the back three times as much (195: dead),
	// a slash in the back only 15; out of its reach it cuts the air. It cannot be dropped and never runs out.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterCharacter* Attacker = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterCharacter* Facing = SpawnShooter(World, FVector(70.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
	Attacker->SelectSlot(EShooterWeaponSlot::Knife);
	AShooterWeapon_Knife* Knife = Cast<AShooterWeapon_Knife>(Attacker->GetWeapon());
	if (!TestNotNull("The knife drawn", Knife))
	{
		return false;
	}
	TickSeconds(World, 1.5f);
	TestTrue("Always armed", Knife->HasAmmo() && !Knife->CanBeDropped() && !Knife->CanReload());
	AimAt(*Attacker, GetChest(*Facing));
	TickFrames(World, 1);
	Shoot(World, *Attacker);
	TestTrue("It cut the terrorist", Knife->GetLastHit().GetActor() == Facing);
	TestEqual("A slash: 15", Facing->GetHealth(), 85.0f, 1.0e-3f);
	TickSeconds(World, Knife->TimeBetweenShots);
	Attacker->StartSecondaryFire();
	TestTrue("A stab", Knife->WasLastAttackStab() && !Knife->WasLastAttackBackstab());
	TestEqual("A stab from the front: 65", Facing->GetHealth(), 20.0f, 1.0e-3f);
	Attacker->StartSecondaryFire();
	TestEqual("The next waits the stab's 1.1 s", Facing->GetHealth(), 20.0f, 1.0e-3f);

	// Behind a terrorist that looks away: the stab kills (195), a slash does not count as in the back.
	AShooterCharacter* Back = SpawnShooter(World, FVector(0.0f, 600.0f, 0.0f), 0.0f, EShooterTeam::T);
	Attacker->Reset(FVector(-70.0f, 600.0f, 0.0f), FRotator::ZeroRotator);
	TickFrames(World, 1);
	TestTrue("In the back", AShooterWeapon_Knife::IsBackstab(Attacker->GetActorLocation(), *Back));
	TestFalse("Not from the front", AShooterWeapon_Knife::IsBackstab(Attacker->GetActorLocation(), *Facing));
	AimAt(*Attacker, GetChest(*Back));
	TickSeconds(World, Knife->StabCycleTime);
	Shoot(World, *Attacker);
	TestEqual("A slash in the back: 15", Back->GetHealth(), 85.0f, 1.0e-3f);
	TickSeconds(World, Knife->TimeBetweenShots);
	Attacker->StartSecondaryFire();
	TestTrue("A backstab", Knife->WasLastAttackBackstab());
	TestFalse("Dead", Back->IsAlive());

	// Out of reach.
	AShooterCharacter* Far = SpawnShooter(World, FVector(0.0f, -600.0f, 0.0f), 180.0f, EShooterTeam::T);
	Attacker->Reset(FVector(-250.0f, -600.0f, 0.0f), FRotator::ZeroRotator);
	TickFrames(World, 1);
	AimAt(*Attacker, GetChest(*Far));
	TickSeconds(World, Knife->StabCycleTime);
	Attacker->StartSecondaryFire();
	TestTrue("The air", !Knife->GetLastHit().bBlockingHit && Far->GetHealth() == 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameWeaponsPenetrationTest, "ShooterGame.Weapons.Penetration",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameWeaponsPenetrationTest::RunTest(const FString& Parameters)
{
	// CS 1.6's wall penetration: a bullet leaves a surface within its power (the AK-47's 39 units, 99 cm, a quarter
	// of it in concrete, 15 % in metal), keeping the material's share of the damage (wood 0.6, concrete and glass 0.5,
	// metal 0.2); a pistol pierces nothing; the AWP two walls; beyond the penetration distance nothing is pierced; a
	// body lets the bullet through with three quarters of the damage. Each wall's surface is its material's physical
	// material's (N30f; N30a named the materials in a table).
	namespace ES = TestSurfaces;
	UClass* AK = AShooterWeapon_AK47::StaticClass();
	UClass* AWP = AShooterWeapon_AWP::StaticClass();
	auto Check = [this](const TCHAR* What, const FPenetrationResult& Result, bool bThrough)
	{
		if (!bThrough)
		{
			TestTrue(*FString::Printf(TEXT("%s: stopped"), What), !Result.bReached && Result.Taken == 0.0f);
			return;
		}
		TestTrue(*FString::Printf(TEXT("%s: through"), What), Result.bReached && Result.Penetrations > 0);
		TestEqual(*FString::Printf(TEXT("%s: the damage kept"), What), Result.Taken, Result.Expected, 0.05f);
	};
	const FTestWall Crate[] = {{ES::Wood, 80.0f}};
	Check(TEXT("AK-47, 80 cm of wood"), ShootThroughWalls(AK, Crate), true);
	const FTestWall BigCrate[] = {{ES::Wood, 110.0f}};
	Check(TEXT("AK-47, de_leon's 110 cm crate"), ShootThroughWalls(AK, BigCrate), false);
	Check(TEXT("AWP, the same crate"), ShootThroughWalls(AWP, BigCrate), true);
	const FTestWall Concrete[] = {{ES::Concrete, 20.0f}};
	Check(TEXT("AK-47, 20 cm of concrete"), ShootThroughWalls(AK, Concrete), true);
	const FTestWall ThickConcrete[] = {{ES::Concrete, 30.0f}};
	Check(TEXT("AK-47, 30 cm of concrete"), ShootThroughWalls(AK, ThickConcrete), false);
	const FTestWall Metal[] = {{ES::Metal, 12.0f}};
	Check(TEXT("AK-47, 12 cm of metal"), ShootThroughWalls(AK, Metal), true);
	const FTestWall ThickMetal[] = {{ES::Metal, 20.0f}};
	Check(TEXT("AK-47, 20 cm of metal"), ShootThroughWalls(AK, ThickMetal), false);
	const FTestWall Glass[] = {{ES::Glass, 5.0f}};
	Check(TEXT("AK-47, a pane of glass"), ShootThroughWalls(AK, Glass), true);
	// Tile (CS: 65 % of the power, a fifth of the damage): the AK-47's 64 cm.
	const FTestWall Tile[] = {{ES::Tile, 50.0f}};
	Check(TEXT("AK-47, 50 cm of tile"), ShootThroughWalls(AK, Tile), true);
	const FTestWall ThickTile[] = {{ES::Tile, 70.0f}};
	Check(TEXT("AK-47, 70 cm of tile"), ShootThroughWalls(AK, ThickTile), false);
	// A material without a physical material is CS's default (the full power, half the damage).
	const FTestWall Plain[] = {{ES::Default, 80.0f}};
	Check(TEXT("AK-47, 80 cm of the default"), ShootThroughWalls(AK, Plain), true);
	const FTestWall Plank[] = {{ES::Wood, 5.0f}};
	Check(TEXT("USP, a plank"), ShootThroughWalls(AShooterWeapon_USP::StaticClass(), Plank), false);
	const FTestWall TwoWalls[] = {{ES::Wood, 20.0f, 300.0f}, {ES::Concrete, 20.0f, 500.0f}};
	Check(TEXT("AWP, wood then concrete"), ShootThroughWalls(AWP, TwoWalls), true);
	Check(TEXT("AK-47, wood then concrete (one wall only)"), ShootThroughWalls(AK, TwoWalls), false);
	// The Desert Eagle pierces up to 1000 units (25.4 m) away.
	const FTestWall NearPlank[] = {{ES::Wood, 5.0f, 2000.0f}};
	Check(TEXT("Deagle, a plank at 20 m"), ShootThroughWalls(AShooterWeapon_Deagle::StaticClass(), NearPlank, 2400.0f),
		true);
	const FTestWall FarPlank[] = {{ES::Wood, 5.0f, 2600.0f}};
	Check(TEXT("Deagle, a plank at 26 m"), ShootThroughWalls(AShooterWeapon_Deagle::StaticClass(), FarPlank, 3000.0f),
		false);

	FHitResult Hit;
	TestTrue("No physical material: the default surface", AShooterWeapon_Instant::GetSurfaceType(Hit) == ES::Default);

	// Through a body: the one behind takes three quarters.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterCharacter* Front = SpawnShooter(World, FVector(500.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
	AShooterCharacter* Behind = SpawnShooter(World, FVector(800.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
	AShooterWeapon_Instant* Rifle = Cast<AShooterWeapon_Instant>(GiveAndDraw(World, *Shooter, AK));
	MakeAccurate(*Rifle);
	AimAt(*Shooter, (GetChest(*Front) + GetChest(*Behind)) * 0.5f);
	TickFrames(World, 1);
	Shoot(World, *Shooter);
	const float FrontTaken = 100.0f - Front->GetHealth();
	const float BehindTaken = 100.0f - Behind->GetHealth();
	TestTrue("Both hit", FrontTaken > 0.0f && BehindTaken > 0.0f && Rifle->GetLastShotPenetrations() == 1);
	TestTrue(
		"The one behind: about three quarters", BehindTaken < FrontTaken * 0.76f && BehindTaken > FrontTaken * 0.7f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameDamageHitGroupsTest, "ShooterGame.Damage.HitGroups",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameDamageHitGroupsTest::RunTest(const FString& Parameters)
{
	// CS's hit groups on the capsule: the head (x4), the chest and the arms (x1), the stomach (x1.25), the legs
	// (x0.75); armor covers every group but the legs, the head only with a helmet. Crouched, the bands shrink with the
	// capsule. The hits are an AK-47's (armor ratio 1.55: an armored group takes 77.5 %).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterCharacter* Shooter = SpawnShooter(World, FVector(-1000.0f, 0.0f, 0.0f), 0.0f, EShooterTeam::CT);
	AShooterCharacter* Victim = SpawnShooter(World, FVector::ZeroVector, 180.0f, EShooterTeam::T);
	AShooterWeapon* Rifle = Shooter->GiveWeapon(AShooterWeapon_AK47::StaticClass());
	TickFrames(World, 2);
	using G = EShooterHitGroup;
	// The victim faces -X: its right is +Y... seen from the victim, its right is its actor right vector.
	const FVector Feet = Victim->GetActorLocation();
	const FVector Right = Victim->GetActorRightVector();
	struct FGroupHit
	{
		const TCHAR* Name;
		FVector Offset;
		G Group;
		float Multiplier;
		bool bArmored;
	};
	const FGroupHit Hits[] = {
		{TEXT("head"), FVector(0.0f, 0.0f, 170.0f), G::Head, 4.0f, false},
		{TEXT("chest"), FVector(0.0f, 0.0f, 130.0f), G::Chest, 1.0f, true},
		{TEXT("stomach"), FVector(0.0f, 0.0f, 100.0f), G::Stomach, 1.25f, true},
		{TEXT("right leg"), (Right * 15.0f) + FVector(0.0f, 0.0f, 50.0f), G::RightLeg, 0.75f, false},
		{TEXT("left leg"), (Right * -15.0f) + FVector(0.0f, 0.0f, 50.0f), G::LeftLeg, 0.75f, false},
		{TEXT("right arm"), (Right * 35.0f) + FVector(0.0f, 0.0f, 130.0f), G::RightArm, 1.0f, true},
		{TEXT("left arm"), (Right * -35.0f) + FVector(0.0f, 0.0f, 130.0f), G::LeftArm, 1.0f, true},
	};
	constexpr float Damage = 20.0f;
	for (const FGroupHit& Hit : Hits)
	{
		const FVector Point = Feet + Hit.Offset;
		TestTrue(*FString::Printf(TEXT("The %s"), Hit.Name), Victim->GetHitGroup(Point) == Hit.Group);
		for (const bool bKevlar : {false, true})
		{
			Victim->ResetHealth();
			Victim->SetArmor(bKevlar ? 100.0f : 0.0f, false);
			FHitResult Impact;
			Impact.bBlockingHit = true;
			Impact.ImpactPoint = Point;
			const FPointDamageEvent Event(Damage, Impact, FVector(1.0f, 0.0f, 0.0f), nullptr);
			(void)Victim->TakeDamage(Damage, Event, Shooter->GetController(), Rifle);
			const float Expected = Damage * Hit.Multiplier * (bKevlar && Hit.bArmored ? 0.775f : 1.0f);
			TestEqual(*FString::Printf(TEXT("The %s, %s"), Hit.Name, bKevlar ? TEXT("kevlar") : TEXT("no armor")),
				100.0f - Victim->GetHealth(), Expected, 1.0e-3f);
			TestTrue(*FString::Printf(TEXT("The %s's armor"), Hit.Name),
				!bKevlar || (Victim->GetArmor() < 100.0f) == Hit.bArmored);
		}
	}
	// The head with a helmet.
	Victim->ResetHealth();
	Victim->SetArmor(100.0f, true);
	FHitResult HeadHit;
	HeadHit.bBlockingHit = true;
	HeadHit.ImpactPoint = Feet + FVector(0.0f, 0.0f, 170.0f);
	(void)Victim->TakeDamage(Damage, FPointDamageEvent(Damage, HeadHit, FVector(1.0f, 0.0f, 0.0f), nullptr),
		Shooter->GetController(), Rifle);
	TestEqual("The head with a helmet", 100.0f - Victim->GetHealth(), Damage * 4.0f * 0.775f, 1.0e-3f);

	// Crouched: the capsule is 92 cm; the head its top 28 cm, the legs below 38 cm.
	Victim->ResetHealth();
	Victim->Crouch();
	TickFrames(World, 2);
	const float Height = 2.0f * Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TestTrue("Crouched", Victim->bIsCrouched && Height < 100.0f);
	TestTrue("Crouched: the head", Victim->GetHitGroup(Feet + FVector(0.0f, 0.0f, Height - 10.0f)) == G::Head);
	TestTrue("Crouched: the chest", Victim->GetHitGroup(Feet + FVector(0.0f, 0.0f, Height - 35.0f)) == G::Chest);
	TestTrue("Crouched: a leg",
		Victim->GetHitGroup(Feet + FVector(0.0f, 0.0f, 20.0f)) == G::RightLeg ||
			Victim->GetHitGroup(Feet + FVector(0.0f, 0.0f, 20.0f)) == G::LeftLeg);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
