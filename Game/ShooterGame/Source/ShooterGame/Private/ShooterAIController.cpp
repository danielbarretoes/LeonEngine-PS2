#include "ShooterAIController.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Misc/Crc.h"
#include "Perception/PawnSensingComponent.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPlayerState.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_Instant.h"
#include "Weapons/ShooterWeapon_Sniper.h"

namespace
{

	/** Where a bot aims on an enemy: the chest, above the feet, cm (standing; the capsule's middle crouched). */
	constexpr float AimHeightStanding = 130.0f;
	constexpr float AimHeightCrouched = 60.0f;

	/** The angle the aim may be off and still fire, beyond the error itself, degrees. */
	constexpr float FireTolerance = 1.5f;

	/** How near the bomb a defuser stands, cm (inside the bomb's DefuseRadius). */
	constexpr float DefuseApproach = 70.0f;

	/** A goal that moves less than this keeps the path, cm. */
	constexpr float GoalRepathDistance = 150.0f;

	/** A target this far or farther makes the AWP zoom, cm. */
	constexpr float SniperZoomDistance = 1200.0f;

	/** Seconds a heard shot is investigated. */
	constexpr float NoiseMemory = 6.0f;

	/** The sensing: CS bots see about 140 degrees and 60 m, and hear shots through the map. */
	constexpr float BotSightRadius = 6000.0f;
	constexpr float BotVisionHalfAngle = 70.0f;
	constexpr float BotSensingInterval = 0.1f;
	constexpr float BotHearingThreshold = 2500.0f;
	constexpr float BotLOSHearingThreshold = 5000.0f;

	FName TaskName(const TCHAR* Name)
	{
		return FName(Name);
	}

} // namespace

const FName AShooterAIController::EnemyKey(TEXT("Enemy"));
const FName AShooterAIController::HasEnemyKey(TEXT("HasEnemy"));
const FName AShooterAIController::EnemyLocationKey(TEXT("EnemyLocation"));
const FName AShooterAIController::ShouldDefuseKey(TEXT("ShouldDefuse"));
const FName AShooterAIController::CarriesBombKey(TEXT("CarriesBomb"));
const FName AShooterAIController::BombDroppedKey(TEXT("BombDropped"));
const FName AShooterAIController::HeardEnemyKey(TEXT("HeardEnemy"));
const FName AShooterAIController::NoiseLocationKey(TEXT("NoiseLocation"));
const FName AShooterAIController::GoalKey(TEXT("Goal"));

AShooterAIController::AShooterAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bot has a player state like a player (UE: bWantsPlayerState): its team, its score and its money.
	bWantsPlayerState = true;
	PawnSensing = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensing"));
	PawnSensing->bOnlySensePlayers = false;
	PawnSensing->SightRadius = BotSightRadius;
	PawnSensing->SetPeripheralVisionAngle(BotVisionHalfAngle);
	PawnSensing->SensingInterval = BotSensingInterval;
	PawnSensing->HearingThreshold = BotHearingThreshold;
	PawnSensing->LOSHearingThreshold = BotLOSHearingThreshold;
	BuildTree();
}

void AShooterAIController::BuildTree()
{
	auto Action = [this](TFunction<EBTNodeResult(UBlackboardComponent&, float)> Fn)
	{
		TreeNodes.Add(MakeUnique<UBTTask_Action>(MoveTemp(Fn)));
		return TreeNodes.Last().Get();
	};
	auto Decorator = [this](FName Key)
	{
		TreeNodes.Add(MakeUnique<UBTDecorator_Bool>(Key, true));
		return TreeNodes.Last().Get();
	};
	auto Sequence = [this](TArray<UBTNode*> Children)
	{
		TreeNodes.Add(MakeUnique<UBTComposite_Sequence>(MoveTemp(Children)));
		return TreeNodes.Last().Get();
	};
	UBTNode* Idle = Action([this](UBlackboardComponent&, float) { return TaskIdle(); });
	UBTNode* Engage = Sequence({Decorator(HasEnemyKey),
		Action([this](UBlackboardComponent&, float DeltaTime) { return TaskEngage(DeltaTime); })});
	UBTNode* Defuse =
		Sequence({Decorator(ShouldDefuseKey), Action([this](UBlackboardComponent&, float) { return TaskDefuse(); })});
	UBTNode* Plant =
		Sequence({Decorator(CarriesBombKey), Action([this](UBlackboardComponent&, float) { return TaskPlant(); })});
	UBTNode* Fetch =
		Sequence({Decorator(BombDroppedKey), Action([this](UBlackboardComponent&, float) { return TaskFetchBomb(); })});
	UBTNode* Investigate = Sequence(
		{Decorator(HeardEnemyKey), Action([this](UBlackboardComponent&, float) { return TaskInvestigate(); })});
	UBTNode* Objective = Action([this](UBlackboardComponent&, float DeltaTime) { return TaskObjective(DeltaTime); });
	TreeNodes.Add(MakeUnique<UBTComposite_Selector>(
		TArray<UBTNode*>{Idle, Engage, Defuse, Plant, Fetch, Investigate, Objective}));
	Tree.SetRoot(TreeNodes.Last().Get());
}

void AShooterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!bRandomSeeded)
	{
		// The bot's stream: the match's seed and the bot's name (the same bots replay the same choices).
		const AShooterGameMode* GameMode = GetShooterGameMode();
		const AShooterPlayerState* State = GetPlayerState<AShooterPlayerState>();
		const uint32 NameHash = State != nullptr ? FCrc::StrCrc32(*State->GetPlayerName()) : 0u;
		BotRandom.Initialize(
			static_cast<int32>(NameHash ^ static_cast<uint32>(GameMode != nullptr ? GameMode->RandomSeed : 1)));
		bRandomSeeded = true;
		PawnSensing->OnSeePawn.AddUObject(this, &AShooterAIController::OnSeePawn);
		PawnSensing->OnHearNoise.AddUObject(this, &AShooterAIController::OnHearNoise);
	}
	Enemy = nullptr;
	EnemyLastSeenTime = -1.0f;
	EnemyFirstSeenTime = -1.0f;
	bHasGoal = false;
	NoiseHeardTime = -1.0f;
	bTriggerHeld = false;
}

void AShooterAIController::OnUnPossess()
{
	ReleaseTrigger();
	StopMovement();
	Enemy = nullptr;
	Super::OnUnPossess();
}

AShooterCharacter* AShooterAIController::GetShooterPawn() const
{
	return Cast<AShooterCharacter>(GetPawn());
}

AShooterCharacter* AShooterAIController::GetEnemy() const
{
	return Enemy;
}

AShooterGameMode* AShooterAIController::GetShooterGameMode() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
}

float AShooterAIController::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0f;
}

int32 AShooterAIController::GetTeamIndex() const
{
	const AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterPlayerState* Own = GetPlayerState<AShooterPlayerState>();
	if (GameMode == nullptr || Own == nullptr)
	{
		return 0;
	}
	int32 Index = 0;
	for (const APlayerState* State : GameMode->GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
		if (ShooterState == Own)
		{
			return Index;
		}
		Index += ShooterState != nullptr && ShooterState->GetTeam() == Own->GetTeam() ? 1 : 0;
	}
	return 0;
}

float AShooterAIController::GetCurrentAimError() const
{
	const float Skill = FMath::Max(0.1f, Difficulty);
	const float TimeOnTarget = EnemyFirstSeenTime >= 0.0f ? GetWorldTime() - EnemyFirstSeenTime : 0.0f;
	const float Settle = FMath::Exp(-TimeOnTarget / FMath::Max(0.01f, AimErrorDecayTime));
	return ((AimError * Settle) + MinAimError) / Skill;
}

// The senses

void AShooterAIController::OnSeePawn(APawn* Pawn)
{
	AShooterCharacter* Seen = Cast<AShooterCharacter>(Pawn);
	const AShooterCharacter* Self = GetShooterPawn();
	if (Seen == nullptr || Self == nullptr || !Seen->IsAlive() || Seen->GetTeam() == Self->GetTeam() ||
		Seen->GetTeam() == EShooterTeam::None)
	{
		return;
	}
	const float Now = GetWorldTime();
	// Keep the enemy engaged while it stays in sight; take a nearer one otherwise.
	const bool bCurrentInSight =
		Enemy != nullptr && Enemy->IsAlive() && Now - EnemyLastSeenTime <= BotSensingInterval * 2.0f;
	if (Enemy == Seen)
	{
		EnemyLastSeenTime = Now;
		return;
	}
	if (bCurrentInSight &&
		FVector::DistSquared(Enemy->GetActorLocation(), Self->GetActorLocation()) <=
			FVector::DistSquared(Seen->GetActorLocation(), Self->GetActorLocation()))
	{
		return;
	}
	Enemy = Seen;
	EnemyLastSeenTime = Now;
	EnemyFirstSeenTime = Now;
	BurstShotsFired = 0;
	const float Error = GetCurrentAimError();
	AimOffset = FRotator(BotRandom.FRandRange(-Error, Error), BotRandom.FRandRange(-Error, Error), 0.0f);
}

void AShooterAIController::OnHearNoise(APawn* Instigator, const FVector& Location, float /*Volume*/)
{
	const AShooterCharacter* Heard = Cast<AShooterCharacter>(Instigator);
	const AShooterCharacter* Self = GetShooterPawn();
	if (Heard == nullptr || Self == nullptr || Heard->GetTeam() == Self->GetTeam())
	{
		return;
	}
	Tree.GetBlackboard().SetValueAsVector(NoiseLocationKey, Location);
	NoiseHeardTime = GetWorldTime();
}

void AShooterAIController::UpdateBlackboard()
{
	UBlackboardComponent& Board = Tree.GetBlackboard();
	const AShooterCharacter* Self = GetShooterPawn();
	const AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterGameState* State = GameMode != nullptr ? GameMode->GetShooterGameState() : nullptr;
	const float Now = GetWorldTime();

	// The enemy: alive and seen within EnemyMemory (its last place is searched afterwards).
	if (Enemy != nullptr && (!Enemy->IsAlive() || Enemy->IsPendingKillPending()))
	{
		Enemy = nullptr;
	}
	if (Enemy != nullptr && Now - EnemyLastSeenTime > EnemyMemory)
	{
		Board.SetValueAsVector(NoiseLocationKey, Enemy->GetActorLocation());
		NoiseHeardTime = Now;
		Enemy = nullptr;
	}
	const bool bEnemyInSight = Enemy != nullptr && Now - EnemyLastSeenTime <= BotSensingInterval * 3.0f;
	Board.SetValueAsObject(EnemyKey, Enemy);
	Board.SetValueAsBool(HasEnemyKey, bEnemyInSight);
	if (Enemy != nullptr)
	{
		Board.SetValueAsVector(EnemyLocationKey, Enemy->GetActorLocation());
	}

	const EShooterTeam Team = Self != nullptr ? Self->GetTeam() : EShooterTeam::None;
	const EShooterBombState BombState = State != nullptr ? State->GetBombState() : EShooterBombState::None;
	Board.SetValueAsBool(ShouldDefuseKey, Team == EShooterTeam::CT && BombState == EShooterBombState::Planted);
	Board.SetValueAsBool(CarriesBombKey, Self != nullptr && Self->GetCarriedBomb() != nullptr);
	Board.SetValueAsBool(BombDroppedKey, Team == EShooterTeam::T && BombState == EShooterBombState::Dropped);
	Board.SetValueAsBool(HeardEnemyKey, NoiseHeardTime >= 0.0f && Now - NoiseHeardTime <= NoiseMemory);
}

// The tasks

EBTNodeResult AShooterAIController::TaskIdle()
{
	AShooterCharacter* Self = GetShooterPawn();
	const AShooterGameMode* GameMode = GetShooterGameMode();
	const bool bStopped = GameMode != nullptr && GameMode->bBotStop;
	if (Self != nullptr && Self->IsAlive() && !Self->IsFrozen() && !bStopped)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = TaskName(TEXT("Idle"));
	ReleaseTrigger();
	StandStill();
	if (Self != nullptr && Self->IsAlive() && !bStopped)
	{
		(void)BuyForRound();
	}
	return EBTNodeResult::Succeeded;
}

TArray<FString> AShooterAIController::BuyForRound()
{
	TArray<FString> Bought;
	AShooterCharacter* Self = GetShooterPawn();
	AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterGameState* State = GameMode != nullptr ? GameMode->GetShooterGameState() : nullptr;
	const AShooterPlayerState* PlayerState = GetPlayerState<AShooterPlayerState>();
	if (Self == nullptr || GameMode == nullptr || State == nullptr || PlayerState == nullptr ||
		BoughtInRound == State->GetRoundNumber() || !GameMode->CanBuy(*Self))
	{
		return Bought;
	}
	BoughtInRound = State->GetRoundNumber();
	RoundPurchases.Reset();
	auto TryBuy = [&](const TCHAR* Item)
	{
		if (GameMode->Buy(Self, Item))
		{
			Bought.Add(Item);
		}
	};
	// A primary first (the AWP now and then when there is money for armor too), then armor, then the kit.
	if (Self->GetWeaponInSlot(EShooterWeaponSlot::Primary) == nullptr)
	{
		const int32 AwpPrice = GameMode->GetPrice(*Self, TEXT("awp"));
		const int32 RiflePrice = GameMode->GetPrice(*Self, TEXT("ak47"));
		const bool bAwp = AwpPrice >= 0 && PlayerState->GetMoney() >= AwpPrice + GameMode->VestHelmetPrice &&
			BotRandom.FRand() < AwpChance;
		if (bAwp)
		{
			TryBuy(TEXT("awp"));
		}
		else if (RiflePrice >= 0 && PlayerState->GetMoney() >= RiflePrice)
		{
			TryBuy(TEXT("ak47"));
		}
	}
	if (PlayerState->GetMoney() >= GameMode->VestHelmetPrice)
	{
		TryBuy(TEXT("vesthelm"));
	}
	else if (PlayerState->GetMoney() >= GameMode->VestPrice)
	{
		TryBuy(TEXT("vest"));
	}
	if (Self->GetTeam() == EShooterTeam::CT && PlayerState->GetMoney() >= GameMode->DefuserPrice)
	{
		TryBuy(TEXT("defuser"));
	}
	Self->EquipBestWeapon();
	RoundPurchases = Bought;
	return Bought;
}

EBTNodeResult AShooterAIController::TaskEngage(float DeltaTime)
{
	AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr || Enemy == nullptr)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = TaskName(TEXT("Engage"));
	StandStill();
	Self->StopUse();
	Self->EquipBestWeapon();
	AShooterWeapon* Weapon = Self->GetWeapon();
	if (Weapon == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	const FVector EnemyFeet = Enemy->GetActorLocation();
	const FVector AimPoint =
		EnemyFeet + FVector(0.0f, 0.0f, Enemy->bIsCrouched ? AimHeightCrouched : AimHeightStanding);
	const FVector Eyes = Self->GetFirstPersonCameraComponent()->GetComponentLocation();
	const FRotator Wanted = (AimPoint - Eyes).Rotation() + AimOffset;
	const float AngleLeft = TurnToward(Eyes + (Wanted.Vector() * 1000.0f), DeltaTime);

	// The AWP zooms on a far target before it fires.
	if (AShooterWeapon_Sniper* Sniper = Cast<AShooterWeapon_Sniper>(Weapon))
	{
		if (!Sniper->IsZoomed() && FVector::Dist(EnemyFeet, Self->GetActorLocation()) >= SniperZoomDistance)
		{
			Sniper->SetZoomLevel(1);
		}
	}

	const float Now = GetWorldTime();
	const float Skill = FMath::Max(0.1f, Difficulty);
	const bool bReacted = Now - EnemyFirstSeenTime >= ReactionTime / Skill;
	const bool bOnTarget = AngleLeft <= FireTolerance;
	if (!bReacted || !bOnTarget || Now < BurstPauseEndTime)
	{
		ReleaseTrigger();
		return EBTNodeResult::Running;
	}
	if (Weapon->bAutomatic)
	{
		if (!bTriggerHeld)
		{
			ShotsAtBurstStart = Weapon->GetShotsFired();
			Self->StartWeaponFire();
			bTriggerHeld = true;
		}
		else if (Weapon->GetShotsFired() - ShotsAtBurstStart >= BurstShots)
		{
			// The burst is over: a pause, and a new error for the next one (smaller the longer on target).
			ReleaseTrigger();
			BurstPauseEndTime = Now + BurstPause;
			const float Error = GetCurrentAimError();
			AimOffset = FRotator(BotRandom.FRandRange(-Error, Error), BotRandom.FRandRange(-Error, Error), 0.0f);
		}
	}
	else
	{
		// A press a shot: the trigger comes up between them.
		if (bTriggerHeld)
		{
			ReleaseTrigger();
		}
		else
		{
			Self->StartWeaponFire();
			bTriggerHeld = true;
			const float Error = GetCurrentAimError();
			AimOffset = FRotator(BotRandom.FRandRange(-Error, Error), BotRandom.FRandRange(-Error, Error), 0.0f);
		}
	}
	return EBTNodeResult::Running;
}

EBTNodeResult AShooterAIController::TaskDefuse()
{
	AShooterCharacter* Self = GetShooterPawn();
	const AShooterGameMode* GameMode = GetShooterGameMode();
	AShooterBomb* Bomb = GameMode != nullptr ? GameMode->GetBomb() : nullptr;
	if (Self == nullptr || Bomb == nullptr || Bomb->GetBombState() != EShooterBombState::Planted)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = TaskName(TEXT("Defuse"));
	ReleaseTrigger();
	if (Self->IsDefusing())
	{
		StandStill();
		return EBTNodeResult::Running;
	}
	const FVector BombLocation = Bomb->GetActorLocation();
	if (FVector::DistSquared2D(Self->GetActorLocation(), BombLocation) > FMath::Square(DefuseApproach))
	{
		MoveToGoal(BombLocation);
		return EBTNodeResult::Running;
	}
	StandStill();
	// Another CT may be at it already: this one guards (StartUse fails).
	(void)Self->StartUse();
	return EBTNodeResult::Running;
}

EBTNodeResult AShooterAIController::TaskPlant()
{
	AShooterCharacter* Self = GetShooterPawn();
	AShooterGameMode* GameMode = GetShooterGameMode();
	if (Self == nullptr || GameMode == nullptr || Self->GetCarriedBomb() == nullptr)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = TaskName(TEXT("Plant"));
	ReleaseTrigger();
	if (Self->IsPlanting())
	{
		StandStill();
		return EBTNodeResult::Running;
	}
	FVector SiteLocation;
	const FName Site = GameMode->GetTerroristTargetSite();
	if (!GameMode->GetBombSiteLocation(Site, SiteLocation))
	{
		return EBTNodeResult::Failed;
	}
	// Inside the site and near its middle: stop, then plant once still.
	if (Self->GetBombSiteHere() != NAME_None &&
		FVector::DistSquared2D(Self->GetActorLocation(), SiteLocation) <= FMath::Square(GoalReachedDistance * 2.0f))
	{
		StandStill();
		(void)Self->StartUse();
		return EBTNodeResult::Running;
	}
	MoveToGoal(SiteLocation);
	return EBTNodeResult::Running;
}

EBTNodeResult AShooterAIController::TaskFetchBomb()
{
	const AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterBomb* Bomb = GameMode != nullptr ? GameMode->GetBomb() : nullptr;
	if (Bomb == nullptr || Bomb->GetBombState() != EShooterBombState::Dropped)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = TaskName(TEXT("FetchBomb"));
	ReleaseTrigger();
	MoveToGoal(Bomb->GetActorLocation());
	return EBTNodeResult::Running;
}

EBTNodeResult AShooterAIController::TaskInvestigate()
{
	const AShooterCharacter* Self = GetShooterPawn();
	UBlackboardComponent& Board = Tree.GetBlackboard();
	if (Self == nullptr || !Board.IsValueSet(NoiseLocationKey))
	{
		return EBTNodeResult::Failed;
	}
	const FVector Noise = Board.GetValueAsVector(NoiseLocationKey);
	if (FVector::DistSquared2D(Self->GetActorLocation(), Noise) <= FMath::Square(GoalReachedDistance))
	{
		// Nothing here: forget it.
		NoiseHeardTime = -1.0f;
		Board.ClearValue(NoiseLocationKey);
		return EBTNodeResult::Failed;
	}
	CurrentTask = TaskName(TEXT("Investigate"));
	ReleaseTrigger();
	MoveToGoal(Noise);
	return EBTNodeResult::Running;
}

EBTNodeResult AShooterAIController::TaskObjective(float DeltaTime)
{
	const AShooterCharacter* Self = GetShooterPawn();
	const AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterGameState* State = GameMode != nullptr ? GameMode->GetShooterGameState() : nullptr;
	if (Self == nullptr || GameMode == nullptr || State == nullptr)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = TaskName(TEXT("Objective"));
	ReleaseTrigger();
	FVector Goal = Self->GetActorLocation();
	const AShooterBomb* Bomb = GameMode->GetBomb();
	if (State->GetBombState() == EShooterBombState::Planted && Bomb != nullptr)
	{
		// Both teams go to the bomb: the T to guard it, the CT to retake the site.
		Goal = Bomb->GetActorLocation();
	}
	else
	{
		const TArray<FName> Sites = GameMode->GetBombSiteNames();
		const FName Site = Self->GetTeam() == EShooterTeam::T
			? GameMode->GetTerroristTargetSite()
			: (Sites.Num() > 0 ? Sites[GetTeamIndex() % Sites.Num()] : NAME_None);
		if (!GameMode->GetBombSiteLocation(Site, Goal))
		{
			StandStill();
			return EBTNodeResult::Succeeded;
		}
	}
	Tree.GetBlackboard().SetValueAsVector(GoalKey, Goal);
	if (FVector::DistSquared2D(Self->GetActorLocation(), Goal) <= FMath::Square(GoalReachedDistance))
	{
		// There: hold, looking around slowly.
		StandStill();
		FRotator Look = GetControlRotation();
		constexpr float LookAroundRate = 30.0f;
		Look.Yaw += LookAroundRate * DeltaTime;
		Look.Pitch = 0.0f;
		SetControlRotation(Look);
		return EBTNodeResult::Succeeded;
	}
	MoveToGoal(Goal);
	return EBTNodeResult::Running;
}

// Helpers

void AShooterAIController::MoveToGoal(const FVector& Goal)
{
	if (bHasGoal && HasMoveTarget() && FVector::DistSquared(Goal, CurrentGoal) <= FMath::Square(GoalRepathDistance))
	{
		return;
	}
	CurrentGoal = Goal;
	bHasGoal = true;
	MoveToLocation(Goal);
}

void AShooterAIController::StandStill()
{
	if (HasMoveTarget())
	{
		StopMovement();
	}
	bHasGoal = false;
	ClearWishDirection();
}

void AShooterAIController::ReleaseTrigger()
{
	if (bTriggerHeld)
	{
		if (AShooterCharacter* Self = GetShooterPawn())
		{
			Self->StopWeaponFire();
		}
		bTriggerHeld = false;
	}
}

float AShooterAIController::TurnToward(const FVector& Target, float DeltaTime)
{
	const AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr)
	{
		return 180.0f;
	}
	const FVector Eyes = Self->GetFirstPersonCameraComponent()->GetComponentLocation();
	const FRotator Wanted = (Target - Eyes).Rotation();
	FRotator Current = GetControlRotation();
	const float MaxStep = AimTurnRate * FMath::Max(0.1f, Difficulty) * DeltaTime;
	const float YawDelta = FRotator::NormalizeAxis(Wanted.Yaw - Current.Yaw);
	const float PitchDelta = FRotator::NormalizeAxis(Wanted.Pitch - Current.Pitch);
	Current.Yaw += FMath::Clamp(YawDelta, -MaxStep, MaxStep);
	Current.Pitch += FMath::Clamp(PitchDelta, -MaxStep, MaxStep);
	Current.Pitch = FMath::Clamp(Current.Pitch, -89.0f, 89.0f);
	SetControlRotation(Current);
	return FMath::Max(FMath::Abs(FRotator::NormalizeAxis(Wanted.Yaw - Current.Yaw)),
		FMath::Abs(FRotator::NormalizeAxis(Wanted.Pitch - Current.Pitch)));
}

void AShooterAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr)
	{
		return;
	}
	UpdateBlackboard();
	(void)Tree.Tick(DeltaSeconds);
	// The steering along the path (AAIController's path following); a pawn that stands has no target.
	const AShooterGameMode* GameMode = GetShooterGameMode();
	if (Self->IsAlive() && !Self->IsFrozen() && (GameMode == nullptr || !GameMode->bBotStop))
	{
		(void)TickAI(DeltaSeconds);
	}
}
