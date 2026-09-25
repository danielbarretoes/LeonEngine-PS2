// Reflected fixtures of the AIModule gameplay tests: actors, pawns and components need a UClass to be spawned.
#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayTestTypes.generated.h"

/** A plain actor. */
UCLASS()
class ATestActor : public AActor
{
	GENERATED_BODY()
};

/** A plain pawn. */
UCLASS()
class ATestPawn : public APawn
{
	GENERATED_BODY()
};

/** A plain controller (not a player controller). */
UCLASS()
class ATestController : public AController
{
	GENERATED_BODY()
};

/** A component that counts its BeginPlay and TickComponent calls. */
UCLASS()
class UCountingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	int32 Ticks = 0;
	int32 Begins = 0;

	void BeginPlay() override
	{
		Super::BeginPlay();
		++Begins;
	}

	void TickComponent(float DeltaTime) override
	{
		Super::TickComponent(DeltaTime);
		++Ticks;
	}
};
