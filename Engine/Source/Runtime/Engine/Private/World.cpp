#include "Engine/World.h"

#include "BodyInstance.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/Player.h"
#include "Engine/PointLight.h"
#include "EngineLogs.h"
#include "EngineStats.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RendererInterface.h"
#include "SceneInterface.h"
#include "Stats/Stats.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

DECLARE_CYCLE_STAT(TEXT("Tick Actors"), STAT_TickActors, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Timers"), STAT_TickTimers, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Physics Step"), STAT_WorldPhysicsStep, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Character Overlaps"), STAT_CharacterOverlaps, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Spawn Actor"), STAT_SpawnActor, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Construct Actor"), STAT_ConstructActor, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("Post Actor Construction"), STAT_PostActorConstruction, STATGROUP_Engine);

FActorSpawnParameters::FActorSpawnParameters()
	: bDeferConstruction(false)
	, bNoFail(false)
{
}

UWorld::UWorld(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UWorld* UWorld::CreateWorld(EWorldType::Type InWorldType, bool /*bInformEngineOfWorld*/, FName WorldName,
	UPackage* InWorldPackage, bool bAddToRoot, const InitializationValues* InIVS)
{
	UPackage* WorldPackage = InWorldPackage;
	if (WorldPackage == nullptr)
	{
		// UE: CreatePackage(nullptr) names the package "/Temp/Untitled_<N>". A transient package is never saved.
		static int32 NextUntitledIndex = 0;
		FString PackageName;
		do
		{
			PackageName = FString::Printf(TEXT("/Temp/Untitled_%d"), NextUntitledIndex++);
		} while (FindPackage(nullptr, *PackageName) != nullptr);
		WorldPackage = CreatePackage(*PackageName);
		WorldPackage->SetFlags(RF_Transient);
	}
	const FName NewWorldName = WorldName.IsNone() ? FName(*FPackageName::GetShortName(WorldPackage)) : WorldName;

	// A map's world is its package's asset (UE: UWorldFactory makes it public and standalone).
	const EObjectFlags WorldFlags = InWorldPackage != nullptr ? RF_Public | RF_Standalone : RF_NoFlags;
	UWorld* NewWorld = NewObject<UWorld>(WorldPackage, NewWorldName, WorldFlags);
	NewWorld->WorldType = InWorldType;
	NewWorld->PersistentLevel = NewObject<ULevel>(NewWorld, TEXT("PersistentLevel"));
	NewWorld->InitWorld(InIVS != nullptr ? *InIVS : InitializationValues());
	if (bAddToRoot)
	{
		NewWorld->AddToRoot();
	}
	return NewWorld;
}

UWorld* UWorld::FindWorldInPackage(UPackage* Package)
{
	if (Package == nullptr)
	{
		return nullptr;
	}
	TArray<UObject*> Objects;
	GetObjectsWithOuter(Package, Objects, /*bIncludeNestedObjects =*/false);
	for (UObject* Object : Objects)
	{
		if (UWorld* World = Cast<UWorld>(Object))
		{
			return World;
		}
	}
	return nullptr;
}

void UWorld::InitWorld(const InitializationValues IVS)
{
	if (bIsWorldInitialized)
	{
		return;
	}
	bIsWorldInitialized = true;
	if (PersistentLevel == nullptr)
	{
		PersistentLevel = NewObject<ULevel>(this, TEXT("PersistentLevel"));
	}
	PersistentLevel->OwningWorld = this;
	// A loaded map's actors were saved in spawn order: they take their IDs in that order, before anything spawns, and
	// their places in the level (the tick order).
	for (AActor* Actor : PersistentLevel->Actors)
	{
		if (Actor != nullptr && Actor->GetUniqueID() == 0)
		{
			Actor->SetUniqueID(++NextUniqueID);
		}
		if (Actor != nullptr && Actor->LevelOrder == 0)
		{
			Actor->LevelOrder = ++NextLevelOrder;
		}
	}
	// UE: InitWorld allocates the scene unless the engine never renders (-nullrhi, a dedicated server).
	if (IVS.bInitializeScenes && FApp::CanEverRender())
	{
		if (IRendererModule* RendererModule = GetRendererModulePtr())
		{
			Scene = RendererModule->AllocateScene(this);
		}
	}
}

void UWorld::UpdateWorldComponents(bool /*bRerunConstructionScripts*/, bool /*bCurrentLevelOnly*/)
{
	if (PersistentLevel == nullptr)
	{
		return;
	}
	// Registration may spawn (a component creating an actor): walk a copy.
	const TArray<AActor*> Actors = PersistentLevel->Actors;
	for (AActor* Actor : Actors)
	{
		if (Actor != nullptr && !Actor->IsPendingKillPending())
		{
			Actor->RegisterAllComponents();
		}
	}
}

void UWorld::BeginDestroy()
{
	// A world the collector frees without DestroyWorld (a world made outside the root set) still frees its scene.
	if (Scene != nullptr)
	{
		GetRendererModule().RemoveScene(Scene);
		Scene = nullptr;
	}
	Super::BeginDestroy();
}

void UWorld::DestroyWorld(bool /*bInformEngineOfWorld*/)
{
	if (bIsTearingDown)
	{
		return;
	}
	bIsTearingDown = true;

	// Every actor ends play in spawn order (UE: the level's actors end play with the world); spawns still waiting for
	// the end of a tick never began.
	TArray<AActor*> Actors;
	if (PersistentLevel != nullptr)
	{
		Actors = PersistentLevel->Actors;
	}
	for (AActor* Actor : Actors)
	{
		if (Actor != nullptr && !Actor->IsPendingKill())
		{
			Actor->RouteEndPlay(EEndPlayReason::Quit);
		}
	}
	Actors.Append(PendingSpawnActors);
	for (AActor* Actor : Actors)
	{
		if (Actor != nullptr)
		{
			Actor->UnregisterAllComponents();
		}
	}
	PendingSpawnActors.Empty();
	// Every component left the scene when it unregistered: the scene goes now.
	if (Scene != nullptr)
	{
		GetRendererModule().RemoveScene(Scene);
		Scene = nullptr;
	}
	AuthorityGameMode = nullptr;
	GameState = nullptr;
	if (PersistentLevel != nullptr)
	{
		PersistentLevel->Actors.Empty();
	}
	TimerManager.ClearAllTimers();
	TickTaskManager.UnregisterAll();
	Physics.Clear();
	Navigation.Clear();

	// Everything inside the world goes with it (UE: MarkObjectsPendingKill): references held elsewhere are cleared by
	// the next collection instead of keeping the objects alive.
	TArray<UObject*> Inner;
	GetObjectsWithOuter(this, Inner, /*bIncludeNestedObjects =*/true);
	for (UObject* Object : Inner)
	{
		Object->MarkPendingKill();
	}
	RemoveFromRoot();
	MarkPendingKill();
}

bool UWorld::SetGameMode(const FURL& InURL)
{
	if (AuthorityGameMode == nullptr && OwningGameInstance != nullptr)
	{
		AuthorityGameMode = OwningGameInstance->CreateGameModeForURL(InURL, this);
	}
	return AuthorityGameMode != nullptr;
}

AGameModeBase* UWorld::SetGameMode(TSubclassOf<AGameModeBase> GameModeClass)
{
	if (AuthorityGameMode == nullptr && GameModeClass != nullptr)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		AuthorityGameMode = SpawnActor<AGameModeBase>(GameModeClass, SpawnInfo);
		if (AuthorityGameMode != nullptr && bBegunPlay)
		{
			AuthorityGameMode->StartPlay();
		}
	}
	return AuthorityGameMode;
}

void UWorld::InitializeActorsForPlay(const FURL& InURL, bool /*bResetTime*/)
{
	// A loaded map's components register here; spawned actors registered theirs when they spawned.
	UpdateWorldComponents();
	if (AuthorityGameMode != nullptr)
	{
		FString Options;
		for (const FString& Option : InURL.Op)
		{
			Options += TEXT("?");
			Options += Option;
		}
		FString Error;
		AuthorityGameMode->InitGame(FPaths::GetBaseFilename(InURL.Map), Options, Error);
		if (!Error.IsEmpty())
		{
			UE_LOG(LogWorld, Warning, TEXT("InitGame: %s"), *Error);
		}
	}
	// UE: ULevel::RouteActorInitialize, in level order.
	if (PersistentLevel != nullptr)
	{
		const TArray<AActor*> Actors = PersistentLevel->Actors;
		for (AActor* Actor : Actors)
		{
			if (Actor != nullptr && !Actor->IsActorInitialized())
			{
				PostActorConstruction(Actor);
			}
		}
	}
}

APlayerController* UWorld::SpawnPlayActor(UPlayer* NewPlayer, const FURL& InURL, FString& Error)
{
	Error.Empty();
	FString Options;
	for (const FString& Option : InURL.Op)
	{
		Options += TEXT("?");
		Options += Option;
	}
	AGameModeBase* const GameMode = GetAuthGameMode();
	if (GameMode == nullptr)
	{
		Error = TEXT("No game mode set");
		UE_LOG(LogSpawn, Warning, TEXT("Login failed: No game mode set."));
		return nullptr;
	}
	// The game mode accepts the login and makes the controller.
	APlayerController* const NewPlayerController = GameMode->Login(NewPlayer, InURL.Portal, Options, Error);
	if (NewPlayerController == nullptr)
	{
		UE_LOG(LogSpawn, Warning, TEXT("Login failed: %s"), *Error);
		return nullptr;
	}
	UE_LOG(LogSpawn, Log, TEXT("%s got player %s"), *NewPlayerController->GetName(), *NewPlayer->GetName());
	// The controller takes the player, then the game mode finishes the login (and restarts the player).
	NewPlayerController->SetPlayer(NewPlayer);
	GameMode->PostLogin(NewPlayerController);
	return NewPlayerController;
}

void UWorld::BeginPlay()
{
	bBegunPlay = true;
	// The waypoint graph of the level (the map's ANavigationWaypoint actors).
	Navigation.Build(*this);
	if (AuthorityGameMode != nullptr)
	{
		AuthorityGameMode->StartPlay();
	}
	ForEach<AActor>([](AActor& Actor) { Actor.DispatchBeginPlay(); });
}

AWorldSettings* UWorld::GetWorldSettings() const
{
	return PersistentLevel != nullptr ? PersistentLevel->GetWorldSettings() : nullptr;
}

bool UWorld::IsPaused() const
{
	const AWorldSettings* Settings = GetWorldSettings();
	return Settings != nullptr && Settings->GetPauserPlayerState() != nullptr;
}

float UWorld::GetGravityZ() const
{
	const AWorldSettings* Settings = GetWorldSettings();
	return Settings != nullptr && Settings->GlobalGravityZ != 0.0f ? Settings->GlobalGravityZ : DefaultGravityZ;
}

APlayerController* UWorld::GetFirstPlayerController() const
{
	return FindFirst<APlayerController>();
}

FString UWorld::GetMapName() const
{
	return GetName();
}

AActor* UWorld::SpawnActor(
	UClass* Class, const FVector* Location, const FRotator* Rotation, const FActorSpawnParameters& SpawnParameters)
{
	return SpawnActorInternal(Class, Location, Rotation, nullptr, SpawnParameters);
}

AActor* UWorld::SpawnActorInternal(UClass* Class, const FVector* Location, const FRotator* Rotation,
	const FTransform* Transform, const FActorSpawnParameters& SpawnParameters)
{
	SCOPE_CYCLE_COUNTER(STAT_SpawnActor);
	if (Class == nullptr)
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because no class was specified"));
		return nullptr;
	}
	if (!Class->IsChildOf(AActor::StaticClass()))
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because %s is not an actor class"), *Class->GetName());
		return nullptr;
	}
	if (Class->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because class %s is abstract"), *Class->GetName());
		return nullptr;
	}
	if (bIsTearingDown)
	{
		UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because the world is being destroyed"));
		return nullptr;
	}
	ULevel* LevelToSpawnIn = SpawnParameters.OverrideLevel != nullptr ? SpawnParameters.OverrideLevel : PersistentLevel;
	if (!SpawnParameters.Name.IsNone() &&
		StaticFindObjectFast(nullptr, LevelToSpawnIn, SpawnParameters.Name) != nullptr)
	{
		UE_LOG(LogSpawn, Error, TEXT("SpawnActor failed because an object named %s already exists in %s"),
			*SpawnParameters.Name.ToString(), *LevelToSpawnIn->GetPathName());
		return nullptr;
	}

	AActor* Actor = nullptr;
	{
		SCOPE_CYCLE_COUNTER(STAT_ConstructActor);
		Actor = NewObject<AActor>(
			LevelToSpawnIn, Class, SpawnParameters.Name, SpawnParameters.ObjectFlags, SpawnParameters.Template);
	}
	Actor->SetUniqueID(++NextUniqueID);
	Actor->SetOwner(SpawnParameters.Owner);
	Actor->SetInstigator(SpawnParameters.Instigator);
	if (USceneComponent* Root = Actor->GetRootComponent())
	{
		// UE: the root takes the spawn transform before the components register, so their render and physics state
		// start where the actor is.
		if (Transform != nullptr)
		{
			Root->SetRelativeTransform(*Transform);
		}
		if (Location != nullptr)
		{
			Root->RelativeLocation = *Location;
		}
		if (Rotation != nullptr)
		{
			Root->RelativeRotation = *Rotation;
		}
	}

	// While the world ticks the actor joins the level once the tick ends (the tick loop never sees it half-made).
	if (bTicking)
	{
		PendingSpawnActors.Add(Actor);
	}
	else
	{
		LevelToSpawnIn->Actors.Add(Actor);
		Actor->LevelOrder = ++NextLevelOrder;
	}

	Actor->RegisterAllComponents();
	if (!SpawnParameters.bDeferConstruction)
	{
		SCOPE_CYCLE_COUNTER(STAT_PostActorConstruction);
		PostActorConstruction(Actor);
	}
	if (!Actor->IsPendingKillPending())
	{
		OnActorSpawned.Broadcast(Actor);
	}
	return Actor;
}

FDelegateHandle UWorld::AddOnActorSpawnedHandler(const FOnActorSpawned::FDelegate& InHandler)
{
	return OnActorSpawned.Add(InHandler);
}

void UWorld::RemoveOnActorSpawnedHandler(FDelegateHandle InHandle)
{
	(void)OnActorSpawned.Remove(InHandle);
}

APointLight* UWorld::AcquirePooledPointLight(const FVector& Location, float LifeSpan)
{
	if (bIsTearingDown || PersistentLevel == nullptr)
	{
		return nullptr;
	}
	// A light destroyed by someone else (a level cleared) leaves the pool, with its timer.
	for (int32 Index = PooledPointLights.Num() - 1; Index >= 0; --Index)
	{
		if (PooledPointLights[Index] == nullptr || PooledPointLights[Index]->IsPendingKillPending())
		{
			TimerManager.ClearTimer(PooledPointLightTimers[Index]);
			PooledPointLights.RemoveAt(Index);
			PooledPointLightTimers.RemoveAt(Index);
		}
	}
	int32 Slot = PooledPointLightTimers.IndexOfByPredicate(
		[this](const FTimerHandle& Handle) { return !TimerManager.IsTimerActive(Handle); });
	if (Slot == INDEX_NONE && PooledPointLights.Num() < MaxPooledPointLights)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		APointLight* Light = SpawnActor<APointLight>(Location, FRotator::ZeroRotator, SpawnInfo);
		if (Light == nullptr)
		{
			return nullptr;
		}
		// Out until the caller shows it with its values.
		Light->GetPointLightComponent()->SetVisibility(false);
		Slot = PooledPointLights.Add(Light);
		PooledPointLightTimers.AddDefaulted();
	}
	if (Slot == INDEX_NONE)
	{
		// Every light is on: the one that goes out first is taken.
		Slot = 0;
		for (int32 Index = 1; Index < PooledPointLightTimers.Num(); ++Index)
		{
			if (TimerManager.GetTimerRemaining(PooledPointLightTimers[Index]) <
				TimerManager.GetTimerRemaining(PooledPointLightTimers[Slot]))
			{
				Slot = Index;
			}
		}
	}
	APointLight* Light = PooledPointLights[Slot];
	(void)Light->SetActorLocation(Location);
	// A timer without a delegate (nothing to allocate): the step hides the light once it is no longer active.
	TimerManager.SetTimer(PooledPointLightTimers[Slot], FMath::Max(LifeSpan, KINDA_SMALL_NUMBER), false);
	return Light;
}

void UWorld::HideExpiredPooledPointLights()
{
	for (int32 Index = 0; Index < PooledPointLights.Num(); ++Index)
	{
		APointLight* Light = PooledPointLights[Index];
		if (Light != nullptr && !Light->IsPendingKillPending() &&
			!TimerManager.IsTimerActive(PooledPointLightTimers[Index]) && Light->GetPointLightComponent()->IsVisible())
		{
			Light->GetPointLightComponent()->SetVisibility(false);
		}
	}
}

AActor* UWorld::SpawnActor(UClass* Class, const FTransform* Transform, const FActorSpawnParameters& SpawnParameters)
{
	return SpawnActorInternal(Class, nullptr, nullptr, Transform, SpawnParameters);
}

void UWorld::PostActorConstruction(AActor* Actor)
{
	if (Actor == nullptr || Actor->IsPendingKillPending())
	{
		return;
	}
	Actor->PreInitializeComponents();
	Actor->InitializeComponents();
	Actor->PostInitializeComponents();
	Actor->bActorInitialized = true;
	if (bBegunPlay && !bTicking && !PendingSpawnActors.Contains(Actor))
	{
		Actor->DispatchBeginPlay();
	}
}

bool UWorld::DestroyActor(AActor* Actor, bool /*bNetForce*/, bool /*bShouldModifyLevel*/)
{
	if (Actor == nullptr || Actor->GetWorld() != this)
	{
		return false;
	}
	if (Actor->IsPendingKillPending())
	{
		return true;
	}
	Actor->bActorIsBeingDestroyed = true;
	Actor->Destroyed();
	Actor->RouteEndPlay(EEndPlayReason::Destroyed);
	Actor->UnregisterAllComponents();

	if (ULevel* Level = Actor->GetLevel())
	{
		const int32 Index = Level->Actors.Find(Actor);
		if (Index != INDEX_NONE)
		{
			if (bTicking || ActorIterationDepth > 0)
			{
				// The tick loop and ForEach walk the array by index: keep its size until they end.
				Level->Actors[Index] = nullptr;
				bHasNullActorSlots = true;
			}
			else
			{
				Level->Actors.RemoveAt(Index);
			}
		}
	}
	PendingSpawnActors.Remove(Actor);
	if (AuthorityGameMode == Actor)
	{
		AuthorityGameMode = nullptr;
	}
	if (GameState == Actor)
	{
		GameState = nullptr;
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component != nullptr)
		{
			Component->MarkPendingKill();
		}
	}
	Actor->MarkPendingKill();
	return true;
}

void UWorld::Tick(float InDeltaTime)
{
	RunTick(InDeltaTime, nullptr);
}

void UWorld::RunTickGroup(ETickingGroup Group, float InDeltaTime, ELevelTick TickType)
{
	// Spawns wait in PendingSpawnActors and destroys null their slot while a group runs: the level keeps its size
	// until the group ends, then the spawned actors join it and begin play.
	bTicking = true;
	TickTaskManager.RunTickGroup(Group, InDeltaTime, TickType);
	bTicking = false;
	FlushPendingSpawns();
	CompactActors();
}

void UWorld::RunTick(float InDeltaTime, const FWorldGameplayFrameParams* Params)
{
	LLM_SCOPE(ELLMTag::GameMisc);
	// UE's pause: the world's time, its timers, the physics and the effects stand still, and only what ticks when
	// paused runs (the players' input and UI).
	const bool bPaused = IsPaused();
	const ELevelTick TickType = bPaused ? LEVELTICK_PauseTick : LEVELTICK_All;
	DeltaTimeSeconds = InDeltaTime;
	// The time is counted in the timers' integer units and read as seconds from them: no float sum drifts (D4).
	RealTimeUnits += FTimerManager::SecondsToTimeUnits(InDeltaTime);
	RealTimeSeconds = FTimerManager::TimeUnitsToSeconds(RealTimeUnits);
	if (!bPaused)
	{
		TimeUnits += FTimerManager::SecondsToTimeUnits(InDeltaTime);
		TimeSeconds = FTimerManager::TimeUnitsToSeconds(TimeUnits);
	}
	TickTaskManager.StartFrame();
	if (!bPaused)
	{
		// The timers first: what a timer due now changes (a reload done, a round gone live) is what this step's actors
		// see, as if each of them had polled its deadline in its own tick (Leon; UE ticks them after TG_PostPhysics).
		SCOPE_CYCLE_COUNTER(STAT_TickTimers);
		bTicking = true;
		TimerManager.Tick(InDeltaTime);
		HideExpiredPooledPointLights();
		bTicking = false;
		FlushPendingSpawns();
		CompactActors();
	}
	{
		// The controllers process their input and their pawns tick after them (AController::AddPawnTickDependency),
		// so a character's movement component moves it with this step's input.
		SCOPE_CYCLE_COUNTER(STAT_TickActors);
		RunTickGroup(TG_PrePhysics, InDeltaTime, TickType);
	}
	if (Params != nullptr && !bPaused)
	{
		SCOPE_CYCLE_COUNTER(STAT_WorldPhysicsStep);
		StepPhysics(*Params);
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_TickActors);
		RunTickGroup(TG_DuringPhysics, InDeltaTime, TickType);
		RunTickGroup(TG_PostPhysics, InDeltaTime, TickType);
	}

	// The cameras last, after every actor moved (UE).
	ForEach<APlayerController>(
		[InDeltaTime](APlayerController& PlayerController) { PlayerController.UpdateCameraManager(InDeltaTime); });
	{
		SCOPE_CYCLE_COUNTER(STAT_TickActors);
		RunTickGroup(TG_PostUpdateWork, InDeltaTime, TickType);
	}

	// The effects age with the world's time.
	if (!bPaused)
	{
		ImpactMarks.Tick(InDeltaTime);
		Tracers.Tick(InDeltaTime);
		EffectSprites.Tick(InDeltaTime);
	}

	if (Params != nullptr)
	{
		if (Params->CollisionDebugDraw != nullptr)
		{
			ForEach<ACharacter>(
				[&](ACharacter& Character)
				{
					Physics.AppendCollisionDebug(*Params->CollisionDebugDraw, Character.GetCapsule(),
						Character.GetActorLocation(),
						static_cast<SIZE_T>(Character.GetCapsuleComponent()->GetUniqueID()));
				});
		}
		if (Params->NavigationDebugDraw != nullptr)
		{
			Navigation.AppendDebugDraw(*Params->NavigationDebugDraw);
		}
	}

	// The step's state goes to the scene: the render draws between the last two steps (D4).
	if (Scene != nullptr)
	{
		SCOPE_CYCLE_COUNTER(STAT_EndOfFrameUpdates);
		SendAllEndOfFrameUpdates();
	}
}

void UWorld::FlushPendingSpawns()
{
	// A BeginPlay may spawn again (not ticking any more: those join the level directly).
	TArray<AActor*> Spawned = MoveTemp(PendingSpawnActors);
	PendingSpawnActors.Reset();
	for (AActor* Actor : Spawned)
	{
		if (Actor == nullptr || Actor->IsPendingKillPending())
		{
			continue;
		}
		ULevel* Level = Actor->GetLevel();
		if (Level == nullptr)
		{
			continue;
		}
		Level->Actors.Add(Actor);
		Actor->LevelOrder = ++NextLevelOrder;
		if (bBegunPlay && Actor->IsActorInitialized())
		{
			Actor->DispatchBeginPlay();
		}
	}
}

void UWorld::CompactActors()
{
	if (PersistentLevel == nullptr)
	{
		return;
	}
	if (!bHasNullActorSlots)
	{
		return;
	}
	bHasNullActorSlots = false;
	PersistentLevel->Actors.RemoveAll([](const AActor* Actor) { return Actor == nullptr || Actor->IsPendingKill(); });
}

SIZE_T UWorld::ActorCount() const
{
	SIZE_T Count = 0;
	ForEach<AActor>([&Count](AActor&) { ++Count; });
	return Count;
}

void UWorld::RecreatePhysicsBodies()
{
	Physics.Clear();
	if (PersistentLevel == nullptr)
	{
		return;
	}
	for (AActor* Actor : PersistentLevel->Actors)
	{
		if (Actor == nullptr || Actor->IsPendingKillPending())
		{
			continue;
		}
		for (UActorComponent* Component : Actor->GetComponents())
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			if (Primitive != nullptr && Primitive->IsRegistered() && !Primitive->IsPendingKill())
			{
				Primitive->RecreatePhysicsState();
			}
		}
	}
}

namespace
{

	/** The characters a world separates: a match's worth without an allocation. */
	using FCharacterArray = TArray<ACharacter*, TInlineAllocator<32>>;

	/** How far (cm) a push may move a character before its pairs are looked up again. */
	constexpr float CharacterPairSlack = 16.0f;

	/** Two characters by their place in the list, First before Second. */
	struct FCharacterPair
	{
		int32 First;
		int32 Second;
	};

	/**
	 * One pass of ACharacter::ResolvePawnOverlap over the pairs of characters in list order ((0, 1), (0, 2), ...,
	 * (1, 2), ...), without the pairs too far apart to touch: a sort and sweep on X finds the others, and when a push
	 * moves a character further than the slack the pairs not resolved yet are found again.
	 */
	void ResolveCharacterPairs(const FCharacterArray& Characters)
	{
		const int32 Num = Characters.Num();
		bool bAnyDone = false;
		FCharacterPair Last{0, 0};
		for (;;)
		{
			// A character's location is its root's (ResolvePawnOverlap moves it there).
			TArray<FVector, TInlineAllocator<32>> Anchors;
			TArray<int32, TInlineAllocator<32>> ByX;
			float MaxRadius = 0.0f;
			for (int32 Index = 0; Index < Num; ++Index)
			{
				Anchors.Add(Characters[Index]->GetRootComponent()->GetRelativeLocation());
				ByX.Add(Index);
				MaxRadius = FMath::Max(MaxRadius, FMath::Abs(Characters[Index]->GetCapsule().GetCapsuleRadius()));
			}
			ByX.Sort([&Anchors](const int32 A, const int32 B)
				{ return Anchors[A].X < Anchors[B].X || (Anchors[A].X == Anchors[B].X && A < B); });

			const float Reach = 2.0f * (MaxRadius + CharacterPairSlack);
			TArray<FCharacterPair, TInlineAllocator<64>> Pairs;
			for (int32 SortedA = 0; SortedA < Num; ++SortedA)
			{
				const int32 A = ByX[SortedA];
				for (int32 SortedB = SortedA + 1; SortedB < Num && Anchors[ByX[SortedB]].X - Anchors[A].X <= Reach;
					++SortedB)
				{
					const int32 B = ByX[SortedB];
					if (FMath::Abs(Anchors[B].Y - Anchors[A].Y) > Reach)
					{
						continue;
					}
					const FCharacterPair Pair{FMath::Min(A, B), FMath::Max(A, B)};
					if (bAnyDone &&
						(Pair.First < Last.First || (Pair.First == Last.First && Pair.Second <= Last.Second)))
					{
						continue;
					}
					Pairs.Add(Pair);
				}
			}
			Pairs.Sort([](const FCharacterPair& L, const FCharacterPair& R)
				{ return L.First < R.First || (L.First == R.First && L.Second < R.Second); });

			auto HasDrifted = [&](int32 Index)
			{
				const FVector Location = Characters[Index]->GetRootComponent()->GetRelativeLocation();
				return FMath::Abs(Location.X - Anchors[Index].X) > CharacterPairSlack ||
					FMath::Abs(Location.Y - Anchors[Index].Y) > CharacterPairSlack;
			};
			bool bDrifted = false;
			for (const FCharacterPair& Pair : Pairs)
			{
				bAnyDone = true;
				Last = Pair;
				Characters[Pair.First]->ResolvePawnOverlap(*Characters[Pair.Second]);
				if (HasDrifted(Pair.First) || HasDrifted(Pair.Second))
				{
					bDrifted = true;
					break;
				}
			}
			if (!bDrifted)
			{
				return;
			}
		}
	}

} // namespace

void UWorld::ResolveCharacterOverlaps()
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterOverlaps);
	// Inline storage: called twice a frame, it allocates nothing for a match's characters.
	FCharacterArray Characters;
	ForEach<ACharacter>([&](ACharacter& Character) { Characters.Add(&Character); });
	if (Characters.Num() < 2)
	{
		return;
	}

	// Flow: collect live Characters → the pairs close enough to touch → equal XY depenetration (3 passes).
	constexpr int32 Iterations = 3;
	for (int32 Iter = 0; Iter < Iterations; ++Iter)
	{
		ResolveCharacterPairs(Characters);
	}
	// The capsules' bodies follow (P17: traces hit characters).
	for (ACharacter* Character : Characters)
	{
		Character->GetCapsuleComponent()->SendPhysicsTransform();
	}
}

void UWorld::TickGameplayFrame(const FWorldGameplayFrameParams& Params)
{
	RunTick(Params.DeltaTime, &Params);
}

void UWorld::StepPhysics(const FWorldGameplayFrameParams& Params)
{
	// The pawns separate, the bodies step, the characters leave what they overlap and separate again, then the
	// simulated bodies move their components.
	ResolveCharacterOverlaps();

	FPhysSceneStepParams Step{};
	Step.DeltaTime = Params.DeltaTime;
	if (Params.bOverridePhysicsStep)
	{
		Step.Damping = Params.PhysicsDamping;
		Step.WalkBounds = Params.PhysicsWalkBounds;
		Step.Gravity = Params.PhysicsGravity;
		Step.FloorZ = Params.PhysicsFloorZ;
		Step.Skin = Params.PhysicsSkin;
	}
	else if (ACharacter* Primary = FindFirst<ACharacter>())
	{
		const UCharacterMovementComponent& MoveCfg = Primary->GetCharacterMovement();
		Step.Damping = MoveCfg.PushDamping;
		Step.WalkBounds = MoveCfg.WalkBounds;
		Step.Gravity = MoveCfg.Gravity;
		Step.FloorZ = MoveCfg.FloorZ;
		Step.Skin = MoveCfg.Skin;
	}
	Physics.Step(Step);

	ForEach<ACharacter>([](ACharacter& Character) { Character.ResolveOverlaps(); });
	ResolveCharacterOverlaps();

	Physics.SyncComponentsToBodies();
}

void UWorld::SendAllEndOfFrameUpdates()
{
	if (Scene == nullptr || PersistentLevel == nullptr)
	{
		return;
	}
	for (AActor* Actor : PersistentLevel->Actors)
	{
		if (Actor == nullptr || Actor->IsPendingKillPending())
		{
			continue;
		}
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->IsRenderStateCreated())
			{
				Component->SendRenderTransform_Concurrent();
				Component->SendRenderDynamicData_Concurrent();
			}
		}
	}
}

void UWorld::Clear()
{
	for (AActor* Actor : PendingSpawnActors)
	{
		if (Actor != nullptr)
		{
			Actor->UnregisterAllComponents();
			Actor->MarkPendingKill();
		}
	}
	PendingSpawnActors.Empty();
	if (PersistentLevel != nullptr)
	{
		const TArray<AActor*> Actors = PersistentLevel->Actors;
		for (AActor* Actor : Actors)
		{
			DestroyActor(Actor);
		}
		PersistentLevel->Actors.RemoveAll([](const AActor* Actor) { return Actor == nullptr; });
	}
	Physics.Clear();
	Navigation.Clear();
}
