#pragma once

#include "AI/Navigation/NavigationSystem.h"
#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Physics/PhysScene.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "World.generated.h"

class ACharacter;
class AGameModeBase;
class AGameStateBase;
class FDebugDraw;
class FSceneRenderer;
class UGameInstance;
class UPrimitiveComponent;

/** What SpawnActor does when the new actor would overlap something (UE: ESpawnActorCollisionHandlingMethod). */
enum class ESpawnActorCollisionHandlingMethod : uint8
{
	/** The class default decides (Leon: always spawns). */
	Undefined,
	AlwaysSpawn,
	AdjustIfPossibleButAlwaysSpawn,
	AdjustIfPossibleButDontSpawnIfColliding,
	DontSpawnIfColliding,
};

/** Optional parameters of UWorld::SpawnActor (UE: FActorSpawnParameters). */
struct ENGINE_API FActorSpawnParameters
{
	FActorSpawnParameters();

	/** The actor's name; NAME_None makes a unique one. An existing actor of that name in the level is a fatal error. */
	FName Name;
	/** An actor whose properties are copied into the new one instead of the class defaults. */
	AActor* Template = nullptr;
	/** The new actor's owner (AActor::GetOwner). */
	AActor* Owner = nullptr;
	/** The pawn responsible for the new actor (AActor::GetInstigator). */
	APawn* Instigator = nullptr;
	/** The level to spawn in; the world's persistent level when null. */
	ULevel* OverrideLevel = nullptr;
	/** Kept for the UE signature: Leon does not test spawn collisions (every spawn succeeds). */
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::Undefined;
	/** The caller calls AActor::FinishSpawning itself (UE: bDeferConstruction). */
	uint8 bDeferConstruction : 1;
	/** Kept for the UE signature (every spawn succeeds). */
	uint8 bNoFail : 1;
	/** Flags of the new actor (UE: ObjectFlags; RF_Transactional there, none here). */
	EObjectFlags ObjectFlags = RF_NoFlags;
};

struct ENGINE_API FWorldGameplayFrameParams
{
	float DeltaTime = 0.0f;
	ULevel* Level = nullptr;
	FSceneRenderer* Renderer = nullptr;
	FDebugDraw* CollisionDebugDraw = nullptr;
	FDebugDraw* NavMeshDebugDraw = nullptr;
	/** When true, FPhysScene::Step uses these values instead of the first Character's movement. */
	bool bOverridePhysicsStep = false;
	float PhysicsDamping = 6.0f;
	/** cm */
	float PhysicsWalkBounds = 1800.0f;
	/** cm/s^2 */
	float PhysicsGravity = 2400.0f;
	/** cm */
	float PhysicsFloorZ = 0.0f;
	/** cm */
	float PhysicsSkin = 2.0f;
};

/**
 * The world (UE: UWorld): a UObject whose outer is its package (a transient "/Temp/Untitled_<N>" until P13's
 * UEngine::LoadMap loads map packages), which owns its persistent level (and through it the actors), the physics scene
 * and the navigation system.
 *
 * - CreateWorld makes the package, the world and its level; DestroyWorld ends play on every actor, marks the world, its
 *   level and its actors pending kill and removes the world from the root set. The owner (a UGameInstance's world
 *   context, a test's FScopedTestWorld) then collects garbage at that safe point.
 * - SpawnActor creates actors with NewObject in the level (their outer). An actor spawned while the world ticks joins
 *   the level (and begins play) once the tick ends.
 * - DestroyActor ends play, unregisters the components, removes the actor from the level and marks it pending kill.
 * - The game mode is spawned by the world (SetGameMode) and kept in AuthorityGameMode.
 */
UCLASS()
class ENGINE_API UWorld : public UObject
{
	GENERATED_BODY()

public:
	UWorld(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The level the world was created with; every actor spawns here (UE: PersistentLevel). */
	UPROPERTY(Transient)
	ULevel* PersistentLevel = nullptr;

	/** The game mode, spawned by SetGameMode (UE: AuthorityGameMode). */
	UPROPERTY(Transient)
	AGameModeBase* AuthorityGameMode = nullptr;

	/** The game state the game mode spawned (UE: GameState). */
	UPROPERTY(Transient)
	AGameStateBase* GameState = nullptr;

	/** The game instance whose world context holds this world (UE: OwningGameInstance). */
	UPROPERTY(Transient)
	UGameInstance* OwningGameInstance = nullptr;

	/**
	 * Creates a world in a new transient package with its persistent level (UE: CreateWorld). bInformEngineOfWorld is
	 * kept for the UE signature (the world contexts belong to UGameInstance until P13). With bAddToRoot the world is in
	 * the root set until DestroyWorld.
	 */
	static UWorld* CreateWorld(EWorldType::Type InWorldType, bool bInformEngineOfWorld, FName WorldName = NAME_None,
		UPackage* InWorldPackage = nullptr, bool bAddToRoot = true);

	/**
	 * Ends play on every actor (EEndPlayReason::Quit), clears the physics and navigation, and marks the actors, the
	 * level and the world pending kill (UE: DestroyWorld). The caller collects garbage afterwards.
	 */
	void DestroyWorld(bool bInformEngineOfWorld);

	/** UWorld::GetWorld is the world itself (UE). */
	[[nodiscard]] UWorld* GetWorld() const
	{
		return const_cast<UWorld*>(this);
	}

	[[nodiscard]] EWorldType::Type GetWorldType() const
	{
		return WorldType;
	}
	[[nodiscard]] ULevel* GetCurrentLevel() const
	{
		return PersistentLevel;
	}
	[[nodiscard]] UGameInstance* GetGameInstance() const
	{
		return OwningGameInstance;
	}
	[[nodiscard]] AGameModeBase* GetAuthGameMode() const
	{
		return AuthorityGameMode;
	}
	template <class T>
	[[nodiscard]] T* GetAuthGameMode() const
	{
		return Cast<T>(AuthorityGameMode);
	}
	/** UE: GetGameState / SetGameState. */
	[[nodiscard]] AGameStateBase* GetGameState() const
	{
		return GameState;
	}
	template <class T>
	[[nodiscard]] T* GetGameState() const
	{
		return Cast<T>(GameState);
	}
	void SetGameState(AGameStateBase* NewGameState)
	{
		GameState = NewGameState;
	}

	/**
	 * Spawns the game mode (UE: SetGameMode(FURL), which asks the game instance for the class; Leon takes the class
	 * until P13 brings FURL and the game mode precedence of plan decision D18), then StartPlay when the world plays.
	 * Returns the game mode.
	 */
	AGameModeBase* SetGameMode(TSubclassOf<AGameModeBase> GameModeClass);

	/** True once the world plays: actors spawned from then on begin play at once (UE: HasBegunPlay). */
	[[nodiscard]] bool HasBegunPlay() const
	{
		return bBegunPlay;
	}
	/** Begins play on every actor that has not (UE: BeginPlay). Leon worlds begin play when they are created. */
	void BeginPlay();

	[[nodiscard]] FPhysScene& GetPhysicsScene()
	{
		return Physics;
	}
	[[nodiscard]] const FPhysScene& GetPhysicsScene() const
	{
		return Physics;
	}

	/** Unreal-like UNavigationSystem lite (grid NavMesh for AI pathfinding). */
	[[nodiscard]] UNavigationSystem& GetNavigationSystem()
	{
		return Navigation;
	}
	[[nodiscard]] const UNavigationSystem& GetNavigationSystem() const
	{
		return Navigation;
	}

	/** Recreate FPhysScene with another backend (clears bodies). Call before RegisterBodiesFromLevel. */
	void SetPhysicsBackend(EPhysicsBackend PhysicsBackend)
	{
		Physics = FPhysScene(PhysicsBackend);
	}

	/**
	 * Spawns an actor of Class (UE: SpawnActor): NewObject in the level, root placed at Location / Rotation (zero when
	 * null), components registered, PreInitializeComponents / InitializeComponents / PostInitializeComponents, then
	 * BeginPlay (after the current tick when the world is ticking). Returns null for a null or abstract class.
	 */
	AActor* SpawnActor(UClass* Class, const FVector* Location = nullptr, const FRotator* Rotation = nullptr,
		const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());
	/**
	 * SpawnActor at a transform: the root takes it whole (rotation as the quaternion given, scale) before the
	 * components register.
	 */
	AActor* SpawnActor(UClass* Class, const FTransform* Transform,
		const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());

	template <class T>
	T* SpawnActor(const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters())
	{
		return CastChecked<T>(
			SpawnActor(T::StaticClass(), nullptr, nullptr, SpawnParameters), ECastCheckedType::NullAllowed);
	}
	template <class T>
	T* SpawnActor(const FVector& Location, const FRotator& Rotation,
		const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters())
	{
		return CastChecked<T>(
			SpawnActor(T::StaticClass(), &Location, &Rotation, SpawnParameters), ECastCheckedType::NullAllowed);
	}
	template <class T>
	T* SpawnActor(UClass* Class, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters())
	{
		return CastChecked<T>(SpawnActor(Class, nullptr, nullptr, SpawnParameters), ECastCheckedType::NullAllowed);
	}
	template <class T>
	T* SpawnActor(UClass* Class, const FVector& Location, const FRotator& Rotation,
		const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters())
	{
		return CastChecked<T>(SpawnActor(Class, &Location, &Rotation, SpawnParameters), ECastCheckedType::NullAllowed);
	}
	template <class T>
	T* SpawnActor(UClass* Class, const FTransform& Transform,
		const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters())
	{
		return CastChecked<T>(SpawnActor(Class, &Transform, SpawnParameters), ECastCheckedType::NullAllowed);
	}
	/** Spawns with bDeferConstruction: the caller finishes with AActor::FinishSpawning (UE: SpawnActorDeferred). */
	template <class T>
	T* SpawnActorDeferred(
		UClass* Class, const FTransform& Transform, AActor* Owner = nullptr, APawn* Instigator = nullptr)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = Owner;
		SpawnParameters.Instigator = Instigator;
		SpawnParameters.bDeferConstruction = true;
		return CastChecked<T>(SpawnActor(Class, &Transform, SpawnParameters), ECastCheckedType::NullAllowed);
	}

	/**
	 * Destroys an actor of this world (UE: DestroyActor): Destroyed, EndPlay, components unregistered, removed from the
	 * level (its slot is nulled while the world ticks), marked pending kill. Returns false for an actor of another
	 * world.
	 */
	bool DestroyActor(AActor* Actor, bool bNetForce = false, bool bShouldModifyLevel = true);

	/** The rest of a spawn once the actor exists (UE: AActor::PostActorConstruction); FinishSpawning calls it. */
	void PostActorConstruction(AActor* Actor);

	/** Actor Tick only (UAnimInstance, etc.). Prefer TickGameplayFrame for Character worlds. */
	void Tick(float InDeltaTime);

	/**
	 * Unreal-like frame: Character move → FPhysScene::Step → overlaps → Actor Tick → the bodies move their components
	 * (FPhysScene::SyncToLevel, with Params.Level) → draw.
	 */
	void TickGameplayFrame(const FWorldGameplayFrameParams& Params);

	/**
	 * Registers a physics scene body for every primitive component of the level whose collision is enabled
	 * (ULevel::GetCollisionPrimitives; clears first). A body's LevelMeshIndex is the component's index in that list.
	 */
	void RegisterBodiesFromLevel(const ULevel& InLevel);

	/**
	 * The render state of the registered primitive components (UPrimitiveComponent::CreateRenderState_Concurrent):
	 * P13 replaces this list with FScene::AddPrimitive / RemovePrimitive.
	 */
	void AddPrimitive(UPrimitiveComponent* Primitive);
	void RemovePrimitive(UPrimitiveComponent* Primitive);
	[[nodiscard]] const TArray<UPrimitiveComponent*>& GetPrimitives() const
	{
		return Primitives;
	}

	/** Every registered primitive that should render submits its draw, in registration order. */
	void SubmitPrimitiveDraws(FSceneRenderer& InRenderer) const;

	/** Destroys every actor (EEndPlayReason::Destroyed) and clears the physics scene (Leon; UE has no counterpart). */
	void Clear();

	/** Actors in the level that are not pending kill (spawns waiting for the end of a tick are not counted). */
	[[nodiscard]] SIZE_T ActorCount() const;

	template <typename T>
	[[nodiscard]] T* FindFirst() const
	{
		static_assert(TIsDerivedFrom<T, AActor>::Value, "T must derive from Actor");
		if (PersistentLevel == nullptr)
		{
			return nullptr;
		}
		for (AActor* Actor : PersistentLevel->Actors)
		{
			if (Actor != nullptr && !Actor->IsPendingKillPending())
			{
				if (T* Typed = Cast<T>(Actor))
				{
					return Typed;
				}
			}
		}
		return nullptr;
	}

	/** Visit every live Actor of type T. */
	template <typename T, typename TFn>
	void ForEach(TFn&& Fn) const
	{
		static_assert(TIsDerivedFrom<T, AActor>::Value, "T must derive from Actor");
		if (PersistentLevel == nullptr)
		{
			return;
		}
		// Index loop: Fn may destroy actors (their slots become null) but the array does not shrink meanwhile.
		for (int32 Index = 0; Index < PersistentLevel->Actors.Num(); ++Index)
		{
			AActor* Actor = PersistentLevel->Actors[Index];
			if (Actor != nullptr && !Actor->IsPendingKillPending())
			{
				if (T* Typed = Cast<T>(Actor))
				{
					Fn(*Typed);
				}
			}
		}
	}

	/** Visit every live Actor (any type). */
	template <typename TFn>
	void ForEachActor(TFn&& Fn) const
	{
		ForEach<AActor>(Forward<TFn>(Fn));
	}

private:
	friend class AActor;

	void InitWorld();
	AActor* SpawnActorInternal(UClass* Class, const FVector* Location, const FRotator* Rotation,
		const FTransform* Transform, const FActorSpawnParameters& SpawnParameters);
	void FlushPendingSpawns();
	/** Removes the null slots destroyed actors left in the level while the world ticked. */
	void CompactActors();
	/** Pairwise Character capsule depenetration (players / AI are not FPhysScene bodies). */
	void ResolveCharacterOverlaps();

	/** Actors spawned during a tick: they join the level when it ends (UE adds them at once). */
	UPROPERTY(Transient)
	TArray<AActor*> PendingSpawnActors;

	/** Registered primitive components (the render scene until P13's FScene). */
	UPROPERTY(Transient)
	TArray<UPrimitiveComponent*> Primitives;

	FPhysScene Physics{};
	UNavigationSystem Navigation{};
	EWorldType::Type WorldType = EWorldType::None;
	uint64 NextUniqueID = 0;
	bool bBegunPlay = false;
	bool bTicking = false;
	bool bIsTearingDown = false;
	/** A DestroyActor during the tick left null slots in the level. */
	bool bHasNullActorSlots = false;
};
