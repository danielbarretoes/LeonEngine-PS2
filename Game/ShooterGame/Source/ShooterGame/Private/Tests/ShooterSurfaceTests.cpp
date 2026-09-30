#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/CharacterAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TriggerVolume.h"
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
#include "Sound/SoundWave.h"
#include "Tests/ScopedTestWorld.h"
#include "Tests/SkinnedTestMesh.h"
#include "Weapons/ShooterWeapon_Instant.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-shipping N30f: the surfaces' sounds, by the physical material of what is under the feet or what the bullet hit
// (DefaultGame.ini's FootstepSounds, LadderStepSoundNames, ImpactSounds, ArmorHitSoundName, HelmetHitSoundName), at
// the fixed 30 Hz step.

namespace
{

	constexpr float Step = 1.0f / 30.0f;

	void TickSteps(UWorld& World, int32 Steps)
	{
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			World.Tick(Step);
		}
	}

	/** A material whose physical material is of Surface (none for the default surface). */
	UMaterial* MakeSurfaceMaterial(UObject* Outer, EPhysicalSurface Surface)
	{
		UMaterial* Material = NewObject<UMaterial>(Outer);
		if (Surface != SHOOTER_SURFACE_Default)
		{
			UPhysicalMaterial* PhysMaterial = NewObject<UPhysicalMaterial>(Material);
			PhysMaterial->SurfaceType = Surface;
			Material->PhysMaterial = PhysMaterial;
		}
		return Material;
	}

	/** A box of the engine's cube, Center and Size in cm, a map's static mesh of Surface. */
	AStaticMeshActor* SpawnSurfaceBox(
		UWorld& World, const FVector& Center, const FVector& Size, EPhysicalSurface Surface)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		AStaticMeshActor* Box = World.SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), FTransform(FQuat::Identity, Center, Size / 100.0f));
		if (Box == nullptr || Cube == nullptr)
		{
			return nullptr;
		}
		UStaticMeshComponent* Mesh = Box->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Static);
		(void)Mesh->SetStaticMesh(Cube);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetMaterial(0, MakeSurfaceMaterial(Box, Surface));
		return Box;
	}

	/** A character on the floor at Feet facing Yaw, possessed by a brainless bot of Team. */
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

	/** Steps with the stick held along Direction. */
	void MoveSteps(UWorld& World, AShooterCharacter& Character, const FVector& Direction, int32 Steps)
	{
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			Character.APawn::AddMovementInput(Direction, 1.0f);
			World.Tick(Step);
		}
	}

	FString SoundName(const USoundWave* Sound)
	{
		return Sound != nullptr ? Sound->GetName() : FString();
	}

	/**
	 * The sounds a runner's steps played on a floor of Surface: a skinned body whose locomotion has Footstep_L and
	 * Footstep_R, running along X for 2 s.
	 */
	TArray<FString> RunOn(EPhysicalSurface Surface, EPhysicalSurface& OutFloor)
	{
		TArray<FString> Played;
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		(void)SpawnSurfaceBox(World, FVector(0.0f, 0.0f, -50.0f), FVector(8000.0f, 8000.0f, 100.0f), Surface);
		(void)World.SetGameMode(AShooterGameMode::StaticClass());
		AShooterCharacter* Runner = SpawnShooter(World, FVector(-2000.0f, 0.0f, 0.0f), 0.0f, EShooterTeam::CT);
		Runner->SetSkeletalBody(FSkinnedTestCharacter::MakeMesh());
		UAnimSequence* Steps =
			FSkinnedTestCharacter::MakeOffsetClip(FSkinnedTestCharacter::Root, FVector::ZeroVector, 1.0f);
		Steps->bLoop = true;
		Steps->AddNotify(TEXT("Footstep_L"), 0.25f);
		Steps->AddNotify(TEXT("Footstep_R"), 0.75f);
		UBlendSpace1D* Locomotion = NewObject<UBlendSpace1D>();
		(void)Locomotion->AddSample(Steps, 0.0f);
		Runner->GetMesh().GetAnimInstance().SetBlendSpace(Locomotion);
		OutFloor = Runner->GetFloorSurface();
		MoveSteps(World, *Runner, FVector(1.0f, 0.0f, 0.0f), 15);
		for (int32 Index = 0; Index < 60; ++Index)
		{
			const USoundWave* Before = Runner->GetLastFootstepSound();
			MoveSteps(World, *Runner, FVector(1.0f, 0.0f, 0.0f), 1);
			if (Runner->GetLastFootstepSound() != Before || (Before != nullptr && Index % 15 == 0))
			{
				Played.AddUnique(SoundName(Runner->GetLastFootstepSound()));
			}
		}
		return Played;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterSurfacesFootstepsTest, "ShooterGame.Surfaces.Footsteps",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterSurfacesFootstepsTest::RunTest(const FString& Parameters)
{
	// A running step plays the floor's sound (its physical material's surface, as CS the texture under the player):
	// the left foot's and the right foot's; a floor without a physical material plays the default's (concrete, CS's).
	struct FCase
	{
		EPhysicalSurface Surface;
		const TCHAR* Sound;
	};
	const FCase Cases[] = {
		{SHOOTER_SURFACE_Concrete, TEXT("Concrete")},
		{SHOOTER_SURFACE_Dirt, TEXT("Dirt")},
		{SHOOTER_SURFACE_Tile, TEXT("Tile")},
		{SHOOTER_SURFACE_Metal, TEXT("Metal")},
		{SHOOTER_SURFACE_Wood, TEXT("Wood")},
		{SHOOTER_SURFACE_Default, TEXT("Concrete")},
		{SHOOTER_SURFACE_Glass, TEXT("Concrete")},
	};
	for (const FCase& Case : Cases)
	{
		EPhysicalSurface Floor = SurfaceType_Max;
		TArray<FString> Played = RunOn(Case.Surface, Floor);
		Played.Sort();
		const FString What = FString::Printf(TEXT("Surface %d"), static_cast<int32>(Case.Surface));
		TestEqual(*(What + TEXT(": the floor under the feet")), Floor, Case.Surface);
		const TArray<FString> Expected = {
			FString::Printf(TEXT("S_Step_%s_L"), Case.Sound), FString::Printf(TEXT("S_Step_%s_R"), Case.Sound)};
		TestEqual(*(What + TEXT(": both feet's steps")), Played, Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterSurfacesLadderStepsTest, "ShooterGame.Surfaces.LadderSteps",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterSurfacesLadderStepsTest::RunTest(const FString& Parameters)
{
	// Climbing a ladder plays CS's pl_ladder: a step every LadderStepInterval (0.35 s), the two sounds in turn; hanging
	// still is silent.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
	(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(300.0f, 0.0f, 400.0f), FVector(4.0f, 6.0f, 8.0f)));
	(void)World.SetGameMode(AShooterGameMode::StaticClass());
	ATriggerVolume* Ladder = World.SpawnActor<ATriggerVolume>(ATriggerVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(90.0f, 0.0f, 400.0f), FVector(0.2f, 1.0f, 8.0f)));
	Ladder->Tags.Add(AShooterGameMode::LadderTag);
	TickSteps(World, 1);
	AShooterCharacter* Climber = SpawnShooter(World, FVector(-100.0f, 0.0f, 0.0f), 0.0f, EShooterTeam::CT);
	UShooterCharacterMovement& Move = *Climber->GetShooterCharacterMovement();
	TestEqual("CS's 0.35 s", Climber->LadderStepInterval, 0.35f);
	for (int32 Index = 0; Index < 60 && !Move.IsOnLadder(); ++Index)
	{
		MoveSteps(World, *Climber, FVector(1.0f, 0.0f, 0.0f), 1);
	}
	if (!TestTrue("On the ladder", Move.IsOnLadder()))
	{
		return false;
	}
	// Climb for 1.5 s: about 0.35 s a step.
	TArray<FString> Played;
	for (int32 Index = 0; Index < 45; ++Index)
	{
		const USoundWave* Before = Climber->GetLastFootstepSound();
		MoveSteps(World, *Climber, FVector(1.0f, 0.0f, 0.0f), 1);
		if (Climber->GetLastFootstepSound() != Before)
		{
			Played.Add(SoundName(Climber->GetLastFootstepSound()));
		}
	}
	TestTrue("Still climbing", Move.IsOnLadder());
	TestTrue("A step every 0.35 s", Played.Num() >= 4 && Played.Num() <= 5);
	bool bInTurn = Played.Num() >= 2;
	for (int32 Index = 0; Index < Played.Num(); ++Index)
	{
		bInTurn &= (Played[Index] == TEXT("S_Step_Ladder_1") || Played[Index] == TEXT("S_Step_Ladder_2")) &&
			(Index == 0 || Played[Index] != Played[Index - 1]);
	}
	TestTrue("The two in turn", bInTurn);
	const USoundWave* Last = Climber->GetLastFootstepSound();
	const FVector Where = Climber->GetActorLocation();
	TickSteps(World, 30);
	TestTrue("Hanging still: silent",
		Climber->GetLastFootstepSound() == Last && Climber->GetActorLocation().Equals(Where, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterSurfacesImpactsTest, "ShooterGame.Surfaces.Impacts",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterSurfacesImpactsTest::RunTest(const FString& Parameters)
{
	// A bullet plays the sound of the surface it enters (CS's debris and ricochets), and on a character CS's bhit_:
	// the flesh unarmored, the kevlar where armor covers, the helmet on a helmeted head; the mark it leaves takes the
	// surface's tint and size.
	struct FCase
	{
		EPhysicalSurface Surface;
		const TCHAR* Sound;
	};
	const FCase Cases[] = {
		{SHOOTER_SURFACE_Concrete, TEXT("S_Impact_Concrete")},
		{SHOOTER_SURFACE_Dirt, TEXT("S_Impact_Dirt")},
		{SHOOTER_SURFACE_Tile, TEXT("S_Impact_Tile")},
		{SHOOTER_SURFACE_Metal, TEXT("S_Impact_Metal")},
		{SHOOTER_SURFACE_Wood, TEXT("S_Impact_Wood")},
		{SHOOTER_SURFACE_Glass, TEXT("S_Impact_Glass")},
		{SHOOTER_SURFACE_Computer, TEXT("S_Impact_Metal")},
		{SHOOTER_SURFACE_Default, TEXT("S_Impact_Concrete")},
	};
	for (const FCase& Case : Cases)
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
		// A wall 2 m thick: nothing goes through.
		(void)SpawnSurfaceBox(World, FVector(500.0f, 0.0f, 150.0f), FVector(200.0f, 400.0f, 300.0f), Case.Surface);
		AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
		AShooterWeapon* Weapon = Shooter->GiveWeapon(AShooterWeapon_AK47::StaticClass());
		Shooter->EquipWeapon(Weapon);
		TickSteps(World, 45);
		AShooterWeapon_Instant* Rifle = Cast<AShooterWeapon_Instant>(Weapon);
		if (!TestNotNull("The rifle", Rifle))
		{
			return false;
		}
		Shooter->StartWeaponFire();
		TickSteps(World, 1);
		Shooter->StopWeaponFire();
		const FString What = FString::Printf(TEXT("Surface %d"), static_cast<int32>(Case.Surface));
		TestEqual(*(What + TEXT(": the hit's surface")), AShooterWeapon_Instant::GetSurfaceType(Rifle->GetLastHit()),
			Case.Surface);
		TestEqual(*(What + TEXT(": its sound")), SoundName(Rifle->GetLastImpactSound()), FString(Case.Sound));
	}

	// The marks: dirt's larger and brown, metal's smaller.
	FLinearColor Concrete;
	FLinearColor Dirt;
	float ConcreteScale = 0.0f;
	float DirtScale = 0.0f;
	float MetalScale = 0.0f;
	FLinearColor Metal;
	AShooterWeapon_Instant::GetImpactMarkStyle(SHOOTER_SURFACE_Concrete, Concrete, ConcreteScale);
	AShooterWeapon_Instant::GetImpactMarkStyle(SHOOTER_SURFACE_Dirt, Dirt, DirtScale);
	AShooterWeapon_Instant::GetImpactMarkStyle(SHOOTER_SURFACE_Metal, Metal, MetalScale);
	TestTrue("Dirt: larger, redder", DirtScale > ConcreteScale && Dirt.R > Concrete.R);
	TestTrue("Metal: smaller", MetalScale < ConcreteScale);

	// A character: flesh, kevlar, helmet.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
	AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterCharacter* Victim = SpawnShooter(World, FVector(600.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
	AShooterWeapon_Instant* Pistol =
		Cast<AShooterWeapon_Instant>(Shooter->GiveWeapon(AShooterWeapon_Deagle::StaticClass()));
	if (!TestNotNull("The pistol", Pistol))
	{
		return false;
	}
	FHitResult Chest;
	Chest.Actor = Victim;
	Chest.ImpactPoint = Victim->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f);
	FHitResult Head = Chest;
	Head.ImpactPoint = Victim->GetActorLocation() + FVector(0.0f, 0.0f, 175.0f);
	FHitResult Leg = Chest;
	Leg.ImpactPoint = Victim->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
	TestEqual("Unarmored: flesh", SoundName(Pistol->GetImpactSound(Chest, 0)), FString(TEXT("S_Hit_Flesh")));
	Victim->SetArmor(100.0f, false);
	TestEqual("Kevlar on the chest", SoundName(Pistol->GetImpactSound(Chest, 0)), FString(TEXT("S_Hit_Kevlar")));
	TestEqual("No helmet: the head's flesh", SoundName(Pistol->GetImpactSound(Head, 0)), FString(TEXT("S_Hit_Flesh")));
	TestEqual("The legs have no armor", SoundName(Pistol->GetImpactSound(Leg, 0)), FString(TEXT("S_Hit_Flesh")));
	Victim->SetArmor(100.0f, true);
	TestEqual("A helmet", SoundName(Pistol->GetImpactSound(Head, 0)), FString(TEXT("S_Hit_Helmet")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
