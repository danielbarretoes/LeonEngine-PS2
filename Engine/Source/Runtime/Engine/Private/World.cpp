#include "Engine/World.h"

#include "BodyInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/Player.h"
#include "EngineLogs.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RendererInterface.h"
#include "SceneInterface.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

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
	UPackage* InWorldPackage, bool bAddToRoot)
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

	UWorld* NewWorld = NewObject<UWorld>(WorldPackage, NewWorldName);
	NewWorld->WorldType = InWorldType;
	NewWorld->InitWorld();
	if (bAddToRoot)
	{
		NewWorld->AddToRoot();
	}
	return NewWorld;
}

void UWorld::InitWorld()
{
	PersistentLevel = NewObject<ULevel>(this, TEXT("PersistentLevel"));
	PersistentLevel->OwningWorld = this;
	// UE: InitWorld allocates the scene unless the engine never renders (-nullrhi, a dedicated server).
	if (FApp::CanEverRender())
	{
		if (IRendererModule* RendererModule = GetRendererModulePtr())
		{
			Scene = RendererModule->AllocateScene(this);
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
	// UE registers the loaded actors' components and initializes them here; Leon's actors did both when they spawned.
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

	AActor* Actor = NewObject<AActor>(
		LevelToSpawnIn, Class, SpawnParameters.Name, SpawnParameters.ObjectFlags, SpawnParameters.Template);
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
	}

	Actor->RegisterAllComponents();
	if (!SpawnParameters.bDeferConstruction)
	{
		PostActorConstruction(Actor);
	}
	return Actor;
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
			if (bTicking)
			{
				// The tick loop walks the array by index: keep its size until the tick ends.
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
	bTicking = true;
	if (PersistentLevel != nullptr)
	{
		// Spawns wait in PendingSpawnActors and destroys null their slot: the array keeps its size during the loop.
		const int32 NumActors = PersistentLevel->Actors.Num();
		for (int32 Index = 0; Index < NumActors; ++Index)
		{
			AActor* Actor = PersistentLevel->Actors[Index];
			if (Actor != nullptr && !Actor->IsPendingKillPending())
			{
				Actor->TickActor(InDeltaTime);
			}
		}
	}
	bTicking = false;
	FlushPendingSpawns();
	CompactActors();
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

void UWorld::SetPhysicsBackend(EPhysicsBackend PhysicsBackend)
{
	Physics = FPhysScene(PhysicsBackend);
	RecreatePhysicsBodies();
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

void UWorld::ResolveCharacterOverlaps()
{
	TArray<ACharacter*> Characters;
	ForEach<ACharacter>([&](ACharacter& Character) { Characters.Add(&Character); });
	if (Characters.Num() < 2)
	{
		return;
	}

	// Flow: collect live Characters → iterate pairs → equal XY depenetration (2–3 passes).
	constexpr int Iterations = 3;
	for (int Iter = 0; Iter < Iterations; ++Iter)
	{
		for (int32 I = 0; I < Characters.Num(); ++I)
		{
			for (int32 J = I + 1; J < Characters.Num(); ++J)
			{
				Characters[I]->ResolvePawnOverlap(*Characters[J]);
			}
		}
	}
}

void UWorld::TickGameplayFrame(const FWorldGameplayFrameParams& Params)
{
	ForEach<ACharacter>(
		[&](ACharacter& Character) { Character.TickCharacterMovement(Params.DeltaTime, Params.CollisionDebugDraw); });
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

	Tick(Params.DeltaTime);

	Physics.SyncComponentsToBodies();

	if (Params.CollisionDebugDraw != nullptr)
	{
		ForEach<ACharacter>(
			[&](ACharacter& Character)
			{
				Physics.AppendCollisionDebug(
					*Params.CollisionDebugDraw, Character.GetCapsule(), Character.GetActorLocation(), NoComponentID);
			});
	}

	if (Params.NavMeshDebugDraw != nullptr)
	{
		Navigation.AppendDebugDraw(*Params.NavMeshDebugDraw);
	}
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
}
