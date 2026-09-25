#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "GameInstance.generated.h"

class UGameInstance;
class UPackage;
class UWorld;

/**
 * The world a game instance plays in, with what the engine knows about it (UE: FWorldContext). UE keeps the contexts
 * in UEngine::WorldList and each game instance points at its own; until P13 brings UEngine, UGameInstance owns its
 * context.
 */
USTRUCT()
struct ENGINE_API FWorldContext
{
	GENERATED_BODY()

	/** What the context's world is for (UE: WorldType). */
	EWorldType::Type WorldType = EWorldType::None;

	/** The game instance this context belongs to (UE: OwningGameInstance). */
	UPROPERTY(Transient)
	UGameInstance* OwningGameInstance = nullptr;

	/** A name that identifies the context (UE: ContextHandle). */
	UPROPERTY(Transient)
	FName ContextHandle;

	/** The world (UE: World()). */
	[[nodiscard]] UWorld* World() const
	{
		return ThisCurrentWorld;
	}

	/** Replaces the world (UE: SetCurrentWorld); the world's OwningGameInstance follows. */
	void SetCurrentWorld(UWorld* World);

private:
	/** UE: ThisCurrentWorld. */
	UPROPERTY(Transient)
	UWorld* ThisCurrentWorld = nullptr;
};

/**
 * The game session (UE: UGameInstance): created by the engine before any world and kept across level changes. It owns
 * the world context, whose world it creates in InitializeStandalone; the engine destroys that world at shutdown (a
 * garbage collection safe point).
 */
UCLASS(Transient)
class ENGINE_API UGameInstance : public UObject
{
	GENERATED_BODY()

public:
	/** Called once the engine is initialized (UE). Overrides call Super. */
	virtual void Init();
	/** Called when the engine shuts down, before its world is destroyed (UE). Overrides call Super. */
	virtual void Shutdown();

	/**
	 * Creates the world context and its game world in a transient package (UE: InitializeStandalone). P13's
	 * UEngine::LoadMap replaces the world with a loaded map.
	 */
	void InitializeStandalone(FName InPackageName = NAME_None, UPackage* InWorldPackage = nullptr);

	/**
	 * Ends play in the context's world, destroys it and clears the context (Leon: UE's engine does this on exit and on
	 * LoadMap). The caller collects garbage afterwards.
	 */
	void DestroyWorldContextWorld();

	[[nodiscard]] FWorldContext* GetWorldContext()
	{
		return &WorldContext;
	}
	[[nodiscard]] const FWorldContext* GetWorldContext() const
	{
		return &WorldContext;
	}
	/** The context's world (UE: GetWorld). */
	[[nodiscard]] UWorld* GetWorld() const
	{
		return WorldContext.World();
	}

	/** Called when a level is successfully activated for gameplay. */
	virtual void NotifyLevelOpened()
	{
		++LevelsOpened;
	}

	[[nodiscard]] int32 GetLevelsOpened() const
	{
		return LevelsOpened;
	}

private:
	/** The world context (UE: WorldContext, a pointer into GEngine->WorldList there). */
	UPROPERTY(Transient)
	FWorldContext WorldContext;

	int32 LevelsOpened = 0;
};
