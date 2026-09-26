#pragma once

#include "AI/Navigation/NavigationSystem.h"
#include "CoreMinimal.h"
#include "Debug/DebugDraw.h"
#include "Engine/EngineBaseTypes.h"
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
class APlayerController;
class AWorldSettings;
class FDebugDraw;
class UAssetImportData;
class FSceneInterface;
class UGameInstance;
class UPlayer;

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
 * The world (UE: UWorld): a UObject whose outer is its package, which owns its persistent level (and through it the
 * actors), the physics scene and the navigation system. A map is a `.lmap` package holding a world (the asset, named
 * after the package), its persistent level, the level's AWorldSettings and the actors with their components (plan
 * decision D13: PKG_ContainsMap).
 *
 * - CreateWorld makes the world and its level, in a new transient "/Temp/Untitled_<N>" package or in the package it is
 *   given (a map being built: UPackage::SavePackage with a `.lmap` file name saves it). A map loaded with LoadPackage
 *   gives its world (FindWorldInPackage); the loader then calls InitWorld, and InitializeActorsForPlay registers the
 *   actors' components and initializes the actors (UEngine::LoadMap).
 * - The world plays once BeginPlay runs (UEngine::LoadMap calls it after the players logged in; a test's
 *   FScopedTestWorld at once). DestroyWorld ends play on every actor, marks the world, its level and its actors
 *   pending kill and removes the world from the root set. The owner (UEngine::LoadMap, a UGameInstance's world
 *   context, a test's FScopedTestWorld) then collects garbage at that safe point.
 * - SpawnActor creates actors with NewObject in the level (their outer). An actor spawned while the world ticks joins
 *   the level (and begins play) once the tick ends.
 * - DestroyActor ends play, unregisters the components, removes the actor from the level and marks it pending kill.
 * - The game mode is spawned by the world (SetGameMode) and kept in AuthorityGameMode.
 * - Scene is the renderer's copy of the world (IRendererModule::AllocateScene at creation, RemoveScene in
 *   DestroyWorld); null when nothing can render (`-nullrhi`, no Renderer module).
 */
UCLASS()
class ENGINE_API UWorld : public UObject
{
	GENERATED_BODY()

public:
	UWorld(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// UObject
	void BeginDestroy() override;

	/** The level the world was created with; every actor spawns here (UE: PersistentLevel). A map saves it. */
	UPROPERTY()
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

#if WITH_EDITORONLY_DATA
	/**
	 * Where an imported map came from (Leon: LeonEd's UGLTFMapFactory makes it, and reimports the map from it; UE's
	 * imported scenes keep theirs on the Datasmith scene asset). Null for a map built otherwise; dropped by the cook.
	 */
	UPROPERTY(Instanced)
	UAssetImportData* AssetImportData = nullptr;
#endif

	/**
	 * The renderer's scene: the proxies of the registered primitives and lights (UE: Scene). Null when the engine
	 * cannot render (FApp::CanEverRender, e.g. `-nullrhi`) or the target has no Renderer module.
	 */
	FSceneInterface* Scene = nullptr;

	/**
	 * This frame's debug lines (UE: LineBatcher, a ULineBatchComponent): gameplay and physics debug draws add lines,
	 * the renderer draws them after the scene and empties the batch.
	 */
	FDebugDraw LineBatcher;

	/** What InitWorld sets up (UE: UWorld::InitializationValues, the part Leon has). */
	struct InitializationValues
	{
		InitializationValues()
			: bInitializeScenes(true)
		{
		}

		/** Allocates the renderer's scene (UE: bInitializeScenes); tools that only build and save a map do not. */
		uint32 bInitializeScenes : 1;

		InitializationValues& InitializeScenes(const bool bInitialize)
		{
			bInitializeScenes = bInitialize;
			return *this;
		}
	};

	/**
	 * Creates a world with its persistent level and initializes it with InIVS, the defaults when null (UE:
	 * CreateWorld): in InWorldPackage (a map's package; the world is then public and standalone, like UE's map
	 * assets), else in a new transient package. bInformEngineOfWorld is kept for the UE signature (the world contexts
	 * belong to the game instances). With bAddToRoot the world is in the root set until DestroyWorld. The world has not
	 * begun play.
	 */
	static UWorld* CreateWorld(EWorldType::Type InWorldType, bool bInformEngineOfWorld, FName WorldName = NAME_None,
		UPackage* InWorldPackage = nullptr, bool bAddToRoot = true, const InitializationValues* InIVS = nullptr);

	/** The world of a map package (UE: FindWorldInPackage): its UWorld object, or null. */
	static UWorld* FindWorldInPackage(UPackage* Package);

	/**
	 * Gets a created or loaded world ready (UE: InitWorld): the persistent level (made when missing) knows its world,
	 * the level's actors get their spawn-order IDs (AActor::GetUniqueID, in level order: a loaded map keeps the order
	 * it was saved in), and the renderer's scene is allocated when IVS asks and the engine can render. Runs once.
	 */
	void InitWorld(const InitializationValues IVS = InitializationValues());

	/**
	 * Registers the components of every actor of the level that are not registered yet, in level order (UE:
	 * UpdateWorldComponents): a loaded map's components get their physics bodies and scene proxies here. The flags are
	 * kept for the UE signature (Leon has no construction scripts and one level).
	 */
	void UpdateWorldComponents(bool bRerunConstructionScripts = false, bool bCurrentLevelOnly = true);

	/** Sets what the world is for (Leon: UE sets WorldType directly); UEngine::LoadMap sets a loaded map's. */
	void SetWorldType(EWorldType::Type InWorldType)
	{
		WorldType = InWorldType;
	}

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
	 * Spawns the game mode the owning game instance picks for the URL (UE: SetGameMode; UGameInstance::
	 * CreateGameModeForURL with plan decision D18's precedence), once. False without a game instance or a game mode.
	 */
	bool SetGameMode(const FURL& InURL);

	/**
	 * Spawns a game mode of GameModeClass, once, and starts it when the world already plays (Leon: tests and tools
	 * that make a world without a map). Returns the game mode.
	 */
	AGameModeBase* SetGameMode(TSubclassOf<AGameModeBase> GameModeClass);

	/**
	 * Gets the actors ready for play (UE: InitializeActorsForPlay): registers the components that are not registered
	 * (UpdateWorldComponents), gives the game mode its map name and options (AGameModeBase::InitGame), then initializes
	 * the level's actors that are not initialized yet (UE: ULevel::RouteActorInitialize: PreInitializeComponents,
	 * InitializeComponents, PostInitializeComponents). A spawned actor is registered and initialized already; a loaded
	 * map's actors are done here.
	 */
	void InitializeActorsForPlay(const FURL& InURL, bool bResetTime = true);

	/**
	 * Logs a player in (UE: SpawnPlayActor): the game mode's Login makes its controller, which takes the player
	 * (APlayerController::SetPlayer), then PostLogin restarts it. Null with Error when the login failed.
	 */
	APlayerController* SpawnPlayActor(UPlayer* NewPlayer, const FURL& InURL, FString& Error);

	/** True once the world plays: actors spawned from then on begin play at once (UE: HasBegunPlay). */
	[[nodiscard]] bool HasBegunPlay() const
	{
		return bBegunPlay;
	}
	/**
	 * Starts play (UE: BeginPlay): the game mode's StartPlay, then every actor that has not begun play. LoadMap calls
	 * it once the players logged in.
	 */
	void BeginPlay();

	/** The persistent level's world settings, or null (UE: GetWorldSettings). */
	[[nodiscard]] AWorldSettings* GetWorldSettings() const;

	/** The first player controller of the level, or null (UE: GetFirstPlayerController). */
	[[nodiscard]] APlayerController* GetFirstPlayerController() const;

	/** The map's name: the world's name (UE: GetMapName). */
	[[nodiscard]] FString GetMapName() const;

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

	/**
	 * Recreates FPhysScene with another backend: the slope planes and the bodies added by hand go, and every registered
	 * primitive gets its body again (RecreatePhysicsBodies).
	 */
	void SetPhysicsBackend(EPhysicsBackend PhysicsBackend);

	/**
	 * Clears the physics scene and recreates the physics state of every registered primitive component, in actor then
	 * component order (Leon; UE recreates one component's state at a time): the bodies start again from the components'
	 * transforms, at rest.
	 */
	void RecreatePhysicsBodies();

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

	/**
	 * Ticks the actors, then updates the player controllers' camera managers (UE: UWorld::Tick updates the cameras
	 * last). A character moves in its tick (UCharacterMovementComponent::TickComponent); TickGameplayFrame adds the
	 * physics step and the pawn separation, and is what the game engine runs.
	 */
	void Tick(float InDeltaTime);

	/** The delta time of the current or last tick, seconds (UE: GetDeltaSeconds). */
	[[nodiscard]] float GetDeltaSeconds() const
	{
		return DeltaTimeSeconds;
	}

	/**
	 * The game's frame (UGameEngine::Tick): Tick (the controllers' input, then the pawns: the characters move, the
	 * camera managers last) → the pawns separate → FPhysScene::Step → the characters leave the bodies they overlap
	 * and separate again → the simulated bodies move their components (FPhysScene::SyncComponentsToBodies) → debug
	 * draws.
	 */
	void TickGameplayFrame(const FWorldGameplayFrameParams& Params);

	/**
	 * Sends the renderer what changed since the last frame (UE: SendAllEndOfFrameUpdates): every registered
	 * component's world transform and per-frame data (a skinned mesh's pose) go to its proxy. The engine calls it
	 * before drawing the world.
	 */
	void SendAllEndOfFrameUpdates();

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

	FPhysScene Physics{};
	UNavigationSystem Navigation{};
	EWorldType::Type WorldType = EWorldType::None;
	uint64 NextUniqueID = 0;
	/** UE: DeltaTimeSeconds. */
	float DeltaTimeSeconds = 0.0f;
	bool bBegunPlay = false;
	bool bTicking = false;
	bool bIsTearingDown = false;
	bool bIsWorldInitialized = false;
	/** A DestroyActor during the tick left null slots in the level. */
	bool bHasNullActorSlots = false;
};
