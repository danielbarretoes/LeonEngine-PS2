#include "ShooterGameMode.h"

#include "Components/CapsuleComponent.h"
#include "Engine/Level.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformMisc.h"
#include "HAL/UnrealMemory.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ShooterAIController.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameState.h"
#include "ShooterHUD.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectBase.h"
#include "Weapons/ShooterProjectile.h"
#include "Weapons/ShooterWeapon.h"

namespace
{

	/** A start's capsule radius when it has none, and a pawn's when it is not a character (cm, UE's 40 x 92). */
	constexpr float DefaultStartRadius = 40.0f;

	/** How high above the feet a pawn is tested against a zone (the volumes stand on the floor), cm. */
	constexpr float ZoneTestHeight = 50.0f;

	/** The half height of a start's capsule: its location is the capsule's centre (UE). */
	float GetStartHalfHeight(const AActor& StartSpot)
	{
		const APlayerStart* Start = Cast<APlayerStart>(&StartSpot);
		const UCapsuleComponent* Capsule = Start != nullptr ? Start->GetCapsuleComponent() : nullptr;
		return Capsule != nullptr ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
	}

	/** A pawn that stands somewhere: not being destroyed, and alive when it is a shooter. */
	bool IsStandingPawn(const APawn& Pawn)
	{
		const AShooterCharacter* Shooter = Cast<AShooterCharacter>(&Pawn);
		return !Pawn.IsPendingKillPending() && (Shooter == nullptr || Shooter->IsAlive());
	}

	/** True when a live pawn stands on the start: within two capsule radii horizontally and 2 m vertically. */
	bool IsStartOccupied(const UWorld& World, const APlayerStart& Start)
	{
		const FVector StartLocation = Start.GetActorLocation();
		const UCapsuleComponent* StartCapsule = Start.GetCapsuleComponent();
		const float StartRadius = StartCapsule != nullptr ? StartCapsule->GetScaledCapsuleRadius() : DefaultStartRadius;
		for (const AActor* Actor : World.PersistentLevel->Actors)
		{
			const APawn* Pawn = Cast<APawn>(Actor);
			if (Pawn == nullptr || !IsStandingPawn(*Pawn))
			{
				continue;
			}
			const ACharacter* Character = Cast<ACharacter>(Pawn);
			const float PawnRadius =
				Character != nullptr ? Character->GetCapsule().GetCapsuleRadius() : DefaultStartRadius;
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

	/** The equipment names Buy takes besides the weapons'. */
	const TCHAR* const VestItem = TEXT("vest");
	const TCHAR* const VestHelmetItem = TEXT("vesthelm");
	const TCHAR* const DefuserItem = TEXT("defuser");

} // namespace

const FName AShooterGameMode::BombSiteTag(TEXT("BombSite"));
const FName AShooterGameMode::BuyZoneTag(TEXT("BuyZone"));

AShooterGameMode::AShooterGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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
		BotMatchRounds = FMath::Clamp(BotMatchRounds, 1, MaxRounds);
		UE_LOG(LogShooter, Display, TEXT("Botmatch: %d round(s), seed %d"), BotMatchRounds, RandomSeed);
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
	int32 NumCT = 0;
	int32 NumT = 0;
	CountPawns(NumCT, NumT);
	return Team == EShooterTeam::CT ? NumCT : Team == EShooterTeam::T ? NumT : 0;
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
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		APlayerStart* Start = Cast<APlayerStart>(Actor);
		if (Start == nullptr || Start->IsPendingKillPending() || Start->PlayerStartTag != TeamTag)
		{
			continue;
		}
		if (FirstTeamStart == nullptr)
		{
			FirstTeamStart = Start;
		}
		if (!IsStartOccupied(*World, *Start))
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

TArray<AActor*> AShooterGameMode::GetTeamStarts(EShooterTeam Team) const
{
	TArray<AActor*> Starts;
	const UWorld* World = GetWorld();
	const FName TeamTag = GetShooterTeamTag(Team);
	if (World == nullptr || World->PersistentLevel == nullptr || TeamTag == NAME_None)
	{
		return Starts;
	}
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		const APlayerStart* Start = Cast<APlayerStart>(Actor);
		if (Start != nullptr && !Start->IsPendingKillPending() && Start->PlayerStartTag == TeamTag)
		{
			Starts.Add(Actor);
		}
	}
	return Starts;
}

bool AShooterGameMode::GetTeamSpawnLocation(EShooterTeam Team, FVector& OutLocation) const
{
	const TArray<AActor*> Starts = GetTeamStarts(Team);
	if (Starts.Num() == 0)
	{
		return false;
	}
	OutLocation = Starts[0]->GetActorLocation();
	return true;
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
		State->SetTeam(BotTeam);
		State->bIsABot = true;
		State->SetMoney(StartMoney, MaxMoney);
		State->SetPlayerName(
			FString::Printf(TEXT("Bot_%s_%d"), GetShooterTeamName(BotTeam), NextBotNumber[TeamIndex]++));
		GetGameState().AddPlayerState(State);
		RestartPlayer(BotController);
		++Added;
	}
	return Added;
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
	bRestartPending = false;
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
	const UWorld* World = GetWorld();
	if (World != nullptr && World->PersistentLevel != nullptr)
	{
		TArray<AShooterCharacter*> Pawns;
		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(Actor))
			{
				Pawns.Add(Shooter);
			}
		}
		for (AShooterCharacter* Shooter : Pawns)
		{
			if (AController* Controller = Shooter->GetController())
			{
				Controller->UnPossess();
			}
			(void)Shooter->Destroy();
		}
	}
	StartRound();
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
	UE_LOG(LogShooter, Display, TEXT("Match over after %d round(s): CT %d - T %d, %s"), State->GetRoundNumber(),
		ScoreCT, ScoreT,
		State->GetMatchWinner() == EShooterTeam::None     ? TEXT("a draw")
			: State->GetMatchWinner() == EShooterTeam::CT ? TEXT("the Counter-Terrorists win")
														  : TEXT("the Terrorists win"));
}

void AShooterGameMode::CleanUpMap()
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return;
	}
	TArray<AActor*> ToDestroy;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (Actor == nullptr || Actor->IsPendingKillPending())
		{
			continue;
		}
		const AShooterWeapon* Weapon = Cast<AShooterWeapon>(Actor);
		const AShooterCharacter* Shooter = Cast<AShooterCharacter>(Actor);
		if ((Weapon != nullptr && Weapon->IsDropped()) || Actor->IsA<AShooterProjectile>() ||
			Actor->IsA<AShooterBomb>() || (Shooter != nullptr && !Shooter->IsAlive()))
		{
			ToDestroy.Add(Actor);
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
	bBombPlantedThisRound = false;
	const float Now = GetWorldTime();
	State->SetRoundNumber(State->GetRoundNumber() + 1);
	// Freeze first: RestartPlayer spawns during the freeze.
	State->SetRoundState(EShooterRoundState::Freeze, Now + FreezeTime);
	State->SetBuyEndTime(Now + BuyTime);
	State->SetBombState(EShooterBombState::None);

	// Each team on its starts, in the order its players joined; the survivors keep their weapons.
	TArray<AShooterCharacter*> Terrorists;
	for (const EShooterTeam Team : {EShooterTeam::CT, EShooterTeam::T})
	{
		const TArray<AActor*> Starts = GetTeamStarts(Team);
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
	const TArray<FName> Sites = GetBombSiteNames();
	TerroristTargetSite = Sites.Num() > 0 ? Sites[RoundRandom.RandRange(0, Sites.Num() - 1)] : NAME_None;
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
	// The clock only runs out before a plant; after it the bomb decides.
	if (!bPlanted && GetWorldTime() >= State->GetPhaseEndTime())
	{
		EndRound(EShooterRoundEndReason::TargetSaved);
	}
}

void AShooterGameMode::RestartGame(float Delay)
{
	RestartGameTime = GetWorldTime() + FMath::Max(0.0f, Delay);
	bRestartPending = true;
	UE_LOG(LogShooter, Display, TEXT("The game will restart in %.0f second(s)"), static_cast<double>(Delay));
}

void AShooterGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AShooterGameState* State = GetShooterGameState();
	if (State == nullptr)
	{
		return;
	}
	if (bBotMatch)
	{
		TickBotMatch();
	}
	const float Now = GetWorldTime();
	if (bRestartPending && Now >= RestartGameTime)
	{
		bRestartPending = false;
		if (IsMatchInProgress())
		{
			BeginNewMatch();
		}
		else if (!HasMatchStarted())
		{
			StartMatch();
		}
		else
		{
			// After the match's end: UE's states only go forward, so the match goes on in progress again.
			SetMatchState(MatchState::InProgress);
		}
		return;
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
	if (!IsMatchInProgress())
	{
		return;
	}
	switch (State->GetRoundState())
	{
		case EShooterRoundState::Freeze:
			if (Now >= State->GetPhaseEndTime())
			{
				State->SetRoundState(EShooterRoundState::Live, Now + RoundTime);
			}
			break;
		case EShooterRoundState::Live:
			CheckRoundEnd();
			break;
		case EShooterRoundState::RoundEnd:
			if (Now >= State->GetPhaseEndTime())
			{
				const int32 RoundsToWin = (MaxRounds / 2) + 1;
				if (State->GetTeamScore(EShooterTeam::CT) >= RoundsToWin ||
					State->GetTeamScore(EShooterTeam::T) >= RoundsToWin || State->GetRoundNumber() >= MaxRounds)
				{
					EndMatch();
				}
				else
				{
					StartRound();
				}
			}
			break;
		case EShooterRoundState::Warmup:
		case EShooterRoundState::MatchEnd:
			break;
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
	UE_LOG(LogShooter, Display, TEXT("Botmatch %s: %d round(s), CT %d - T %d, %d kill(s), seed %d, reasons [%s]"),
		bPassed ? TEXT("OK") : TEXT("FAILED"), RoundsPlayed, State->GetTeamScore(EShooterTeam::CT),
		State->GetTeamScore(EShooterTeam::T), NumKills, RandomSeed, *ReasonList);
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

TArray<FName> AShooterGameMode::GetBombSiteNames() const
{
	TArray<FName> Names;
	const UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return Names;
	}
	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		const ATriggerVolume* Zone = Cast<ATriggerVolume>(Actor);
		if (Zone != nullptr && !Zone->IsPendingKillPending() && Zone->ActorHasTag(BombSiteTag))
		{
			const FName Name = GetZoneName(*Zone, BombSiteTag);
			if (Name != NAME_None)
			{
				Names.AddUnique(Name);
			}
		}
	}
	Names.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
	return Names;
}

bool AShooterGameMode::GetBombSiteLocation(FName Site, FVector& OutLocation) const
{
	const UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return false;
	}
	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		const ATriggerVolume* Zone = Cast<ATriggerVolume>(Actor);
		if (Zone != nullptr && !Zone->IsPendingKillPending() && Zone->ActorHasTag(BombSiteTag) &&
			Zone->ActorHasTag(Site))
		{
			const FBox Bounds = Zone->GetBrushBounds();
			OutLocation = FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);
			return true;
		}
	}
	return false;
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

ATriggerVolume* AShooterGameMode::FindZone(const UWorld& World, const FVector& Feet, FName Kind, FName SecondTag)
{
	if (World.PersistentLevel == nullptr)
	{
		return nullptr;
	}
	const FVector Point = Feet + FVector(0.0f, 0.0f, ZoneTestHeight);
	for (AActor* Actor : World.PersistentLevel->Actors)
	{
		ATriggerVolume* Zone = Cast<ATriggerVolume>(Actor);
		if (Zone != nullptr && !Zone->IsPendingKillPending() && Zone->ActorHasTag(Kind) &&
			(SecondTag == NAME_None || Zone->ActorHasTag(SecondTag)) && Zone->EncompassesPoint(Point))
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
	const UWorld* World = GetWorld();
	if (!Buyer.IsAlive() || World == nullptr || State == nullptr)
	{
		return Refuse(TEXT("dead"));
	}
	const EShooterRoundState RoundState = State->GetRoundState();
	if (RoundState == EShooterRoundState::MatchEnd || RoundState == EShooterRoundState::RoundEnd ||
		(RoundState != EShooterRoundState::Warmup && GetWorldTime() > State->GetBuyEndTime()))
	{
		return Refuse(TEXT("the buy time is over"));
	}
	if (FindZone(*World, Buyer.GetActorLocation(), BuyZoneTag, GetShooterTeamTag(Buyer.GetTeam())) == nullptr)
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
	UClass* WeaponClass = AShooterWeapon::FindWeaponClass(Item);
	if (WeaponClass == nullptr)
	{
		return -1;
	}
	const AShooterWeapon* Defaults = WeaponClass->GetDefaultObject<AShooterWeapon>();
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
			*OutReason = FString::Printf(TEXT("cannot buy '%s'"), *Item);
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
	else
	{
		AShooterWeapon* Weapon = Buyer->GiveWeapon(AShooterWeapon::FindWeaponClass(Item));
		if (Weapon == nullptr)
		{
			return false;
		}
		Buyer->EquipWeapon(Weapon);
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
	Super::EndPlay(EndPlayReason);
}
