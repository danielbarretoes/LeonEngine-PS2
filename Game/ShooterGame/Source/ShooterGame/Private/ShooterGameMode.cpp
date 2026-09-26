#include "ShooterGameMode.h"

#include "Components/CapsuleComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Parse.h"
#include "ShooterAIController.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterHUD.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "Weapons/ShooterProjectile.h"
#include "Weapons/ShooterWeapon.h"

namespace
{

	/** A start's capsule radius when it has none, and a pawn's when it is not a character (cm, UE's 40 x 92). */
	constexpr float DefaultStartRadius = 40.0f;

	/** The half height of a start's capsule: its location is the capsule's centre (UE). */
	float GetStartHalfHeight(const AActor& StartSpot)
	{
		const APlayerStart* Start = Cast<APlayerStart>(&StartSpot);
		const UCapsuleComponent* Capsule = Start != nullptr ? Start->GetCapsuleComponent() : nullptr;
		return Capsule != nullptr ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
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
			if (Pawn == nullptr || Pawn->IsPendingKillPending())
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

} // namespace

AShooterGameMode::AShooterGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = AShooterCharacter::StaticClass();
	PlayerControllerClass = AShooterPlayerController::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	HUDClass = AShooterHUD::StaticClass();
	GameStateClass = AGameState::StaticClass();
	BotControllerClass = AShooterAIController::StaticClass();
}

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
	OutCT = 0;
	OutT = 0;
	const UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return;
	}
	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		const AShooterCharacter* Character = Cast<AShooterCharacter>(Actor);
		if (Character == nullptr || Character->IsPendingKillPending() || !Character->IsAlive())
		{
			continue;
		}
		const EShooterTeam Team = Character->GetTeam();
		OutCT += Team == EShooterTeam::CT ? 1 : 0;
		OutT += Team == EShooterTeam::T ? 1 : 0;
	}
}

FString AShooterGameMode::InitNewPlayer(
	APlayerController* NewPlayerController, const FString& Options, const FString& Portal)
{
	// The team first: the start spot chosen below depends on it (UE ShooterGame: ChooseTeam in PostLogin).
	if (AShooterPlayerState* State =
			NewPlayerController != nullptr ? NewPlayerController->GetPlayerState<AShooterPlayerState>() : nullptr)
	{
		State->SetTeam(ChooseTeam(Options));
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

void AShooterGameMode::RestartPlayer(AController* NewPlayer)
{
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

bool AShooterGameMode::CanDealDamage(AController* Instigator, AController* Victim) const
{
	if (Instigator == nullptr || Victim == nullptr || Instigator == Victim || bFriendlyFire)
	{
		return true;
	}
	const AShooterPlayerState* InstigatorState = Instigator->GetPlayerState<AShooterPlayerState>();
	const AShooterPlayerState* VictimState = Victim->GetPlayerState<AShooterPlayerState>();
	return InstigatorState == nullptr || VictimState == nullptr || InstigatorState->GetTeam() == EShooterTeam::None ||
		InstigatorState->GetTeam() != VictimState->GetTeam();
}

void AShooterGameMode::Killed(
	AController* Killer, AController* Victim, APawn* VictimPawn, AActor* DamageCauser, bool bHeadshot)
{
	++NumKills;
	FString WeaponName = TEXT("world");
	if (const AShooterWeapon* Weapon = Cast<AShooterWeapon>(DamageCauser))
	{
		WeaponName = Weapon->WeaponName;
	}
	else if (DamageCauser != nullptr && DamageCauser->GetOwner() != nullptr)
	{
		// A projectile's owner is the weapon that threw it.
		if (const AShooterWeapon* Launcher = Cast<AShooterWeapon>(DamageCauser->GetOwner()))
		{
			WeaponName = Launcher->WeaponName;
		}
	}
	const FString VictimName = GetDisplayName(Victim, VictimPawn);
	if (Killer == nullptr || Killer == Victim)
	{
		UE_LOG(LogShooter, Log, TEXT("Kill: %s died (%s)"), *VictimName, *WeaponName);
		return;
	}
	UE_LOG(LogShooter, Log, TEXT("Kill: %s killed %s with %s%s"), *GetDisplayName(Killer, nullptr), *VictimName,
		*WeaponName, bHeadshot ? TEXT(" (headshot)") : TEXT(""));
}

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
		AShooterAIController* Bot = World->SpawnActor<AShooterAIController>(
			BotControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
		AShooterPlayerState* State = Bot != nullptr ? Bot->GetPlayerState<AShooterPlayerState>() : nullptr;
		if (State == nullptr)
		{
			UE_LOG(LogShooter, Error, TEXT("AddBots: the bot controller has no ShooterPlayerState"));
			continue;
		}
		const int32 TeamIndex = static_cast<int32>(BotTeam);
		State->SetTeam(BotTeam);
		State->bIsABot = true;
		State->SetPlayerName(
			FString::Printf(TEXT("Bot_%s_%d"), GetShooterTeamName(BotTeam), NextBotNumber[TeamIndex]++));
		GetGameState().AddPlayerState(State);
		RestartPlayer(Bot);
		if (Bot->GetPawn() != nullptr)
		{
			++Added;
		}
	}
	return Added;
}

int32 AShooterGameMode::FillTeamsWithBots()
{
	const int32 AddedCT = AddBots(EShooterTeam::CT, FMath::Max(0, MaxPlayersPerTeam - GetTeamSize(EShooterTeam::CT)));
	const int32 AddedT = AddBots(EShooterTeam::T, FMath::Max(0, MaxPlayersPerTeam - GetTeamSize(EShooterTeam::T)));
	return AddedCT + AddedT;
}

bool AShooterGameMode::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	const TCHAR* Str = Cmd;
	if (FParse::Command(&Str, TEXT("bot_fill")))
	{
		Ar.Logf(TEXT("%d bot(s) added"), FillTeamsWithBots());
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
