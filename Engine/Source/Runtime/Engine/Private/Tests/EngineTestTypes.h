// Reflected fixtures of the Engine tests.
#pragma once

#include "Commandlets/Commandlet.h"
#include "Components/SphereComponent.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "EngineTestTypes.generated.h"

class UTexture2D;

/** An actor that, on its first tick, spawns an actor and destroys another, to check what a tick may do. */
UCLASS()
class AEngineTestTickSpawner : public AActor
{
	GENERATED_BODY()

public:
	/** Destroyed on the first tick. */
	UPROPERTY()
	AActor* Victim = nullptr;

	/** Spawned on the first tick. */
	UPROPERTY()
	AActor* Spawned = nullptr;

	/** What the tick saw right after spawning. */
	bool bSpawnedInLevelDuringTick = true;
	bool bSpawnedBegunDuringTick = true;
	bool bVictimSlotNulledDuringTick = false;

	void Tick(float DeltaSeconds) override
	{
		Super::Tick(DeltaSeconds);
		if (Spawned != nullptr)
		{
			return;
		}
		UWorld* World = GetWorld();
		Spawned = World->SpawnActor<AActor>();
		bSpawnedInLevelDuringTick = World->PersistentLevel->Actors.Contains(Spawned);
		bSpawnedBegunDuringTick = Spawned->HasActorBegunPlay();
		const int32 NumBefore = World->PersistentLevel->Actors.Num();
		World->DestroyActor(Victim);
		bVictimSlotNulledDuringTick =
			World->PersistentLevel->Actors.Num() == NumBefore && !World->PersistentLevel->Actors.Contains(Victim);
	}
};

/** Counts the input bindings it receives, for the input tests. */
UCLASS()
class UEngineTestInputReceiver : public UObject
{
	GENERATED_BODY()

public:
	int32 Presses = 0;
	int32 Releases = 0;
	int32 AxisCalls = 0;
	float LastAxisValue = 0.0f;

	void OnPressed()
	{
		++Presses;
	}
	void OnReleased()
	{
		++Releases;
	}
	void OnAxis(float Value)
	{
		++AxisCalls;
		LastAxisValue = Value;
	}
};

/** A game's data asset, for the asset tests: plain properties and a reference to another asset. */
UCLASS()
class UEngineTestDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	FString Label;

	UPROPERTY()
	TArray<FName> Tags;

	UPROPERTY()
	UTexture2D* Icon = nullptr;
};

/** A commandlet for the tests: Main keeps its parameters and returns the number of tokens among them. */
UCLASS()
class UEngineTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	/** The parameters of the last Main. */
	FString LastParams;

	int32 Main(const FString& Params) override
	{
		LastParams = Params;
		TArray<FString> Tokens;
		TArray<FString> Switches;
		ParseCommandLine(*Params, Tokens, Switches);
		return Tokens.Num();
	}
};

/** A character movement whose speed a flag halves (the GetMaxSpeed hook, as a game's walk modifier). */
UCLASS()
class UEngineTestCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	/** Halves the speed. */
	bool bTestWalking = false;

	float GetMaxSpeed() const override
	{
		const float Speed = Super::GetMaxSpeed();
		return bTestWalking ? Speed * 0.5f : Speed;
	}
};

/** A character built with the test movement (UE: SetDefaultSubobjectClass on the movement's name). */
UCLASS()
class AEngineTestCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEngineTestCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer.SetDefaultSubobjectClass<UEngineTestCharacterMovement>(
			  ACharacter::CharacterMovementComponentName))
	{
		GetCharacterMovement().NavAgentProps.bCanCrouch = true;
	}
};

/**
 * A projectile for the movement tests (UE ShooterGame's AShooterProjectile in small): a 5 cm query-only WorldDynamic
 * sphere as the root, moved by a bouncing UProjectileMovementComponent at 1000 cm/s.
 */
UCLASS()
class AEngineTestProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEngineTestProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer.DoNotCreateDefaultSubobject(AActor::DefaultSceneRootName))
	{
		CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
		CollisionComp->InitSphereRadius(5.0f);
		CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CollisionComp->SetCanEverAffectNavigation(false);
		CollisionComp->SetMobility(EComponentMobility::Movable);
		RootComponent = CollisionComp;

		MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
		MovementComp->UpdatedComponent = CollisionComp;
		MovementComp->InitialSpeed = 1000.0f;
		MovementComp->bShouldBounce = true;
	}

	UPROPERTY()
	USphereComponent* CollisionComp = nullptr;

	UPROPERTY()
	UProjectileMovementComponent* MovementComp = nullptr;
};
