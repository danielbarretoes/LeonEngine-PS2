#include "ShooterGameMode.h"

#include "AI/Navigation/NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformMisc.h"
#include "HAL/UnrealMemory.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "ShooterAIController.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameState.h"
#include "ShooterHUD.h"
#include "ShooterPawnSensingComponent.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "TimerManager.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectBase.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Weapons/ShooterProjectile.h"
#include "Weapons/ShooterSmokeCloud.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_Projectile.h"

namespace
{

	/** A start's capsule radius when it has none, and a pawn's when it is not a character (cm, UE's 40 x 92). */
	constexpr float DefaultStartRadius = 40.0f;

	/** How high above the feet a pawn is tested against a zone (the volumes stand on the floor), cm. */
	constexpr float ZoneTestHeight = 50.0f;

	/**
	 * The bots' lookouts (GetBombSiteLookouts): the waypoints' flag; how far apart the directions one watches are
	 * (degrees) and how many; how far from its site one watches over it first (cm); how many waypoints stand in for
	 * them on a map without.
	 */
	const FName LookoutFlag(TEXT("Lookout"));
	constexpr float LookoutWatchSpacing = 35.0f;
	constexpr int32 MaxLookoutWatchYaws = 4;
	constexpr float LookoutOverSiteDistance = 800.0f;
	constexpr int32 MaxFallbackLookouts = 3;

	/** The half height of a start's capsule: its location is the capsule's centre (UE). */
	float GetStartHalfHeight(const AActor& StartSpot)
	{
		const APlayerStart* Start = Cast<APlayerStart>(&StartSpot);
		const UCapsuleComponent* Capsule = Start != nullptr ? Start->GetCapsuleComponent() : nullptr;
		return Capsule != nullptr ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
	}

	/**
	 * True when a live pawn stands on the start: within two capsule radii horizontally and 2 m vertically (the
	 * registered shooter pawns: the spectators fly and the dead lie down).
	 */
	bool IsStartOccupied(const TArray<AShooterCharacter*>& Pawns, const APlayerStart& Start)
	{
		const FVector StartLocation = Start.GetActorLocation();
		const UCapsuleComponent* StartCapsule = Start.GetCapsuleComponent();
		const float StartRadius = StartCapsule != nullptr ? StartCapsule->GetScaledCapsuleRadius() : DefaultStartRadius;
		for (const AShooterCharacter* Pawn : Pawns)
		{
			if (Pawn->IsPendingKillPending() || !Pawn->IsAlive())
			{
				continue;
			}
			const float PawnRadius = Pawn->GetCapsule().GetCapsuleRadius();
			const FVector Delta = Pawn->GetActorLocation() - StartLocation;
			const float MinDistance = StartRadius + PawnRadius;
			constexpr float VerticalReach = 200.0f;
			if (Delta.SizeSquared2D() < FMath::Square(MinDistance) && FMath::Abs(Delta.Z) < VerticalReach)
			{
				return true;
			}
		}
		return false;
	}

	/** A player's name for the log: its player state's, else its controller's or its pawn's object name. */
	FString GetDisplayName(const AController* Controller, const APawn* Pawn)
	{
		const APlayerState* State = Controller != nullptr ? Controller->GetPlayerState<APlayerState>() : nullptr;
		if (State != nullptr && !State->GetPlayerName().IsEmpty())
		{
			return State->GetPlayerName();
		}
		if (Controller != nullptr)
		{
			return Controller->GetName();
		}
		return Pawn != nullptr ? Pawn->GetName() : FString(TEXT("?"));
	}

	/** What a kill is credited to: the feed's weapon name and the killer's reward (CS: $300 without a weapon). */
	struct FKillCredit
	{
		FString WeaponName;
		int32 Reward = 300;
	};

	/** The credit of a damage causer: a weapon, a projectile (its thrower's, copied at the throw), the bomb. */
	FKillCredit GetKillCredit(const AActor* DamageCauser)
	{
		FKillCredit Credit;
		if (const AShooterWeapon* Weapon = Cast<AShooterWeapon>(DamageCauser))
		{
			Credit.WeaponName = Weapon->WeaponName;
			Credit.Reward = Weapon->KillReward;
		}
		else if (const AShooterProjectile* Projectile = Cast<AShooterProjectile>(DamageCauser))
		{
			Credit.WeaponName = Projectile->WeaponName;
			Credit.Reward = Projectile->KillReward;
		}
		else
		{
			Credit.WeaponName =
				DamageCauser != nullptr && DamageCauser->IsA<AShooterBomb>() ? TEXT("c4") : TEXT("world");
		}
		return Credit;
	}

	/** The controller that owns a player state (AController::InitPlayerState spawns it with the controller as owner).
	 */
	AController* GetStateController(const APlayerState* State)
	{
		return State != nullptr ? Cast<AController>(State->GetOwner()) : nullptr;
	}

	/** Removes Actor from a registry, keeping the order of the rest (the level's). */
	template <typename T>
	void RemoveFromRegistry(TArray<T*>& Registry, const AActor* Actor)
	{
		const int32 Index = Registry.IndexOfByKey(Actor);
		if (Index != INDEX_NONE)
		{
			Registry.RemoveAt(Index);
		}
	}

	/** The equipment names Buy takes besides the weapons' (CS's buy commands). */
	const TCHAR* const VestItem = TEXT("vest");
	const TCHAR* const VestHelmetItem = TEXT("vesthelm");
	const TCHAR* const DefuserItem = TEXT("defuser");
	const TCHAR* const PrimaryAmmoItem = TEXT("primammo");
	const TCHAR* const SecondaryAmmoItem = TEXT("secammo");

	/** The weapon a box of ammunition is for: the primary for primammo, the pistol for secammo, else null. */
	AShooterWeapon* GetAmmoWeapon(const AShooterCharacter& Buyer, const FString& Item)
	{
		if (Item.Equals(PrimaryAmmoItem, ESearchCase::IgnoreCase))
		{
			return Buyer.GetWeaponInSlot(EShooterWeaponSlot::Primary);
		}
		if (Item.Equals(SecondaryAmmoItem, ESearchCase::IgnoreCase))
		{
			return Buyer.GetWeaponInSlot(EShooterWeaponSlot::Secondary);
		}
		return nullptr;
	}

	/** The ammunition items. */
	bool IsAmmoItem(const FString& Item)
	{
		return Item.Equals(PrimaryAmmoItem, ESearchCase::IgnoreCase) ||
			Item.Equals(SecondaryAmmoItem, ESearchCase::IgnoreCase);
	}

} // namespace

const FName AShooterGameMode::BombSiteTag(TEXT("BombSite"));
const FName AShooterGameMode::BuyZoneTag(TEXT("BuyZone"));
const FName AShooterGameMode::LadderTag(TEXT("Ladder"));

AShooterGameMode::AShooterGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The warmup's fill and start, the eliminations and the bot match's checks run in the game mode's tick.
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AShooterCharacter::StaticClass();
	PlayerControllerClass = AShooterPlayerController::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	HUDClass = AShooterHUD::StaticClass();
	GameStateClass = AShooterGameState::StaticClass();
	BotControllerClass = AShooterAIController::StaticClass();
	BombClass = AShooterBomb::StaticClass();
}

void AShooterGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	RandomSeed = UGameplayStatics::GetIntOption(Options, TEXT("seed"), RandomSeed);
	const TCHAR* CmdLine = FCommandLine::Get();
	(void)FParse::Value(CmdLine, TEXT("seed="), RandomSeed);
	if (FParse::Param(CmdLine, TEXT("botmatch")))
	{
		bBotMatch = true;
		bFillTeamsWithBots = true;
		(void)FParse::Value(CmdLine, TEXT("rounds="), BotMatchRounds);
		// The match is that long (mp_maxrounds): its halftime and its majority come from it.
		BotMatchRounds = FMath::Max(BotMatchRounds, 1);
		MaxRounds = BotMatchRounds;
		UE_LOG(LogShooter, Display, TEXT("Botmatch: %d round(s), seed %d"), BotMatchRounds, RandomSeed);
	}
	RequestGameplayAssets();
}

namespace
{
	/**
	 * The soft object paths in a property's value: an FSoftObjectPath, and those inside a struct (a surface's sounds,
	 * FShooterSurfaceSounds) or an array (sound variants).
	 */
	void GatherValuePaths(const FProperty* Property, const void* Value, TArray<FSoftObjectPath>& OutPaths)
	{
		static const FName SoftObjectPathName(TEXT("SoftObjectPath"));
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == nullptr)
			{
				return;
			}
			if (StructProperty->Struct->GetFName() == SoftObjectPathName)
			{
				const FSoftObjectPath& Path = *static_cast<const FSoftObjectPath*>(Value);
				if (!Path.IsNull())
				{
					OutPaths.AddUnique(Path);
				}
				return;
			}
			for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
			{
				for (int32 Index = 0; Index < It->ArrayDim; ++Index)
				{
					GatherValuePaths(*It, It->ContainerPtrToValuePtr<void>(Value, Index), OutPaths);
				}
			}
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				GatherValuePaths(ArrayProperty->Inner, Helper.GetRawPtr(Index), OutPaths);
			}
		}
	}

	/**
	 * The soft object paths a class default names (its FSoftObjectPath properties, also in structs and arrays), and
	 * those of the actor classes it names (TSubclassOf: a weapon's projectile), once each class.
	 */
	void GatherSoftObjectPaths(UClass* Class, TArray<UClass*>& Visited, TArray<FSoftObjectPath>& OutPaths)
	{
		if (Class == nullptr || Visited.Contains(Class) || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			return;
		}
		Visited.Add(Class);
		const UObject* Defaults = Class->GetDefaultObject();
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			if (CastField<FStructProperty>(*It) != nullptr || CastField<FArrayProperty>(*It) != nullptr)
			{
				for (int32 Index = 0; Index < It->ArrayDim; ++Index)
				{
					GatherValuePaths(*It, It->ContainerPtrToValuePtr<void>(Defaults, Index), OutPaths);
				}
			}
			else if (const FClassProperty* ClassProperty = CastField<FClassProperty>(*It))
			{
				UClass* Named = Cast<UClass>(ClassProperty->GetObjectPropertyValue_InContainer(Defaults));
				if (Named != nullptr && Named->IsChildOf(AActor::StaticClass()))
				{
					GatherSoftObjectPaths(Named, Visited, OutPaths);
				}
			}
		}
	}
} // namespace

void AShooterGameMode::RequestGameplayAssets()
{
	TArray<UClass*> Classes;
	GetDerivedClasses(AShooterWeapon::StaticClass(), Classes, /*bRecursive=*/true);
	Classes.Add(DefaultPawnClass);
	Classes.Add(BombClass);
	Classes.Add(PlayerControllerClass); // the radio's sounds
	TArray<UClass*> Visited;
	TArray<FSoftObjectPath> Paths;
	for (UClass* Class : Classes)
	{
		GatherSoftObjectPaths(Class, Visited, Paths);
	}
	PreloadPaths.Reset();
	PreloadRequestIds.Reset();
	int32 NumRequested = 0;
	for (const FSoftObjectPath& Path : Paths)
	{
		NumRequested += RequestPreloadPath(Path) ? 1 : 0;
	}
	UE_LOG(LogShooter, Log, TEXT("Preloading %d package(s) for %d asset path(s)"), NumRequested, Paths.Num());
}
bool AShooterGameMode::RequestPreloadPath(const FSoftObjectPath& Path)
{
	const FString PackageName = Path.GetLongPackageName();
	const FName PackageFName(*PackageName);
	if (TArray<FSoftObjectPath>* Known = PreloadPaths.Find(PackageFName))
	{
		Known->AddUnique(Path);
		return false;
	}
	if (!FPackageName::DoesPackageExist(PackageName))
	{
		return false; // a name for art that is not there yet (N27): nothing to load
	}
	PreloadPaths.Add(PackageFName).Add(Path);
	PreloadRequestIds.Add(LoadPackageAsync(
		PackageName, FLoadPackageAsyncDelegate::CreateUObject(this, &AShooterGameMode::OnGameplayPackageLoaded)));
	return true;
}

void AShooterGameMode::OnGameplayPackageLoaded(
	const FName& PackageName, UPackage* /*Package*/, EAsyncLoadingResult::Type Result)
{
	const TArray<FSoftObjectPath>* Paths = PreloadPaths.Find(PackageName);
	if (Paths == nullptr || Result != EAsyncLoadingResult::Succeeded)
	{
		return;
	}
	for (const FSoftObjectPath& Path : *Paths)
	{
		if (UObject* Asset = Path.ResolveObject())
		{
			PreloadedAssets.AddUnique(Asset);
		}
	}
}

float AShooterGameMode::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0f;
}

AShooterGameState* AShooterGameMode::GetShooterGameState() const
{
	return GetGameState<AShooterGameState>();
}

// Teams and spawns

EShooterTeam AShooterGameMode::ChooseTeam(const FString& Options) const
{
	const EShooterTeam Requested = ParseShooterTeam(UGameplayStatics::ParseOption(Options, TEXT("team")));
	if (Requested != EShooterTeam::None)
	{
		return Requested;
	}
	return GetTeamSize(EShooterTeam::T) < GetTeamSize(EShooterTeam::CT) ? EShooterTeam::T : EShooterTeam::CT;
}

int32 AShooterGameMode::GetTeamSize(EShooterTeam Team) const
{
	int32 Size = 0;
	for (const APlayerState* State : GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
		if (ShooterState != nullptr && !ShooterState->IsPendingKillPending() && ShooterState->GetTeam() == Team)
		{
			++Size;
		}
	}
	return Size;
}

void AShooterGameMode::CountPawns(int32& OutCT, int32& OutT) const
{
	// Through the players' controllers, not the level's actors: a pawn spawned during a world tick joins the level's
	// list only when the tick ends.
	OutCT = 0;
	OutT = 0;
	for (const APlayerState* PlayerState : GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
		const AController* Controller = GetStateController(ShooterState);
		const AShooterCharacter* Character =
			Controller != nullptr ? Cast<AShooterCharacter>(Controller->GetPawn()) : nullptr;
		if (Character == nullptr || Character->IsPendingKillPending() || !Character->IsAlive())
		{
			continue;
		}
		OutCT += ShooterState->GetTeam() == EShooterTeam::CT ? 1 : 0;
		OutT += ShooterState->GetTeam() == EShooterTeam::T ? 1 : 0;
	}
}

int32 AShooterGameMode::CountAlive(EShooterTeam Team) const
{
	// Every bot asks for both teams every frame: they are counted once until the time moves on or a pawn changes.
	const float Now = GetWorldTime();
	if (Now != AliveCountTime || PawnsSerial != AliveCountSerial)
	{
		CountPawns(AliveCount[static_cast<int32>(EShooterTeam::CT)], AliveCount[static_cast<int32>(EShooterTeam::T)]);
		AliveCountTime = Now;
		AliveCountSerial = PawnsSerial;
	}
	return Team == EShooterTeam::None ? 0 : AliveCount[static_cast<int32>(Team)];
}

bool AShooterGameMode::ClaimSensingUpdate(UShooterPawnSensingComponent* Sensor)
{
	const float Now = GetWorldTime();
	if (Now != SensingFrameTime)
	{
		SensingFrameTime = Now;
		SensingUpdatesThisFrame = 0;
	}
	if (MaxSensingUpdatesPerFrame > 0)
	{
		// In turn: the frame's looks left go to the head of the queue, whichever of them asks first this frame.
		int32 Place = SensingQueue.Find(Sensor);
		if (Place == INDEX_NONE)
		{
			Place = SensingQueue.Add(Sensor);
		}
		if (Place >= MaxSensingUpdatesPerFrame - SensingUpdatesThisFrame)
		{
			return false;
		}
		// Keeps its capacity: the queue changes every frame.
		SensingQueue.RemoveAt(Place, 1, false);
	}
	++SensingUpdatesThisFrame;
	return true;
}

void AShooterGameMode::CancelSensingUpdate(UShooterPawnSensingComponent* Sensor)
{
	SensingQueue.Remove(Sensor);
}

int32 AShooterGameMode::GetSensingUpdatesThisFrame() const
{
	return GetWorldTime() == SensingFrameTime ? SensingUpdatesThisFrame : 0;
}

// Registries

void AShooterGameMode::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return;
	}
	// The map's volumes and starts are in the level already (a loaded map, or a test's spawns); later ones join as
	// they spawn.
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		RegisterMapActor(Actor);
	}
	ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
		UWorld::FOnActorSpawned::FDelegate::CreateUObject(this, &AShooterGameMode::OnActorSpawned));
}

void AShooterGameMode::OnActorSpawned(AActor* Actor)
{
	RegisterMapActor(Actor);
}

void AShooterGameMode::RegisterMapActor(AActor* Actor)
{
	if (ATriggerVolume* Zone = Cast<ATriggerVolume>(Actor))
	{
		if (!Zones.Contains(Zone))
		{
			Zones.Add(Zone);
			bMapCachesDirty = true;
		}
	}
	else if (APlayerStart* Start = Cast<APlayerStart>(Actor))
	{
		if (!PlayerStarts.Contains(Start))
		{
			PlayerStarts.Add(Start);
			bMapCachesDirty = true;
		}
	}
}

void AShooterGameMode::UpdateMapCaches() const
{
	// A volume or a start destroyed since (a test's; a map keeps them) is dropped from the caches too.
	for (const ATriggerVolume* Zone : BombSiteZones)
	{
		bMapCachesDirty |= Zone == nullptr || Zone->IsPendingKillPending();
	}
	for (const TArray<APlayerStart*>& Starts : TeamStarts)
	{
		for (const APlayerStart* Start : Starts)
		{
			bMapCachesDirty |= Start == nullptr || Start->IsPendingKillPending();
		}
	}
	if (!bMapCachesDirty)
	{
		return;
	}
	bMapCachesDirty = false;
	bLookoutsDirty = true;
	// The sites by name ("A", "B"), compared as text once here (not in every bot's frame).
	struct FSite
	{
		FString SortKey;
		FName Name;
		ATriggerVolume* Zone = nullptr;
	};
	TArray<FSite> Sites;
	for (ATriggerVolume* Zone : Zones)
	{
		if (Zone == nullptr || Zone->IsPendingKillPending() || !Zone->ActorHasTag(BombSiteTag))
		{
			continue;
		}
		const FName Name = GetZoneName(*Zone, BombSiteTag);
		if (Name != NAME_None && !Sites.ContainsByPredicate([Name](const FSite& Site) { return Site.Name == Name; }))
		{
			Sites.Add(FSite{Name.ToString(), Name, Zone});
		}
	}
	Sites.Sort([](const FSite& A, const FSite& B) { return A.SortKey < B.SortKey; });
	BombSiteNames.Reset();
	BombSiteLocations.Reset();
	BombSiteZones.Reset();
	for (const FSite& Site : Sites)
	{
		// The first volume with the site's tags in level order (GetBombSiteLocation's).
		const ATriggerVolume* Zone = nullptr;
		for (const ATriggerVolume* Candidate : Zones)
		{
			if (Candidate != nullptr && !Candidate->IsPendingKillPending() && Candidate->ActorHasTag(BombSiteTag) &&
				Candidate->ActorHasTag(Site.Name))
			{
				Zone = Candidate;
				break;
			}
		}
		const FBox Bounds = Zone != nullptr ? Zone->GetBrushBounds() : Site.Zone->GetBrushBounds();
		BombSiteNames.Add(Site.Name);
		BombSiteLocations.Add(FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z));
		BombSiteZones.Add(Site.Zone);
	}
	for (TArray<APlayerStart*>& Starts : TeamStarts)
	{
		Starts.Reset();
	}
	for (APlayerStart* Start : PlayerStarts)
	{
		if (Start == nullptr || Start->IsPendingKillPending())
		{
			continue;
		}
		for (const EShooterTeam Team : {EShooterTeam::CT, EShooterTeam::T})
		{
			if (Start->PlayerStartTag == GetShooterTeamTag(Team))
			{
				TeamStarts[static_cast<int32>(Team)].Add(Start);
			}
		}
	}
}

void AShooterGameMode::RegisterPawn(AShooterCharacter* Pawn)
{
	if (Pawn != nullptr && !Pawns.Contains(Pawn))
	{
		Pawns.Add(Pawn);
		NotifyPawnsChanged();
	}
}

void AShooterGameMode::UnregisterPawn(AShooterCharacter* Pawn)
{
	RemoveFromRegistry(Pawns, Pawn);
	NotifyPawnsChanged();
}

void AShooterGameMode::RegisterPickup(AActor* Pickup)
{
	if (Pickup != nullptr && !Pickups.Contains(Pickup))
	{
		Pickups.Add(Pickup);
	}
}

void AShooterGameMode::UnregisterPickup(AActor* Pickup)
{
	RemoveFromRegistry(Pickups, Pickup);
}

void AShooterGameMode::RegisterBomb(AShooterBomb* InBomb)
{
	if (InBomb != nullptr && !Bombs.Contains(InBomb))
	{
		Bombs.Add(InBomb);
	}
}

void AShooterGameMode::UnregisterBomb(AShooterBomb* InBomb)
{
	RemoveFromRegistry(Bombs, InBomb);
}

void AShooterGameMode::RegisterProjectile(AShooterProjectile* Projectile)
{
	if (Projectile != nullptr && !Projectiles.Contains(Projectile))
	{
		Projectiles.Add(Projectile);
	}
}

void AShooterGameMode::UnregisterProjectile(AShooterProjectile* Projectile)
{
	RemoveFromRegistry(Projectiles, Projectile);
}

void AShooterGameMode::RegisterSmokeCloud(AShooterSmokeCloud* Cloud)
{
	if (Cloud != nullptr && !SmokeClouds.Contains(Cloud))
	{
		SmokeClouds.Add(Cloud);
	}
}

void AShooterGameMode::UnregisterSmokeCloud(AShooterSmokeCloud* Cloud)
{
	RemoveFromRegistry(SmokeClouds, Cloud);
}

bool AShooterGameMode::IsSightBlockedBySmoke(const FVector& Start, const FVector& End) const
{
	for (const AShooterSmokeCloud* Cloud : SmokeClouds)
	{
		if (Cloud != nullptr && Cloud->BlocksLine(Start, End))
		{
			return true;
		}
	}
	return false;
}

FString AShooterGameMode::InitNewPlayer(
	APlayerController* NewPlayerController, const FString& Options, const FString& Portal)
{
	// The team first: the start spot chosen below depends on it (UE ShooterGame: ChooseTeam in PostLogin).
	if (AShooterPlayerState* State =
			NewPlayerController != nullptr ? NewPlayerController->GetPlayerState<AShooterPlayerState>() : nullptr)
	{
		// A bot match's player only watches (no team: RestartPlayer makes it a spectator).
		State->SetTeam(bBotMatch ? EShooterTeam::None : ChooseTeam(Options));
		State->SetMoney(StartMoney, MaxMoney);
	}
	return Super::InitNewPlayer(NewPlayerController, Options, Portal);
}

AActor* AShooterGameMode::ChoosePlayerStart(AController* Player)
{
	UWorld* World = GetWorld();
	const AShooterPlayerState* State = Player != nullptr ? Player->GetPlayerState<AShooterPlayerState>() : nullptr;
	const FName TeamTag = State != nullptr ? GetShooterTeamTag(State->GetTeam()) : NAME_None;
	if (World == nullptr || World->PersistentLevel == nullptr || TeamTag == NAME_None)
	{
		return Super::ChoosePlayerStart(Player);
	}

	APlayerStart* FirstTeamStart = nullptr;
	for (APlayerStart* Start : GetTeamStarts(State->GetTeam()))
	{
		if (FirstTeamStart == nullptr)
		{
			FirstTeamStart = Start;
		}
		if (!IsStartOccupied(Pawns, *Start))
		{
			return Start;
		}
	}
	if (FirstTeamStart != nullptr)
	{
		UE_LOG(LogShooter, Warning, TEXT("ChoosePlayerStart: every %s start is taken; reusing %s"), *TeamTag.ToString(),
			*FirstTeamStart->GetName());
		return FirstTeamStart;
	}
	UE_LOG(LogShooter, Warning, TEXT("ChoosePlayerStart: the map has no %s start"), *TeamTag.ToString());
	return Super::ChoosePlayerStart(Player);
}

const TArray<APlayerStart*>& AShooterGameMode::GetTeamStarts(EShooterTeam Team) const
{
	UpdateMapCaches();
	return TeamStarts[static_cast<int32>(Team)];
}

bool AShooterGameMode::GetTeamSpawnLocation(EShooterTeam Team, FVector& OutLocation) const
{
	const TArray<APlayerStart*>& Starts = GetTeamStarts(Team);
	if (Starts.Num() == 0)
	{
		return false;
	}
	OutLocation = Starts[0]->GetActorLocation();
	return true;
}

const TArray<FShooterLookout>& AShooterGameMode::GetBombSiteLookouts(FName Site) const
{
	UpdateMapCaches();
	const UWorld* World = GetWorld();
	const int32 NumNodes = World != nullptr ? World->GetNavigationSystem().GetNodes().Num() : 0;
	if (bLookoutsDirty || NumNodes != LookoutsNodeCount)
	{
		BuildBombSiteLookouts();
	}
	static const TArray<FShooterLookout> NoLookouts;
	const int32 Index = BombSiteNames.IndexOfByKey(Site);
	return BombSiteLookouts.IsValidIndex(Index) ? BombSiteLookouts[Index] : NoLookouts;
}

void AShooterGameMode::BuildBombSiteLookouts() const
{
	bLookoutsDirty = false;
	BombSiteLookouts.Reset();
	BombSiteLookouts.SetNum(BombSiteNames.Num());
	const UWorld* World = GetWorld();
	const TArray<UNavigationSystem::FNode> NoNodes;
	const TArray<UNavigationSystem::FNode>& Nodes =
		World != nullptr ? World->GetNavigationSystem().GetNodes() : NoNodes;
	LookoutsNodeCount = Nodes.Num();
	if (BombSiteNames.Num() == 0)
	{
		return;
	}
	auto AddYaw = [](TArray<float, TInlineAllocator<4>>& Yaws, float Yaw)
	{
		for (const float Watched : Yaws)
		{
			if (FMath::Abs(FRotator::NormalizeAxis(Yaw - Watched)) < LookoutWatchSpacing)
			{
				return;
			}
		}
		if (Yaws.Num() < MaxLookoutWatchYaws)
		{
			Yaws.Add(FRotator::NormalizeAxis(Yaw));
		}
	};
	auto YawTo = [](const FVector& From, const FVector& To) { return (To - From).Rotation().Yaw; };
	// Each team's watcher looks toward the other team's spawn (the way it comes).
	FVector Spawns[3] = {FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector};
	bool bSpawns[3] = {false, false, false};
	bSpawns[static_cast<int32>(EShooterTeam::CT)] =
		GetTeamSpawnLocation(EShooterTeam::T, Spawns[static_cast<int32>(EShooterTeam::CT)]);
	bSpawns[static_cast<int32>(EShooterTeam::T)] =
		GetTeamSpawnLocation(EShooterTeam::CT, Spawns[static_cast<int32>(EShooterTeam::T)]);
	constexpr EShooterTeam Watchers[] = {EShooterTeam::CT, EShooterTeam::T};
	int32 SpawnNodes[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
	for (const EShooterTeam Team : Watchers)
	{
		const int32 TeamIndex = static_cast<int32>(Team);
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num() && bSpawns[TeamIndex]; ++NodeIndex)
		{
			if (SpawnNodes[TeamIndex] == INDEX_NONE ||
				FVector::DistSquared2D(Nodes[NodeIndex].Location, Spawns[TeamIndex]) <
					FVector::DistSquared2D(Nodes[SpawnNodes[TeamIndex]].Location, Spawns[TeamIndex]))
			{
				SpawnNodes[TeamIndex] = NodeIndex;
			}
		}
	}
	auto MakeLookout = [&](int32 NodeIndex, const FVector& SiteLocation)
	{
		FShooterLookout Lookout;
		const UNavigationSystem::FNode& Node = Nodes[NodeIndex];
		Lookout.Location = Node.Location;
		for (const EShooterTeam Team : Watchers)
		{
			const int32 TeamIndex = static_cast<int32>(Team);
			TArray<float, TInlineAllocator<4>>& Yaws = Lookout.WatchYaws[TeamIndex];
			// The main way in: the path's first link toward the other team's spawn (CS's bots' approach areas).
			TArray<int32> Approach;
			if (SpawnNodes[TeamIndex] != INDEX_NONE &&
				UNavigationSystem::FindNodePath(Nodes, NodeIndex, SpawnNodes[TeamIndex], Approach) &&
				Approach.Num() >= 2)
			{
				AddYaw(Yaws, YawTo(Node.Location, Nodes[Approach[1]].Location));
			}
			// Away from the site: over it; then the links toward the other team's spawn, else any link.
			if (FVector::Dist2D(Node.Location, SiteLocation) > LookoutOverSiteDistance)
			{
				AddYaw(Yaws, YawTo(Node.Location, SiteLocation));
			}
			const float ToEnemy = FVector::Dist2D(Node.Location, Spawns[TeamIndex]);
			for (const int32 Link : Node.Links)
			{
				if (bSpawns[TeamIndex] && Nodes.IsValidIndex(Link) &&
					FVector::Dist2D(Nodes[Link].Location, Spawns[TeamIndex]) < ToEnemy)
				{
					AddYaw(Yaws, YawTo(Node.Location, Nodes[Link].Location));
				}
			}
			for (const int32 Link : Node.Links)
			{
				if (Yaws.Num() == 0 && Nodes.IsValidIndex(Link))
				{
					AddYaw(Yaws, YawTo(Node.Location, Nodes[Link].Location));
				}
			}
		}
		return Lookout;
	};
	// The flagged waypoints, each to its nearest site (ties: the first by name).
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		if (!Nodes[NodeIndex].Flags.Contains(LookoutFlag))
		{
			continue;
		}
		int32 Nearest = 0;
		for (int32 SiteIndex = 1; SiteIndex < BombSiteLocations.Num(); ++SiteIndex)
		{
			if (FVector::DistSquared2D(Nodes[NodeIndex].Location, BombSiteLocations[SiteIndex]) <
				FVector::DistSquared2D(Nodes[NodeIndex].Location, BombSiteLocations[Nearest]))
			{
				Nearest = SiteIndex;
			}
		}
		BombSiteLookouts[Nearest].Add(MakeLookout(NodeIndex, BombSiteLocations[Nearest]));
	}
	for (int32 SiteIndex = 0; SiteIndex < BombSiteNames.Num(); ++SiteIndex)
	{
		TArray<FShooterLookout>& Lookouts = BombSiteLookouts[SiteIndex];
		const FVector& SiteLocation = BombSiteLocations[SiteIndex];
		if (Lookouts.Num() > 0)
		{
			continue;
		}
		// A map without lookouts: the site's nearest waypoints (ties: the lower index).
		TArray<int32> Near;
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			if (FVector::DistSquared2D(Nodes[NodeIndex].Location, SiteLocation) <= FMath::Square(LookoutFallbackRadius))
			{
				Near.Add(NodeIndex);
			}
		}
		Near.Sort(
			[&Nodes, &SiteLocation](int32 A, int32 B)
			{
				const float DistA = FVector::DistSquared2D(Nodes[A].Location, SiteLocation);
				const float DistB = FVector::DistSquared2D(Nodes[B].Location, SiteLocation);
				return DistA < DistB || (DistA == DistB && A < B);
			});
		for (int32 Index = 0; Index < FMath::Min(Near.Num(), MaxFallbackLookouts); ++Index)
		{
			Lookouts.Add(MakeLookout(Near[Index], SiteLocation));
		}
		if (Lookouts.Num() > 0)
		{
			continue;
		}
		// No waypoints near: the site's middle, watching around from the other team's side.
		FShooterLookout& Middle = Lookouts.AddDefaulted_GetRef();
		Middle.Location = SiteLocation;
		for (const EShooterTeam Team : Watchers)
		{
			const int32 TeamIndex = static_cast<int32>(Team);
			const float Base = bSpawns[TeamIndex] ? YawTo(SiteLocation, Spawns[TeamIndex]) : 0.0f;
			for (int32 Quarter = 0; Quarter < MaxLookoutWatchYaws; ++Quarter)
			{
				AddYaw(Middle.WatchYaws[TeamIndex], Base + (90.0f * static_cast<float>(Quarter)));
			}
		}
	}
}

APawn* AShooterGameMode::SpawnDefaultPawnFor(AController* NewPlayer, AActor* StartSpot)
{
	if (StartSpot == nullptr)
	{
		return Super::SpawnDefaultPawnFor(NewPlayer, StartSpot);
	}
	// The start's location is its capsule's centre (UE); the character stands on its feet (Leon).
	const FVector Feet = StartSpot->GetActorLocation() - FVector(0.0f, 0.0f, GetStartHalfHeight(*StartSpot));
	const FRotator StartRotation(0.0f, StartSpot->GetActorRotation().Yaw, 0.0f);
	return SpawnDefaultPawnAtTransform(NewPlayer, FTransform(StartRotation, Feet));
}

bool AShooterGameMode::IsRoundLive() const
{
	const AShooterGameState* State = GetShooterGameState();
	return State != nullptr &&
		(State->GetRoundState() == EShooterRoundState::Live || State->GetRoundState() == EShooterRoundState::RoundEnd);
}

void AShooterGameMode::RestartPlayer(AController* NewPlayer)
{
	// CS: a player who joins while a round is being fought waits for the next one; one without a team watches.
	const AShooterPlayerState* JoiningState =
		NewPlayer != nullptr ? NewPlayer->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (IsRoundLive() || (JoiningState != nullptr && JoiningState->GetTeam() == EShooterTeam::None))
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(NewPlayer))
		{
			PlayerController->ChangeState(NAME_Spectating);
		}
		return;
	}
	Super::RestartPlayer(NewPlayer);
	const APawn* Pawn = NewPlayer != nullptr ? NewPlayer->GetPawn() : nullptr;
	const AShooterPlayerState* State =
		NewPlayer != nullptr ? NewPlayer->GetPlayerState<AShooterPlayerState>() : nullptr;
	if (Pawn == nullptr || State == nullptr)
	{
		return;
	}
	int32 NumCT = 0;
	int32 NumT = 0;
	CountPawns(NumCT, NumT);
	const FVector Location = Pawn->GetActorLocation();
	UE_LOG(LogShooter, Log, TEXT("%s '%s' joined %s at (%.0f, %.0f, %.0f): %d pawn(s), CT %d, T %d"),
		State->bIsABot ? TEXT("Bot") : TEXT("Player"), *State->GetPlayerName(), GetShooterTeamName(State->GetTeam()),
		static_cast<double>(Location.X), static_cast<double>(Location.Y), static_cast<double>(Location.Z), NumCT + NumT,
		NumCT, NumT);
}

// Bots

int32 AShooterGameMode::AddBots(EShooterTeam Team, int32 Count)
{
	UWorld* World = GetWorld();
	if (World == nullptr || BotControllerClass == nullptr)
	{
		return 0;
	}
	int32 Added = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const EShooterTeam BotTeam = Team != EShooterTeam::None ? Team : ChooseTeam(FString());
		if (GetTeamSize(BotTeam) >= MaxPlayersPerTeam)
		{
			UE_LOG(LogShooter, Warning, TEXT("AddBots: team %s is full (%d players)"), GetShooterTeamName(BotTeam),
				MaxPlayersPerTeam);
			continue;
		}

		// UE ShooterGame: CreateBotControllers / InitBot, then RestartPlayer. The controller spawns the player state.
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		AShooterAIController* BotController = World->SpawnActor<AShooterAIController>(
			BotControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
		AShooterPlayerState* State =
			BotController != nullptr ? BotController->GetPlayerState<AShooterPlayerState>() : nullptr;
		if (State == nullptr)
		{
			UE_LOG(LogShooter, Error, TEXT("AddBots: the bot controller has no ShooterPlayerState"));
			continue;
		}
		const int32 TeamIndex = static_cast<int32>(BotTeam);
		// The teams' looks interleaved over the interval: CT 0, T 1, CT 2, ...
		BotController->SetSensingSlot(
			(2 * NumBotsAddedToTeam[TeamIndex]++) + (BotTeam == EShooterTeam::T ? 1 : 0), 2 * MaxPlayersPerTeam);
		// The bot's index seeds its stream (its name does not: a name never changes the match) and picks its name.
		const int32 BotIndex = NumBotsCreated++;
		BotController->SetBotIndex(BotIndex);
		State->SetTeam(BotTeam);
		State->bIsABot = true;
		State->SetMoney(StartMoney, MaxMoney);
		State->SetPlayerName(GetBotName(BotIndex));
		GetGameState().AddPlayerState(State);
		RestartPlayer(BotController);
		++Added;
	}
	return Added;
}

FString AShooterGameMode::GetBotName(int32 BotIndex) const
{
	const int32 Index = FMath::Max(0, BotIndex);
	if (BotNames.Num() == 0)
	{
		return FString::Printf(TEXT("Bot %d"), Index + 1);
	}
	const FString& Name = BotNames[Index % BotNames.Num()];
	const int32 Round = Index / BotNames.Num();
	return Round == 0 ? Name : FString::Printf(TEXT("%s (%d)"), *Name, Round + 1);
}

int32 AShooterGameMode::FillTeamsWithBots()
{
	const int32 AddedCT = AddBots(EShooterTeam::CT, FMath::Max(0, MaxPlayersPerTeam - GetTeamSize(EShooterTeam::CT)));
	const int32 AddedT = AddBots(EShooterTeam::T, FMath::Max(0, MaxPlayersPerTeam - GetTeamSize(EShooterTeam::T)));
	return AddedCT + AddedT;
}

int32 AShooterGameMode::KickBots(const FString& Name)
{
	const bool bAll = Name.IsEmpty() || Name.Equals(TEXT("all"), ESearchCase::IgnoreCase);
	TArray<AShooterAIController*> Kicked;
	for (const APlayerState* State : GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
		AShooterAIController* Bot = Cast<AShooterAIController>(GetStateController(ShooterState));
		if (Bot != nullptr && ShooterState->bIsABot &&
			(bAll || ShooterState->GetPlayerName().Equals(Name, ESearchCase::IgnoreCase)))
		{
			Kicked.Add(Bot);
		}
	}
	for (AShooterAIController* Bot : Kicked)
	{
		if (APawn* Pawn = Bot->GetPawn())
		{
			if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(Pawn))
			{
				Shooter->Suicide();
			}
			(void)Pawn->Destroy();
		}
		Logout(Bot);
		(void)Bot->Destroy();
	}
	return Kicked.Num();
}

// Damage and kills

bool AShooterGameMode::CanDealDamage(AController* DamageInstigator, AController* Victim) const
{
	if (DamageInstigator == nullptr || Victim == nullptr || DamageInstigator == Victim || bFriendlyFire)
	{
		return true;
	}
	const AShooterPlayerState* InstigatorState = DamageInstigator->GetPlayerState<AShooterPlayerState>();
	const AShooterPlayerState* VictimState = Victim->GetPlayerState<AShooterPlayerState>();
	return InstigatorState == nullptr || VictimState == nullptr || InstigatorState->GetTeam() == EShooterTeam::None ||
		InstigatorState->GetTeam() != VictimState->GetTeam();
}

void AShooterGameMode::Killed(
	AController* Killer, AController* Victim, APawn* VictimPawn, AActor* DamageCauser, bool bHeadshot)
{
	++NumKills;
	AShooterPlayerState* KillerState = Killer != nullptr ? Killer->GetPlayerState<AShooterPlayerState>() : nullptr;
	AShooterPlayerState* VictimState = Victim != nullptr ? Victim->GetPlayerState<AShooterPlayerState>() : nullptr;
	const FKillCredit Credit = GetKillCredit(DamageCauser);
	const FString& WeaponName = Credit.WeaponName;
	const FString VictimName = GetDisplayName(Victim, VictimPawn);
	const FString KillerName = GetDisplayName(Killer, nullptr);
	const bool bSuicide = Killer == nullptr || Killer == Victim;

	if (VictimState != nullptr)
	{
		VictimState->ScoreDeath();
	}
	if (!bSuicide && KillerState != nullptr)
	{
		const bool bTeamKill = VictimState != nullptr && KillerState->GetTeam() == VictimState->GetTeam();
		if (bTeamKill)
		{
			KillerState->ScoreKill(-1);
			(void)KillerState->AddMoney(-TeamKillPenalty, MaxMoney);
		}
		else
		{
			KillerState->ScoreKill(1);
			(void)KillerState->AddMoney(Credit.Reward, MaxMoney);
		}
	}

	if (AShooterGameState* State = GetShooterGameState())
	{
		FShooterKillFeedEntry Entry;
		Entry.KillerName = bSuicide ? FString() : KillerName;
		Entry.VictimName = VictimName;
		Entry.WeaponName = WeaponName;
		Entry.KillerTeam = KillerState != nullptr ? KillerState->GetTeam() : EShooterTeam::None;
		Entry.VictimTeam = VictimState != nullptr ? VictimState->GetTeam() : EShooterTeam::None;
		Entry.bHeadshot = bHeadshot;
		Entry.Time = GetWorldTime();
		State->AddKillFeedEntry(Entry);
	}
	if (bSuicide)
	{
		UE_LOG(LogShooter, Log, TEXT("Kill: %s died (%s)"), *VictimName, *WeaponName);
	}
	else
	{
		UE_LOG(LogShooter, Log, TEXT("Kill: %s killed %s with %s%s"), *KillerName, *VictimName, *WeaponName,
			bHeadshot ? TEXT(" (headshot)") : TEXT(""));
	}
	// The round's end waits for the game mode's next tick, so deaths of the same moment (an explosion) count together.
}

// The match and the rounds

bool AShooterGameMode::ReadyToStartMatch() const
{
	return GetTeamSize(EShooterTeam::CT) > 0 && GetTeamSize(EShooterTeam::T) > 0;
}

void AShooterGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	BeginNewMatch();
}

void AShooterGameMode::BeginNewMatch()
{
	RoundRandom.Initialize(RandomSeed);
	LossStreak[0] = LossStreak[1] = LossStreak[2] = 0;
	GetWorldTimerManager().ClearTimer(TimerHandle_RestartGame);
	if (AShooterGameState* State = GetShooterGameState())
	{
		State->ResetMatch();
	}
	for (APlayerState* PlayerState : GetGameState().GetPlayerArray())
	{
		if (AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState))
		{
			ShooterState->SetMoney(StartMoney, MaxMoney);
			ShooterState->ResetStats();
		}
	}
	UE_LOG(LogShooter, Display, TEXT("Match started: CT %d, T %d, %d rounds, seed %d"), GetTeamSize(EShooterTeam::CT),
		GetTeamSize(EShooterTeam::T), MaxRounds, RandomSeed);
	// A new match starts clean (CS's "Game Commencing"): every pawn goes, and the round respawns everyone with the
	// default inventory.
	DestroyAllPawns();
	StartRound();
}

void AShooterGameMode::DestroyAllPawns()
{
	// A copy: each pawn leaves the registry as it goes.
	const TArray<AShooterCharacter*> MatchPawns = Pawns;
	for (AShooterCharacter* Shooter : MatchPawns)
	{
		if (AController* Controller = Shooter->GetController())
		{
			Controller->UnPossess();
		}
		(void)Shooter->Destroy();
	}
}

void AShooterGameMode::HandleHalftime()
{
	AShooterGameState* State = GetShooterGameState();
	if (State == nullptr)
	{
		return;
	}
	State->BeginSecondHalf();
	LossStreak[0] = LossStreak[1] = LossStreak[2] = 0;
	for (APlayerState* PlayerState : GetGameState().GetPlayerArray())
	{
		AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
		if (ShooterState == nullptr || ShooterState->GetTeam() == EShooterTeam::None)
		{
			continue; // a spectator (the bot match's player) stays one
		}
		ShooterState->SetTeam(GetOpposingTeam(ShooterState->GetTeam()));
		ShooterState->SetMoney(StartMoney, MaxMoney);
	}
	// The second half starts as the first: everyone on the new side's starts with the default inventory.
	DestroyAllPawns();
	UE_LOG(LogShooter, Display, TEXT("Halftime after round %d: the teams switch sides, CT %d - T %d"),
		State->GetRoundNumber(), State->GetTeamScore(EShooterTeam::CT), State->GetTeamScore(EShooterTeam::T));
}

void AShooterGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
	AShooterGameState* State = GetShooterGameState();
	if (State == nullptr)
	{
		return;
	}
	const int32 ScoreCT = State->GetTeamScore(EShooterTeam::CT);
	const int32 ScoreT = State->GetTeamScore(EShooterTeam::T);
	State->SetMatchWinner(ScoreCT > ScoreT ? EShooterTeam::CT
			: ScoreT > ScoreCT             ? EShooterTeam::T
										   : EShooterTeam::None);
	State->SetRoundState(EShooterRoundState::MatchEnd, 0.0f);
	GetWorldTimerManager().ClearTimer(TimerHandle_Phase);
	GetWorldTimerManager().ClearTimer(TimerHandle_BuyTime);
	UE_LOG(LogShooter, Display, TEXT("Match over after %d round(s): CT %d - T %d, %s"), State->GetRoundNumber(),
		ScoreCT, ScoreT,
		State->GetMatchWinner() == EShooterTeam::None     ? TEXT("a draw")
			: State->GetMatchWinner() == EShooterTeam::CT ? TEXT("the Counter-Terrorists win")
														  : TEXT("the Terrorists win"));
}

void AShooterGameMode::CleanUpMap()
{
	// From the registries (each actor leaves its registry as it is destroyed, so they are copied first).
	TArray<AActor*> ToDestroy;
	for (AActor* Pickup : Pickups)
	{
		const AShooterWeapon* Weapon = Cast<AShooterWeapon>(Pickup);
		if (Weapon != nullptr && Weapon->IsDropped())
		{
			ToDestroy.AddUnique(Pickup);
		}
	}
	for (AShooterProjectile* Projectile : Projectiles)
	{
		ToDestroy.AddUnique(Projectile);
	}
	for (AShooterSmokeCloud* Cloud : SmokeClouds)
	{
		ToDestroy.AddUnique(Cloud);
	}
	for (AShooterBomb* RoundBomb : Bombs)
	{
		ToDestroy.AddUnique(RoundBomb);
	}
	for (AShooterCharacter* Shooter : Pawns)
	{
		if (!Shooter->IsAlive())
		{
			ToDestroy.AddUnique(Shooter);
		}
	}
	for (AActor* Actor : ToDestroy)
	{
		(void)Actor->Destroy();
	}
	Bomb = nullptr;
}

void AShooterGameMode::StartRound()
{
	AShooterGameState* State = GetShooterGameState();
	UWorld* World = GetWorld();
	if (State == nullptr || World == nullptr)
	{
		return;
	}
	CleanUpMap();
	// The last round's weapons, grenades, corpses and bomb go in a full collection at the next safe point (the steps'
	// incremental ones collect in between).
	if (GEngine != nullptr)
	{
		GEngine->ForceGarbageCollection(true);
	}
	bBombPlantedThisRound = false;
	const float Now = GetWorldTime();
	State->SetRoundNumber(State->GetRoundNumber() + 1);
	// Freeze first: RestartPlayer spawns during the freeze.
	State->SetRoundState(EShooterRoundState::Freeze, Now + FreezeTime);
	SetPhaseTimer(FreezeTime);
	// CS: mp_buytime counts from the freeze's end, so the whole freeze and BuyTime of the live round are for buying
	// (the Live transition sets it again from the moment the freeze really ends).
	State->SetBuyEndTime(Now + FreezeTime + BuyTime);
	GetWorldTimerManager().SetTimer(TimerHandle_BuyTime, FreezeTime + BuyTime, false);
	State->SetBombState(EShooterBombState::None);

	// Each team on its starts, in the order its players joined; the survivors keep their weapons.
	TArray<AShooterCharacter*> Terrorists;
	for (const EShooterTeam Team : {EShooterTeam::CT, EShooterTeam::T})
	{
		const TArray<APlayerStart*> Starts = GetTeamStarts(Team);
		int32 StartIndex = 0;
		for (APlayerState* PlayerState : GetGameState().GetPlayerArray())
		{
			const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
			AController* Controller = GetStateController(ShooterState);
			if (ShooterState == nullptr || ShooterState->GetTeam() != Team || Controller == nullptr ||
				Controller->IsPendingKillPending())
			{
				continue;
			}
			AActor* Start = Starts.Num() > 0 ? Starts[StartIndex++ % Starts.Num()] : FindPlayerStart(Controller);
			AShooterCharacter* Pawn = Cast<AShooterCharacter>(Controller->GetPawn());
			if (Pawn != nullptr && Pawn->IsAlive() && Start != nullptr)
			{
				const FVector Feet = Start->GetActorLocation() - FVector(0.0f, 0.0f, GetStartHalfHeight(*Start));
				Pawn->ResetForNewRound(Feet, Start->GetActorRotation().Yaw);
			}
			else
			{
				RestartPlayerAtPlayerStart(Controller, Start);
				Pawn = Cast<AShooterCharacter>(Controller->GetPawn());
			}
			if (Pawn != nullptr && Team == EShooterTeam::T)
			{
				Terrorists.Add(Pawn);
			}
		}
	}

	// The bomb to a random terrorist.
	if (Terrorists.Num() > 0 && BombClass != nullptr)
	{
		AShooterCharacter* Carrier = Terrorists[RoundRandom.RandRange(0, Terrorists.Num() - 1)];
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		Bomb =
			World->SpawnActor<AShooterBomb>(BombClass, Carrier->GetActorLocation(), FRotator::ZeroRotator, SpawnInfo);
		if (Bomb != nullptr)
		{
			Bomb->GiveTo(Carrier);
		}
	}
	// The terrorists' plan: one of the sites, from the round stream.
	const TArray<FName>& Sites = GetBombSiteNames();
	TerroristTargetSite = Sites.Num() > 0 ? Sites[RoundRandom.RandRange(0, Sites.Num() - 1)] : NAME_None;
	// Each team's purchases for the round, from what its players have now (CS's economy).
	DecideTeamBuyPlans();
	int32 NumCT = 0;
	int32 NumT = 0;
	CountPawns(NumCT, NumT);
	UE_LOG(LogShooter, Display, TEXT("Round %d: CT %d vs T %d, the bomb with %s"), State->GetRoundNumber(), NumCT, NumT,
		Bomb != nullptr && Bomb->GetCarrier() != nullptr ? *GetDisplayName(Bomb->GetCarrier()->GetController(), nullptr)
														 : TEXT("nobody"));
}

void AShooterGameMode::EndRound(EShooterRoundEndReason Reason)
{
	AShooterGameState* State = GetShooterGameState();
	if (State == nullptr || State->GetRoundState() == EShooterRoundState::RoundEnd ||
		State->GetRoundState() == EShooterRoundState::MatchEnd)
	{
		return;
	}
	const EShooterTeam Winner = GetRoundEndWinner(Reason);
	const EShooterTeam Loser = GetOpposingTeam(Winner);
	State->SetRoundState(EShooterRoundState::RoundEnd, GetWorldTime() + RoundRestartDelay);
	SetPhaseTimer(RoundRestartDelay);
	State->SetLastRoundEndReason(Reason);
	if (Winner != EShooterTeam::None)
	{
		State->AddTeamScore(Winner);
		const bool bObjective =
			Reason == EShooterRoundEndReason::TargetBombed || Reason == EShooterRoundEndReason::BombDefused;
		PayTeam(Winner, bObjective ? BombWinReward : WinReward);
		LossStreak[static_cast<int32>(Winner)] = 0;
		// The loss bonus grows with the streak: the first loss pays the base.
		const int32 LoserIndex = static_cast<int32>(Loser);
		LossStreak[LoserIndex] = LossStreak[LoserIndex] + 1;
		int32 LoserPay = FMath::Min(LossBonusBase + ((LossStreak[LoserIndex] - 1) * LossBonusIncrement), LossBonusMax);
		if (Loser == EShooterTeam::T && bBombPlantedThisRound)
		{
			LoserPay += LosingTeamPlantBonus;
		}
		PayTeam(Loser, LoserPay);
	}
	UE_LOG(LogShooter, Display, TEXT("Round %d over: %s CT %d - T %d"), State->GetRoundNumber(),
		GetRoundEndMessage(Reason), State->GetTeamScore(EShooterTeam::CT), State->GetTeamScore(EShooterTeam::T));
}

int32 AShooterGameMode::GetLossStreak(EShooterTeam Team) const
{
	return LossStreak[static_cast<int32>(Team)];
}

int32 AShooterGameMode::GetLossBonus(EShooterTeam Team) const
{
	return FMath::Min(LossBonusBase + (GetLossStreak(Team) * LossBonusIncrement), LossBonusMax);
}

EShooterBuyPlan AShooterGameMode::ChooseBuyPlan(bool bPistolRound, bool bLastRoundOfHalf, int32 LossStreak,
	int32 NumEquipped, int32 TeamSize, int32 InForceBuyLossStreak)
{
	if (bPistolRound)
	{
		return EShooterBuyPlan::Pistol;
	}
	// Half of the team or more can have the rifle and armor: all buy.
	if (TeamSize > 0 && NumEquipped * 2 >= TeamSize)
	{
		return EShooterBuyPlan::Full;
	}
	// Short of it: spend anyway at the half's end, after a win, or when the losses pile up; else save for the next.
	const bool bLongLossStreak = InForceBuyLossStreak > 0 && LossStreak >= InForceBuyLossStreak;
	return bLastRoundOfHalf || LossStreak == 0 || bLongLossStreak ? EShooterBuyPlan::Force : EShooterBuyPlan::Eco;
}

int32 AShooterGameMode::GetFullBuyCost(const AShooterCharacter& Buyer) const
{
	const UClass* RifleClass =
		AShooterWeapon::FindWeaponClass(Buyer.GetTeam() == EShooterTeam::T ? TEXT("ak47") : TEXT("m4a1"));
	const int32 RiflePrice = RifleClass != nullptr ? RifleClass->GetDefaultObject<AShooterWeapon>()->Price : 0;
	return RiflePrice + VestHelmetPrice;
}

bool AShooterGameMode::IsEquippedOrCanFullBuy(const AShooterCharacter& Buyer, int32 Money) const
{
	return Buyer.GetWeaponInSlot(EShooterWeaponSlot::Primary) != nullptr || Money >= GetFullBuyCost(Buyer);
}

void AShooterGameMode::DecideTeamBuyPlans()
{
	const AShooterGameState* State = GetShooterGameState();
	if (State == nullptr)
	{
		return;
	}
	// The first round of each half is the pistol round; the last of each half forces the buy.
	const int32 Round = State->GetRoundNumber();
	const bool bPistolRound = Round == 1 || (State->IsSecondHalf() && Round == State->GetHalftimeRound() + 1);
	const int32 Halftime = GetHalftimeRound();
	const bool bLastRoundOfHalf = Round == MaxRounds || (Halftime > 0 && !State->IsSecondHalf() && Round == Halftime);
	for (const EShooterTeam Team : {EShooterTeam::CT, EShooterTeam::T})
	{
		int32 TeamSize = 0;
		int32 NumEquipped = 0;
		for (APlayerState* PlayerState : GetGameState().GetPlayerArray())
		{
			const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
			const AController* Controller = GetStateController(ShooterState);
			const AShooterCharacter* Pawn =
				Controller != nullptr ? Cast<AShooterCharacter>(Controller->GetPawn()) : nullptr;
			if (ShooterState == nullptr || ShooterState->GetTeam() != Team || Pawn == nullptr || !Pawn->IsAlive())
			{
				continue;
			}
			++TeamSize;
			NumEquipped += IsEquippedOrCanFullBuy(*Pawn, ShooterState->GetMoney()) ? 1 : 0;
		}
		const int32 Index = static_cast<int32>(Team);
		TeamBuyPlan[Index] =
			ChooseBuyPlan(bPistolRound, bLastRoundOfHalf, LossStreak[Index], NumEquipped, TeamSize, ForceBuyLossStreak);
	}
	UE_LOG(LogShooter, Log, TEXT("Round %d buys: CT %s, T %s"), Round, GetBuyPlanName(GetTeamBuyPlan(EShooterTeam::CT)),
		GetBuyPlanName(GetTeamBuyPlan(EShooterTeam::T)));
}

bool AShooterGameMode::SendRadioMessage(AController* Sender, EShooterRadioMessage Message, const FVector* Location)
{
	AShooterGameState* State = GetShooterGameState();
	AShooterPlayerState* SenderState = Sender != nullptr ? Sender->GetPlayerState<AShooterPlayerState>() : nullptr;
	const AShooterCharacter* SenderPawn = Sender != nullptr ? Cast<AShooterCharacter>(Sender->GetPawn()) : nullptr;
	if (State == nullptr || SenderState == nullptr || SenderPawn == nullptr || !SenderPawn->IsAlive() ||
		Message == EShooterRadioMessage::None || SenderState->GetTeam() == EShooterTeam::None)
	{
		return false;
	}
	// CS 1.6: a message every 1.5 s and 60 a round; the grenade's call and the bomb's news go out regardless.
	const float Now = GetWorldTime();
	if (SenderState->RadioRoundSerial != State->GetRoundSerial())
	{
		SenderState->RadioRoundSerial = State->GetRoundSerial();
		SenderState->RadioMessagesInRound = 0;
	}
	const bool bAutomatic =
		Message == EShooterRadioMessage::FireInTheHole || Message == EShooterRadioMessage::BombPlanted;
	if (!bAutomatic &&
		(Now - SenderState->LastRadioTime < RadioCooldown ||
			SenderState->RadioMessagesInRound >= MaxRadioMessagesPerRound))
	{
		return false;
	}
	SenderState->LastRadioTime = Now;
	++SenderState->RadioMessagesInRound;

	FShooterRadioEntry Entry;
	Entry.SenderName = GetDisplayName(Sender, SenderPawn);
	Entry.Team = SenderState->GetTeam();
	Entry.Message = Message;
	Entry.Location = Location != nullptr ? *Location : SenderPawn->GetActorLocation();
	Entry.Time = Now;
	State->AddRadioEntry(Entry);
	UE_LOG(LogShooter, Log, TEXT("Radio: %s (%s): %s"), *Entry.SenderName, GetShooterTeamName(Entry.Team),
		GetRadioMessageText(Message));

	// The team's local players hear it (its sound).
	for (APlayerState* PlayerState : GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
		AShooterPlayerController* Player = Cast<AShooterPlayerController>(GetStateController(PlayerState));
		if (Player != nullptr && ShooterState != nullptr && Player->IsLocalController() &&
			ShooterState->GetTeam() == Entry.Team)
		{
			Player->HearRadio(Entry);
		}
	}

	// The team's bots hear it; a request is answered by the living bot nearest the sender (the first in the players'
	// order on a tie).
	AShooterAIController* Responder = nullptr;
	float ResponderDistSq = 0.0f;
	for (APlayerState* PlayerState : GetGameState().GetPlayerArray())
	{
		AShooterAIController* Bot = Cast<AShooterAIController>(GetStateController(PlayerState));
		const AShooterCharacter* BotPawn = Bot != nullptr ? Bot->GetShooterPawn() : nullptr;
		if (Bot == nullptr || Bot == Sender || BotPawn == nullptr || !BotPawn->IsAlive() ||
			BotPawn->GetTeam() != Entry.Team)
		{
			continue;
		}
		Bot->OnRadioMessage(Entry);
		const float DistSq = FVector::DistSquared(BotPawn->GetActorLocation(), SenderPawn->GetActorLocation());
		if (IsRadioRequest(Message) && (Responder == nullptr || DistSq < ResponderDistSq))
		{
			Responder = Bot;
			ResponderDistSq = DistSq;
		}
	}
	if (Responder != nullptr)
	{
		Responder->AnswerRadioRequest(Entry);
	}
	return true;
}

void AShooterGameMode::PayTeam(EShooterTeam Team, int32 Amount)
{
	for (APlayerState* PlayerState : GetGameState().GetPlayerArray())
	{
		AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
		if (ShooterState != nullptr && ShooterState->GetTeam() == Team)
		{
			(void)ShooterState->AddMoney(Amount, MaxMoney);
		}
	}
}

void AShooterGameMode::CheckRoundEnd()
{
	const AShooterGameState* State = GetShooterGameState();
	if (State == nullptr || State->GetRoundState() != EShooterRoundState::Live)
	{
		return;
	}
	const bool bPlanted = State->GetBombState() == EShooterBombState::Planted;
	if (GetTeamSize(EShooterTeam::CT) > 0 && GetTeamSize(EShooterTeam::T) > 0)
	{
		// The deaths of the last step end the round first, then its time (OnPhaseTimer).
		const int32 AliveCT = CountAlive(EShooterTeam::CT);
		const int32 AliveT = CountAlive(EShooterTeam::T);
		if (AliveCT == 0 && AliveT == 0 && !bPlanted)
		{
			EndRound(EShooterRoundEndReason::Draw);
			return;
		}
		if (AliveCT == 0)
		{
			EndRound(EShooterRoundEndReason::CTsEliminated);
			return;
		}
		if (AliveT == 0 && !bPlanted)
		{
			EndRound(EShooterRoundEndReason::TerroristsEliminated);
			return;
		}
	}
}

void AShooterGameMode::SetPhaseTimer(float Seconds)
{
	GetWorldTimerManager().SetTimer(TimerHandle_Phase, this, &AShooterGameMode::OnPhaseTimer, Seconds);
}

void AShooterGameMode::OnPhaseTimer()
{
	AShooterGameState* State = GetShooterGameState();
	if (State == nullptr || !IsMatchInProgress())
	{
		return;
	}
	const float Now = GetWorldTime();
	switch (State->GetRoundState())
	{
		case EShooterRoundState::Freeze:
			State->SetRoundState(EShooterRoundState::Live, Now + RoundTime);
			SetPhaseTimer(RoundTime);
			State->SetBuyEndTime(Now + BuyTime);
			GetWorldTimerManager().SetTimer(TimerHandle_BuyTime, BuyTime, false);
			break;
		case EShooterRoundState::Live:
			// The eliminations of the last step come first; the clock only runs out before a plant (after it the bomb
			// decides).
			CheckRoundEnd();
			if (State->GetRoundState() == EShooterRoundState::Live &&
				State->GetBombState() != EShooterBombState::Planted)
			{
				EndRound(EShooterRoundEndReason::TargetSaved);
			}
			break;
		case EShooterRoundState::RoundEnd:
		{
			const int32 RoundsToWin = (MaxRounds / 2) + 1;
			if (State->GetTeamScore(EShooterTeam::CT) >= RoundsToWin ||
				State->GetTeamScore(EShooterTeam::T) >= RoundsToWin || State->GetRoundNumber() >= MaxRounds)
			{
				EndMatch();
			}
			else
			{
				if (State->GetRoundNumber() == GetHalftimeRound() && !State->IsSecondHalf())
				{
					HandleHalftime();
				}
				StartRound();
			}
			break;
		}
		case EShooterRoundState::Warmup:
		case EShooterRoundState::MatchEnd:
			break;
	}
}

void AShooterGameMode::RestartGame(float Delay)
{
	// A delay of 0 restarts at the next step.
	GetWorldTimerManager().SetTimer(
		TimerHandle_RestartGame, this, &AShooterGameMode::OnRestartGameTimer, FMath::Max(Delay, KINDA_SMALL_NUMBER));
	UE_LOG(LogShooter, Display, TEXT("The game will restart in %.0f second(s)"), static_cast<double>(Delay));
}

void AShooterGameMode::OnRestartGameTimer()
{
	if (IsMatchInProgress())
	{
		BeginNewMatch();
	}
	else if (!HasMatchStarted())
	{
		// From the warmup only with both teams in: an empty team would lose every round on time.
		if (ReadyToStartMatch())
		{
			StartMatch();
		}
		else
		{
			UE_LOG(LogShooter, Warning, TEXT("mp_restartgame: both teams need a player (CT %d, T %d)"),
				GetTeamSize(EShooterTeam::CT), GetTeamSize(EShooterTeam::T));
		}
	}
	else
	{
		// After the match's end: UE's states only go forward, so the match goes on in progress again.
		SetMatchState(MatchState::InProgress);
	}
}

void AShooterGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AShooterGameState* State = GetShooterGameState();
	if (State == nullptr)
	{
		return;
	}
	if (bBotMatch)
	{
		TickBotMatch();
	}
	if (GetMatchState() == MatchState::WaitingToStart)
	{
		bool bHasHuman = false;
		for (const APlayerState* PlayerState : GetGameState().GetPlayerArray())
		{
			const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
			bHasHuman |= ShooterState != nullptr && !ShooterState->bIsABot;
		}
		if (bFillTeamsWithBots && (bHasHuman || bBotMatch))
		{
			(void)FillTeamsWithBots();
		}
		if (ReadyToStartMatch())
		{
			StartMatch();
		}
		return;
	}
	// The phases end with their timer (OnPhaseTimer); a live round also ends with its eliminations.
	if (IsMatchInProgress() && State->GetRoundState() == EShooterRoundState::Live)
	{
		CheckRoundEnd();
	}
}

void AShooterGameMode::TickBotMatch()
{
	const AShooterGameState* State = GetShooterGameState();
	if (bBotMatchOver || State == nullptr)
	{
		return;
	}
	MatchChecker.Tick(*this);
	BotMatchPeakObjects = FMath::Max(BotMatchPeakObjects, GUObjectArray.GetObjectArrayNumMinusAvailable());
	const int32 RoundsPlayed = MatchChecker.GetRoundsPlayed();
	// A round lasts at most the freeze, the round, a bomb planted at its last second and the result.
	const AShooterBomb* BombDefaults = BombClass != nullptr ? BombClass->GetDefaultObject<AShooterBomb>() : nullptr;
	const float BombTimer = BombDefaults != nullptr ? BombDefaults->BombTimer : 0.0f;
	const float RoundDeadline = FreezeTime + RoundTime + BombTimer + RoundRestartDelay;
	const bool bTimedOut = GetWorldTime() > (static_cast<float>(BotMatchRounds) * RoundDeadline) + 30.0f;
	const bool bDone = RoundsPlayed >= BotMatchRounds || GetMatchState() == MatchState::WaitingPostMatch;
	if (!bDone && !bTimedOut)
	{
		return;
	}
	bBotMatchOver = true;
	for (const FString& Violation : MatchChecker.GetViolations())
	{
		UE_LOG(LogShooter, Error, TEXT("Botmatch: %s"), *Violation);
	}
	if (bTimedOut && !bDone)
	{
		UE_LOG(LogShooter, Error, TEXT("Botmatch: %d of %d round(s) ended in %.0f s of game time"), RoundsPlayed,
			BotMatchRounds, static_cast<double>(GetWorldTime()));
	}
	const bool bPassed = bDone && !MatchChecker.HasViolations();
	FString ReasonList;
	for (const EShooterRoundEndReason Reason : MatchChecker.GetRoundEndReasons())
	{
		ReasonList +=
			FString::Printf(TEXT("%s%d"), ReasonList.IsEmpty() ? TEXT("") : TEXT(","), static_cast<int32>(Reason));
	}
	// The scores are the sides': the teams that end the match on them (they switched at halftime).
	const FString Halftime = State->IsSecondHalf()
		? FString::Printf(TEXT("sides switched after round %d"), State->GetHalftimeRound())
		: FString(TEXT("no halftime"));
	UE_LOG(LogShooter, Display, TEXT("Botmatch %s: %d round(s), CT %d - T %d, %d kill(s), seed %d, %s, reasons [%s]"),
		bPassed ? TEXT("OK") : TEXT("FAILED"), RoundsPlayed, State->GetTeamScore(EShooterTeam::CT),
		State->GetTeamScore(EShooterTeam::T), NumKills, RandomSeed, *Halftime, *ReasonList);
	LogBotMatchBudget();
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void AShooterGameMode::LogBotMatchBudget() const
{
	// The numbers TestPAL logs on the PS2 (Engine/Platforms/PS2/Documentation/Budgets.md), for a whole game.
	const FUObjectReflectionStats Reflection = GetUObjectReflectionStats();
	const FMallocUsage Usage = FMemory::GetUsage();
	UE_LOG(LogShooter, Display,
		TEXT("Botmatch budget: %d classes, %d structs, %d enums, %d functions, %d properties, construction heap %d KB; "
			 "UObjects peak %d, now %d of %d slots; names %d, %d KB used of %d KB; GMalloc peak %llu KB, current %llu "
			 "KB"),
		Reflection.NumClasses, Reflection.NumStructs, Reflection.NumEnums, Reflection.NumFunctions,
		Reflection.NumProperties, static_cast<int32>(Reflection.ConstructionHeapBytes / 1024), BotMatchPeakObjects,
		GUObjectArray.GetObjectArrayNumMinusAvailable(), GUObjectArray.GetObjectArrayCapacity(), FName::GetNumNames(),
		FName::GetNameEntryMemorySize() / 1024, FName::GetNameTableMemorySize() / 1024,
		static_cast<unsigned long long>(Usage.PeakBytes / 1024),
		static_cast<unsigned long long>(Usage.CurrentBytes / 1024));
}

// The bomb

const TArray<FName>& AShooterGameMode::GetBombSiteNames() const
{
	UpdateMapCaches();
	return BombSiteNames;
}

bool AShooterGameMode::GetBombSiteLocation(FName Site, FVector& OutLocation) const
{
	UpdateMapCaches();
	const int32 Index = BombSiteNames.IndexOfByKey(Site);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	OutLocation = BombSiteLocations[Index];
	return true;
}

void AShooterGameMode::OnBombStateChanged(AShooterBomb* InBomb)
{
	AShooterGameState* State = GetShooterGameState();
	if (State != nullptr && InBomb != nullptr && InBomb == Bomb)
	{
		State->SetBombState(InBomb->GetBombState(), InBomb->GetExplodeTime(), InBomb->GetSite());
	}
}

void AShooterGameMode::OnBombPlanted(AShooterBomb* InBomb, AShooterCharacter* Planter)
{
	// Only this round's bomb, while the round is fought, counts (and pays).
	const AShooterGameState* RoundState = GetShooterGameState();
	if (InBomb == nullptr || InBomb != Bomb || RoundState == nullptr ||
		RoundState->GetRoundState() != EShooterRoundState::Live)
	{
		return;
	}
	bBombPlantedThisRound = true;
	OnBombStateChanged(InBomb);
	AShooterPlayerState* PlanterState = Planter != nullptr && Planter->GetController() != nullptr
		? Planter->GetController()->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	if (PlanterState != nullptr)
	{
		(void)PlanterState->AddMoney(PlantReward, MaxMoney);
	}
	UE_LOG(LogShooter, Display, TEXT("The bomb has been planted at %s by %s"), *InBomb->GetSite().ToString(),
		Planter != nullptr ? *GetDisplayName(Planter->GetController(), Planter) : TEXT("?"));
	// A bot tells its team (CS's bots).
	if (AShooterAIController* Bot = Planter != nullptr ? Cast<AShooterAIController>(Planter->GetController()) : nullptr)
	{
		const FVector Site = InBomb->GetActorLocation();
		(void)SendRadioMessage(Bot, EShooterRadioMessage::BombPlanted, &Site);
	}
}

void AShooterGameMode::OnBombDefused(AShooterBomb* InBomb, AShooterCharacter* Defuser)
{
	OnBombStateChanged(InBomb);
	AShooterPlayerState* DefuserState = Defuser != nullptr && Defuser->GetController() != nullptr
		? Defuser->GetController()->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	if (DefuserState != nullptr)
	{
		(void)DefuserState->AddMoney(DefuseReward, MaxMoney);
	}
	EndRound(EShooterRoundEndReason::BombDefused);
}

void AShooterGameMode::OnBombExploded(AShooterBomb* InBomb)
{
	OnBombStateChanged(InBomb);
	EndRound(EShooterRoundEndReason::TargetBombed);
}

// Buying

ATriggerVolume* AShooterGameMode::FindZone(const FVector& Feet, FName Kind, FName SecondTag) const
{
	const FVector Point = Feet + FVector(0.0f, 0.0f, ZoneTestHeight);
	for (ATriggerVolume* Zone : Zones)
	{
		if (Zone != nullptr && !Zone->IsPendingKillPending() && Zone->ActorHasTag(Kind) &&
			(SecondTag == NAME_None || Zone->ActorHasTag(SecondTag)) && Zone->EncompassesPoint(Point))
		{
			return Zone;
		}
	}
	return nullptr;
}

ATriggerVolume* AShooterGameMode::FindLadder(const FVector& Feet, float Radius, float Height) const
{
	for (ATriggerVolume* Zone : Zones)
	{
		if (Zone == nullptr || Zone->IsPendingKillPending() || !Zone->ActorHasTag(LadderTag))
		{
			continue;
		}
		const FBox Box = Zone->GetBrushBounds();
		if (Feet.X + Radius > Box.Min.X && Feet.X - Radius < Box.Max.X && Feet.Y + Radius > Box.Min.Y &&
			Feet.Y - Radius < Box.Max.Y && Feet.Z < Box.Max.Z && Feet.Z + Height > Box.Min.Z)
		{
			return Zone;
		}
	}
	return nullptr;
}

FName AShooterGameMode::GetZoneName(const ATriggerVolume& Zone, FName Kind)
{
	for (const FName& Tag : Zone.Tags)
	{
		if (Tag != Kind)
		{
			return Tag;
		}
	}
	return NAME_None;
}

bool AShooterGameMode::CanBuy(const AShooterCharacter& Buyer, FString* OutReason) const
{
	auto Refuse = [OutReason](const TCHAR* Reason)
	{
		if (OutReason != nullptr)
		{
			*OutReason = Reason;
		}
		return false;
	};
	const AShooterGameState* State = GetShooterGameState();
	if (!Buyer.IsAlive() || State == nullptr)
	{
		return Refuse(TEXT("dead"));
	}
	const EShooterRoundState RoundState = State->GetRoundState();
	if (RoundState == EShooterRoundState::MatchEnd || RoundState == EShooterRoundState::RoundEnd ||
		(RoundState != EShooterRoundState::Warmup && !GetWorldTimerManager().IsTimerActive(TimerHandle_BuyTime)))
	{
		return Refuse(TEXT("the buy time is over"));
	}
	if (FindZone(Buyer.GetActorLocation(), BuyZoneTag, GetShooterTeamTag(Buyer.GetTeam())) == nullptr)
	{
		return Refuse(TEXT("not in a buy zone"));
	}
	return true;
}

int32 AShooterGameMode::GetPrice(const AShooterCharacter& Buyer, const FString& Item) const
{
	if (Item.Equals(VestItem, ESearchCase::IgnoreCase))
	{
		return Buyer.GetArmor() >= Buyer.MaxArmor ? -1 : VestPrice;
	}
	if (Item.Equals(VestHelmetItem, ESearchCase::IgnoreCase))
	{
		if (Buyer.GetArmor() >= Buyer.MaxArmor)
		{
			return Buyer.HasHelmet() ? -1 : HelmetPrice;
		}
		return VestHelmetPrice;
	}
	if (Item.Equals(DefuserItem, ESearchCase::IgnoreCase))
	{
		return Buyer.GetTeam() == EShooterTeam::CT && !Buyer.HasDefuseKit() ? DefuserPrice : -1;
	}
	if (IsAmmoItem(Item))
	{
		// A box of the weapon's calibre, while its reserve has room (CS: buyammo1 / buyammo2).
		const AShooterWeapon* Weapon = GetAmmoWeapon(Buyer, Item);
		return Weapon != nullptr && Weapon->NeedsAmmo() && Weapon->AmmoBoxPrice > 0 ? Weapon->AmmoBoxPrice : -1;
	}
	UClass* WeaponClass = AShooterWeapon::FindWeaponClass(Item);
	if (WeaponClass == nullptr)
	{
		return -1;
	}
	// Not for sale (the knife) or not for the buyer's team (CS: the AK-47 for the T, the M4A1 for the CT).
	const AShooterWeapon* Defaults = WeaponClass->GetDefaultObject<AShooterWeapon>();
	if (!Defaults->CanBeBoughtBy(Buyer.GetTeam()))
	{
		return -1;
	}
	// A grenade already carried: one more up to its limit (CS: two flashbangs, one HE, one smoke grenade).
	if (Defaults->Slot == EShooterWeaponSlot::Grenade)
	{
		const AShooterWeapon* Carried = Buyer.FindWeaponOfClass(WeaponClass);
		return Carried != nullptr && Carried->GetCurrentAmmoInClip() >= Carried->AmmoPerClip ? -1 : Defaults->Price;
	}
	const AShooterWeapon* Owned = Buyer.GetWeaponInSlot(Defaults->Slot);
	return Owned != nullptr && Owned->GetClass() == WeaponClass ? -1 : Defaults->Price;
}

bool AShooterGameMode::Buy(AShooterCharacter* Buyer, const FString& Item, FString* OutReason)
{
	AShooterPlayerState* State = Buyer != nullptr && Buyer->GetController() != nullptr
		? Buyer->GetController()->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	if (Buyer == nullptr || State == nullptr || !CanBuy(*Buyer, OutReason))
	{
		return false;
	}
	const int32 Price = GetPrice(*Buyer, Item);
	if (Price < 0)
	{
		if (OutReason != nullptr)
		{
			const UClass* WeaponClass = AShooterWeapon::FindWeaponClass(Item);
			const AShooterWeapon* Defaults =
				WeaponClass != nullptr ? WeaponClass->GetDefaultObject<AShooterWeapon>() : nullptr;
			if (Defaults != nullptr && Defaults->Price > 0 && !Defaults->CanBeBoughtBy(Buyer->GetTeam()))
			{
				*OutReason =
					FString::Printf(TEXT("only the %s can buy '%s'"), GetShooterTeamName(Defaults->BuyTeam), *Item);
			}
			else if (IsAmmoItem(Item))
			{
				*OutReason = GetAmmoWeapon(*Buyer, Item) == nullptr ? FString(TEXT("no weapon for that ammunition"))
																	: FString(TEXT("the ammunition is full"));
			}
			else if (Defaults != nullptr && Defaults->Slot == EShooterWeaponSlot::Grenade)
			{
				*OutReason = FString::Printf(TEXT("cannot carry more of '%s'"), *Item);
			}
			else
			{
				*OutReason = FString::Printf(TEXT("cannot buy '%s'"), *Item);
			}
		}
		return false;
	}
	if (State->GetMoney() < Price)
	{
		if (OutReason != nullptr)
		{
			*OutReason = FString::Printf(TEXT("not enough money ($%d of $%d)"), State->GetMoney(), Price);
		}
		return false;
	}
	if (Item.Equals(VestItem, ESearchCase::IgnoreCase))
	{
		Buyer->SetArmor(Buyer->MaxArmor, Buyer->HasHelmet());
	}
	else if (Item.Equals(VestHelmetItem, ESearchCase::IgnoreCase))
	{
		Buyer->SetArmor(Buyer->MaxArmor, true);
	}
	else if (Item.Equals(DefuserItem, ESearchCase::IgnoreCase))
	{
		Buyer->SetDefuseKit(true);
	}
	else if (IsAmmoItem(Item))
	{
		AShooterWeapon* Weapon = GetAmmoWeapon(*Buyer, Item);
		(void)Weapon->GiveAmmo(Weapon->AmmoBoxRounds);
	}
	else
	{
		UClass* WeaponClass = AShooterWeapon::FindWeaponClass(Item);
		AShooterWeapon_Projectile* Carried = Cast<AShooterWeapon_Projectile>(Buyer->FindWeaponOfClass(WeaponClass));
		if (Carried != nullptr)
		{
			// One more of a grenade carried (GetPrice has checked the limit).
			(void)Carried->AddGrenade();
		}
		else
		{
			AShooterWeapon* Weapon = Buyer->GiveWeapon(WeaponClass);
			if (Weapon == nullptr)
			{
				return false;
			}
			// A gun is drawn; a grenade waits in its slot (CS).
			if (Weapon->Slot != EShooterWeaponSlot::Grenade)
			{
				Buyer->EquipWeapon(Weapon);
			}
		}
	}
	(void)State->AddMoney(-Price, MaxMoney);
	UE_LOG(LogShooter, Log, TEXT("%s bought %s for $%d ($%d left)"), *GetDisplayName(Buyer->GetController(), Buyer),
		*Item, Price, State->GetMoney());
	return true;
}

// Console

bool AShooterGameMode::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	const TCHAR* Str = Cmd;
	if (FParse::Command(&Str, TEXT("bot_fill")))
	{
		Ar.Logf(TEXT("%d bot(s) added"), FillTeamsWithBots());
		return true;
	}
	if (FParse::Command(&Str, TEXT("bot_kick")))
	{
		FString Name;
		(void)FParse::Token(Str, Name, false);
		Ar.Logf(TEXT("%d bot(s) kicked"), KickBots(Name));
		return true;
	}
	if (FParse::Command(&Str, TEXT("bot_stop")))
	{
		FString Value;
		bBotStop = FParse::Token(Str, Value, false) ? FCString::Atoi(*Value) != 0 : !bBotStop;
		Ar.Logf(TEXT("bot_stop %d"), bBotStop ? 1 : 0);
		return true;
	}
	if (FParse::Command(&Str, TEXT("mp_restartgame")))
	{
		FString DelayText;
		const float Delay =
			FParse::Token(Str, DelayText, false) ? static_cast<float>(FCString::Atoi(*DelayText)) : 1.0f;
		RestartGame(Delay);
		return true;
	}
	if (FParse::Command(&Str, TEXT("mp_maxrounds")))
	{
		FString Value;
		if (FParse::Token(Str, Value, false))
		{
			MaxRounds = FMath::Max(1, FCString::Atoi(*Value));
		}
		Ar.Logf(TEXT("mp_maxrounds %d (halftime after round %d)"), MaxRounds, GetHalftimeRound());
		return true;
	}
	if (FParse::Command(&Str, TEXT("mp_halftime")))
	{
		FString Value;
		bHalftime = FParse::Token(Str, Value, false) ? FCString::Atoi(*Value) != 0 : !bHalftime;
		Ar.Logf(TEXT("mp_halftime %d"), bHalftime ? 1 : 0);
		return true;
	}
	EShooterTeam Team = EShooterTeam::None;
	bool bBotCommand = true;
	if (FParse::Command(&Str, TEXT("bot_add_ct")))
	{
		Team = EShooterTeam::CT;
	}
	else if (FParse::Command(&Str, TEXT("bot_add_t")))
	{
		Team = EShooterTeam::T;
	}
	else if (!FParse::Command(&Str, TEXT("bot_add")))
	{
		bBotCommand = false;
	}
	if (bBotCommand)
	{
		FString CountText;
		const int32 Count = FParse::Token(Str, CountText, false) ? FMath::Max(1, FCString::Atoi(*CountText)) : 1;
		const int32 Added = AddBots(Team, Count);
		Ar.Logf(TEXT("%d bot(s) added"), Added);
		return true;
	}
	return Super::ProcessConsoleExec(Cmd, Ar, Executor);
}

void AShooterGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	int32 NumCT = 0;
	int32 NumT = 0;
	CountPawns(NumCT, NumT);
	UE_LOG(LogShooter, Display, TEXT("ShooterGameMode: %d pawn(s) at the end of the match, CT %d, T %d"), NumCT + NumT,
		NumCT, NumT);
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	// A world that never went through LoadMap (the tests' worlds) does not leave its preloads in flight.
	for (const int32 RequestId : PreloadRequestIds)
	{
		FlushAsyncLoading(RequestId);
	}
	PreloadRequestIds.Reset();
	Super::EndPlay(EndPlayReason);
}
