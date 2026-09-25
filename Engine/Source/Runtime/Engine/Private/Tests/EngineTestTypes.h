// Reflected fixtures of the Engine tests.
#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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
