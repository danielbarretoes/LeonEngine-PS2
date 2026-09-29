#include "ShooterAIController.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterCharacterMovement.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPawnSensingComponent.h"
#include "ShooterPlayerState.h"
#include "Stats/Stats.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_AWP.h"
#include "Weapons/ShooterWeapon_Instant.h"
#include "Weapons/ShooterWeapon_Projectile.h"

DECLARE_CYCLE_STAT(TEXT("Bot Tick"), STAT_ShooterBotTick, STATGROUP_Game);

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
	constexpr float BotHearingThreshold = 2500.0f;
	constexpr float BotLOSHearingThreshold = 5000.0f;

	/** The grenades: how often a throw is considered, s; a spot this near the last one gets no new draw, cm. */
	constexpr float GrenadeCheckInterval = 0.25f;
	constexpr float GrenadeSpotSpacing = 500.0f;
	/** The nearest a flashbang or a smoke grenade is thrown on the way in, cm (the HE keeps MinGrenadeDistance). */
	constexpr float MinFlashDistance = 500.0f;
	/** The first part of a throw that must be clear of walls, cm (else it would bounce back at the thrower). */
	constexpr float ThrowClearance = 300.0f;
	/** The aim a throw waits for, degrees; a throw not out after this long is given up, s. */
	constexpr float ThrowAimTolerance = 1.0f;
	constexpr float ThrowTimeout = 3.0f;
	/** Seconds past its flashbang's fuse the thrower keeps its back to it. */
	constexpr float FlashTurnAwayMargin = 0.2f;

	/** The blind fire's pitch stays within this, degrees. */
	constexpr float BlindFireMaxPitch = 45.0f;

	/** The tree's branches (GetCurrentTask), named once. */
	const FName IdleTaskName(TEXT("Idle"));
	const FName BlindTaskName(TEXT("Blind"));
	const FName EngageTaskName(TEXT("Engage"));
	const FName ThrowGrenadeTaskName(TEXT("ThrowGrenade"));
	const FName DefuseTaskName(TEXT("Defuse"));
	const FName PlantTaskName(TEXT("Plant"));
	const FName FetchBombTaskName(TEXT("FetchBomb"));
	const FName EscortTaskName(TEXT("Escort"));
	const FName InvestigateTaskName(TEXT("Investigate"));
	const FName HuntTaskName(TEXT("Hunt"));
	const FName ObjectiveTaskName(TEXT("Objective"));

} // namespace

const FName AShooterAIController::EnemyKey(TEXT("Enemy"));
const FName AShooterAIController::HasEnemyKey(TEXT("HasEnemy"));
const FName AShooterAIController::ShouldDefuseKey(TEXT("ShouldDefuse"));
const FName AShooterAIController::CarriesBombKey(TEXT("CarriesBomb"));
const FName AShooterAIController::BombDroppedKey(TEXT("BombDropped"));
const FName AShooterAIController::HeardEnemyKey(TEXT("HeardEnemy"));
const FName AShooterAIController::NoiseLocationKey(TEXT("NoiseLocation"));
const FName AShooterAIController::ShouldEscortKey(TEXT("ShouldEscort"));
const FName AShooterAIController::ShouldHuntKey(TEXT("ShouldHunt"));
const FName AShooterAIController::IsBlindKey(TEXT("IsBlind"));
const FName AShooterAIController::IsThrowingKey(TEXT("IsThrowing"));

AShooterAIController::AShooterAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bot has a player state like a player (UE: bWantsPlayerState): its team, its score and its money.
	bWantsPlayerState = true;
	// The bot thinks in its tick, before its pawn's (AController::AddPawnTickDependency).
	PrimaryActorTick.bCanEverTick = true;
	PawnSensing = CreateDefaultSubobject<UShooterPawnSensingComponent>(TEXT("PawnSensing"));
	PawnSensing->bOnlySensePlayers = false;
	PawnSensing->SightRadius = BotSightRadius;
	PawnSensing->SetPeripheralVisionAngle(BotVisionHalfAngle);
	PawnSensing->SensingInterval = SensingInterval;
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
	UBTNode* Blind = Sequence({Decorator(IsBlindKey),
		Action([this](UBlackboardComponent&, float DeltaTime) { return TaskBlind(DeltaTime); })});
	UBTNode* Engage = Sequence({Decorator(HasEnemyKey),
		Action([this](UBlackboardComponent&, float DeltaTime) { return TaskEngage(DeltaTime); })});
	UBTNode* Throw = Sequence({Decorator(IsThrowingKey),
		Action([this](UBlackboardComponent&, float DeltaTime) { return TaskThrowGrenade(DeltaTime); })});
	UBTNode* Defuse =
		Sequence({Decorator(ShouldDefuseKey), Action([this](UBlackboardComponent&, float) { return TaskDefuse(); })});
	UBTNode* Plant =
		Sequence({Decorator(CarriesBombKey), Action([this](UBlackboardComponent&, float) { return TaskPlant(); })});
	UBTNode* Fetch =
		Sequence({Decorator(BombDroppedKey), Action([this](UBlackboardComponent&, float) { return TaskFetchBomb(); })});
	UBTNode* Escort = Sequence({Decorator(ShouldEscortKey),
		Action([this](UBlackboardComponent&, float DeltaTime) { return TaskEscort(DeltaTime); })});
	UBTNode* Investigate = Sequence(
		{Decorator(HeardEnemyKey), Action([this](UBlackboardComponent&, float) { return TaskInvestigate(); })});
	UBTNode* Hunt = Sequence({Decorator(ShouldHuntKey),
		Action([this](UBlackboardComponent&, float DeltaTime) { return TaskHunt(DeltaTime); })});
	UBTNode* Objective = Action([this](UBlackboardComponent&, float DeltaTime) { return TaskObjective(DeltaTime); });
	TreeNodes.Add(MakeUnique<UBTComposite_Selector>(
		TArray<UBTNode*>{Idle, Blind, Throw, Engage, Defuse, Plant, Fetch, Escort, Investigate, Hunt, Objective}));
	Tree.SetRoot(TreeNodes.Last().Get());
}

void AShooterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!bRandomSeeded)
	{
		// The bot's stream: the match's seed and the bot's index (the same bots replay the same choices, whatever their
		// names).
		const AShooterGameMode* GameMode = GetShooterGameMode();
		const uint32 IndexHash = FCrc::MemCrc32(&BotIndex, static_cast<int32>(sizeof(BotIndex)));
		BotRandom.Initialize(
			static_cast<int32>(IndexHash ^ static_cast<uint32>(GameMode != nullptr ? GameMode->RandomSeed : 1)));
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
	StrafeDirection = 0.0f;
	bStrafingThisTick = false;
	bStrafedLastTick = false;
	bCombatCrouch = false;
	EndThrow();
	// The bots do not climb ladders yet: they walk through a ladder's volume (ps2-shipping N30c).
	const AShooterCharacter* Shooter = Cast<AShooterCharacter>(InPawn);
	if (UShooterCharacterMovement* Move = Shooter != nullptr ? Shooter->GetShooterCharacterMovement() : nullptr)
	{
		Move->bCanClimbLadders = false;
	}
}

void AShooterAIController::OnUnPossess()
{
	ReleaseTrigger();
	StopMovement();
	EndThrow();
	if (AShooterCharacter* Self = GetShooterPawn())
	{
		Self->SetWalking(false);
	}
	Enemy = nullptr;
	Super::OnUnPossess();
}

void AShooterAIController::SetSensingSlot(int32 Slot, int32 NumSlots)
{
	const int32 Slots = FMath::Max(1, NumSlots);
	const int32 SlotIndex = ((Slot % Slots) + Slots) % Slots;
	PawnSensing->SetTimer(SensingInterval * static_cast<float>(SlotIndex) / static_cast<float>(Slots));
}

AShooterCharacter* AShooterAIController::GetShooterPawn() const
{
	return Cast<AShooterCharacter>(GetPawn());
}

AShooterCharacter* AShooterAIController::GetEnemy() const
{
	// UE ShooterGame: the blackboard's Enemy key (the engaged enemy the tree sees).
	return Cast<AShooterCharacter>(Tree.GetBlackboard().GetValueAsObject(EnemyKey));
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

float AShooterAIController::GetRecoilCompensation() const
{
	return FMath::Clamp(RecoilCompensation * FMath::Max(0.1f, Difficulty), 0.0f, 1.0f);
}

float AShooterAIController::GetCurrentAimError() const
{
	const float Skill = FMath::Max(0.1f, Difficulty);
	const float TimeOnTarget = EnemyFirstSeenTime >= 0.0f ? GetWorldTime() - EnemyFirstSeenTime : 0.0f;
	const float Settle = FMath::Exp(-TimeOnTarget / FMath::Max(0.01f, AimErrorDecayTime));
	return ((AimError * Settle) + MinAimError) / Skill;
}

// The senses

void AShooterAIController::OnSeePawn(APawn* SeenPawn)
{
	AShooterCharacter* Seen = Cast<AShooterCharacter>(SeenPawn);
	const AShooterCharacter* Self = GetShooterPawn();
	// Frozen, the bot sees nobody: the reaction and the aim's settling start when the round does.
	if (Seen == nullptr || Self == nullptr || Self->IsFrozen() || !Seen->IsAlive() ||
		Seen->GetTeam() == Self->GetTeam() || Seen->GetTeam() == EShooterTeam::None)
	{
		return;
	}
	const float Now = GetWorldTime();
	// Keep the enemy engaged while it stays in sight; take a nearer one otherwise.
	const bool bCurrentInSight =
		Enemy != nullptr && Enemy->IsAlive() && Now - EnemyLastSeenTime <= SensingInterval * 2.0f;
	if (Enemy == Seen)
	{
		EnemyLastSeenTime = Now;
		EnemyLastSeenLocation = Seen->GetActorLocation();
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
	EnemyLastSeenLocation = Seen->GetActorLocation();
	EnemyFirstSeenTime = Now;
	BurstShotsFired = 0;
	// A new engagement draws its first strafe.
	StrafeDirection = 0.0f;
	const float Error = GetCurrentAimError();
	AimOffset = FRotator(BotRandom.FRandRange(-Error, Error), BotRandom.FRandRange(-Error, Error), 0.0f);
	// CS's bots call it (unless a teammate just did), with where the enemy stands.
	(void)SayOnRadio(EShooterRadioMessage::EnemySpotted, &EnemyLastSeenLocation);
}

void AShooterAIController::OnHearNoise(APawn* NoiseInstigator, const FVector& Location, float /*Volume*/)
{
	const AShooterCharacter* Heard = Cast<AShooterCharacter>(NoiseInstigator);
	const AShooterCharacter* Self = GetShooterPawn();
	if (Heard == nullptr || Self == nullptr || Heard->GetTeam() == Self->GetTeam())
	{
		return;
	}
	LookAt(Location);
}

void AShooterAIController::LookAt(const FVector& Location)
{
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

	// A new round: last round's enemy, noise, goal and throw are forgotten (the survivors start over too); the round's
	// entry grenade is drawn.
	if (State != nullptr && State->GetRoundSerial() != ObservedRoundSerial)
	{
		ObservedRoundSerial = State->GetRoundSerial();
		Enemy = nullptr;
		NoiseHeardTime = -1.0f;
		Board.ClearValue(NoiseLocationKey);
		bHasGoal = false;
		SiteRotation = 0;
		HoldingSinceTime = -1.0f;
		EndThrow();
		NextGrenadeTime = 0.0f;
		NextGrenadeCheckTime = 0.0f;
		bUsesEntryGrenade = BotRandom.FRand() < GrenadeChance;
		bEntryGrenadeDone = false;
		LastGrenadeSpot = FVector(1.0e9f, 1.0e9f, 1.0e9f);
		bAskedForBackup = false;
		StrafeDirection = 0.0f;
		NextBlindAimTime = 0.0f;
	}

	// The enemy: alive and seen within EnemyMemory (its last place is searched afterwards).
	if (Enemy != nullptr && (!Enemy->IsAlive() || Enemy->IsPendingKillPending()))
	{
		Enemy = nullptr;
	}
	if (Enemy != nullptr && Now - EnemyLastSeenTime > EnemyMemory)
	{
		// Where it was last seen, not where it is now (behind a wall).
		LookAt(EnemyLastSeenLocation);
		Enemy = nullptr;
	}
	const bool bEnemyInSight = Enemy != nullptr && Now - EnemyLastSeenTime <= SensingInterval * 3.0f;
	Board.SetValueAsObject(EnemyKey, Enemy);
	Board.SetValueAsBool(HasEnemyKey, bEnemyInSight);
	// Blinded by a flashbang: it sees nobody (its sensing), and fires blindly until it sees again.
	Board.SetValueAsBool(IsBlindKey, Self != nullptr && Self->IsAlive() && Self->IsBlind());

	const EShooterTeam Team = Self != nullptr ? Self->GetTeam() : EShooterTeam::None;
	const EShooterBombState BombState = State != nullptr ? State->GetBombState() : EShooterBombState::None;
	Board.SetValueAsBool(ShouldDefuseKey, Team == EShooterTeam::CT && BombState == EShooterBombState::Planted);
	Board.SetValueAsBool(CarriesBombKey, Self != nullptr && Self->GetCarriedBomb() != nullptr);
	Board.SetValueAsBool(BombDroppedKey, Team == EShooterTeam::T && BombState == EShooterBombState::Dropped);
	Board.SetValueAsBool(HeardEnemyKey, NoiseHeardTime >= 0.0f && Now - NoiseHeardTime <= NoiseMemory);

	// Escort: a terrorist without the bomb stays with the teammate carrying it.
	const AShooterBomb* Bomb = GameMode != nullptr ? GameMode->GetBomb() : nullptr;
	const AShooterCharacter* Carrier = Bomb != nullptr ? Bomb->GetCarrier() : nullptr;
	Board.SetValueAsBool(ShouldEscortKey,
		Team == EShooterTeam::T && Carrier != nullptr && Carrier != Self && Carrier->IsAlive() &&
			BombState == EShooterBombState::Carried);
	// Hunt: outnumbering the enemy (by HuntAdvantage) with no bomb to go for, the bot goes after them (the game mode
	// counts the living once a frame for every bot).
	const EShooterTeam EnemyTeam = Team == EShooterTeam::CT ? EShooterTeam::T : EShooterTeam::CT;
	const int32 EnemiesAlive = GameMode != nullptr ? GameMode->CountAlive(EnemyTeam) : 0;
	Board.SetValueAsBool(ShouldHuntKey,
		HuntAdvantage > 0 && Team != EShooterTeam::None && EnemiesAlive > 0 &&
			BombState != EShooterBombState::Planted && GameMode->CountAlive(Team) >= EnemiesAlive + HuntAdvantage);

	// A grenade: with nobody in sight, a throw may start. A throw under way is finished before the fight (CS's bots
	// throw anyway); turned away from its flashbang, the bot fights an enemy that shows up.
	if (!bEnemyInSight)
	{
		ConsiderGrenade();
	}
	Board.SetValueAsBool(IsThrowingKey, bThrowing && (!bThrown || !bEnemyInSight));
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
	CurrentTask = IdleTaskName;
	ReleaseTrigger();
	StandStill();
	if (Self != nullptr && Self->IsAlive() && !bStopped)
	{
		(void)BuyForRound();
	}
	return EBTNodeResult::Succeeded;
}

EBTNodeResult AShooterAIController::TaskBlind(float DeltaTime)
{
	AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = BlindTaskName;
	if (bThrowing && !bThrown)
	{
		EndThrow();
	}
	StandStill();
	// CS's blinded bots fire at random: around where the enemy was last seen, else around the view.
	Self->EquipBestWeapon();
	AShooterWeapon* Weapon = Self->GetWeapon();
	if (Weapon == nullptr ||
		(Weapon->Slot != EShooterWeaponSlot::Primary && Weapon->Slot != EShooterWeaponSlot::Secondary))
	{
		ReleaseTrigger();
		return EBTNodeResult::Succeeded;
	}
	const float Now = GetWorldTime();
	if (Now >= NextBlindAimTime)
	{
		FRotator Base(0.0f, GetControlRotation().Yaw, 0.0f);
		if (EnemyLastSeenTime >= 0.0f && Now - EnemyLastSeenTime <= EnemyMemory * 2.0f)
		{
			const FVector Eyes = Self->GetFirstPersonCameraComponent()->GetComponentLocation();
			Base = (EnemyLastSeenLocation + FVector(0.0f, 0.0f, AimHeightStanding) - Eyes).Rotation();
		}
		BlindAim = Base +
			FRotator(BotRandom.FRandRange(-0.25f, 0.25f) * BlindFireError,
				BotRandom.FRandRange(-1.0f, 1.0f) * BlindFireError, 0.0f);
		BlindAim.Pitch = FMath::Clamp(BlindAim.Pitch, -BlindFireMaxPitch, BlindFireMaxPitch);
		bBlindFiring = BotRandom.FRand() < BlindFireChance;
		NextBlindAimTime = Now + BotRandom.FRandRange(BlindFireMinTime, BlindFireMaxTime);
		// Each point is a new press.
		ReleaseTrigger();
	}
	(void)AimToward(BlindAim, *Weapon, DeltaTime);
	if (!bBlindFiring || Self->IsPlanting() || Self->IsDefusing())
	{
		ReleaseTrigger();
		return EBTNodeResult::Succeeded;
	}
	if (Weapon->bAutomatic)
	{
		if (!bTriggerHeld)
		{
			Self->StartWeaponFire();
			bTriggerHeld = true;
		}
	}
	else if (bTriggerHeld)
	{
		ReleaseTrigger();
	}
	else
	{
		Self->StartWeaponFire();
		bTriggerHeld = true;
	}
	return EBTNodeResult::Running;
}

TArray<FString> AShooterAIController::BuyForRound()
{
	TArray<FString> Bought;
	AShooterCharacter* Self = GetShooterPawn();
	AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterGameState* State = GameMode != nullptr ? GameMode->GetShooterGameState() : nullptr;
	const AShooterPlayerState* BotState = GetPlayerState<AShooterPlayerState>();
	if (Self == nullptr || GameMode == nullptr || State == nullptr || BotState == nullptr ||
		BoughtInRound == State->GetRoundSerial() || !GameMode->CanBuy(*Self))
	{
		return Bought;
	}
	BoughtInRound = State->GetRoundSerial();
	auto TryBuy = [&](const TCHAR* Item)
	{
		if (GameMode->Buy(Self, Item))
		{
			Bought.Add(Item);
		}
	};
	// The team's plan (CS's economy): on an eco only a bot that can afford the full buy spends.
	const EShooterBuyPlan Plan = GameMode->GetTeamBuyPlan(Self->GetTeam());
	if (Plan == EShooterBuyPlan::Eco && BotState->GetMoney() < GameMode->GetFullBuyCost(*Self))
	{
		return Bought;
	}
	// A primary first, with the money for kevlar too (the armor keeps it alive): the AWP now and then (with a helmet),
	// else the team's rifle (CS's AK-47 for the terrorists, M4A1 for the counter-terrorists); short of it (a
	// force-buy), an MP5, else a Desert Eagle (CS's bots).
	if (Self->GetWeaponInSlot(EShooterWeaponSlot::Primary) == nullptr)
	{
		const TCHAR* Rifle = Self->GetTeam() == EShooterTeam::T ? TEXT("ak47") : TEXT("m4a1");
		const int32 AwpPrice = GameMode->GetPrice(*Self, TEXT("awp"));
		const int32 RiflePrice = GameMode->GetPrice(*Self, Rifle);
		const int32 SmgPrice = GameMode->GetPrice(*Self, TEXT("mp5"));
		const int32 PistolPrice = GameMode->GetPrice(*Self, TEXT("deagle"));
		const int32 Money = BotState->GetMoney();
		const int32 Armor = GameMode->VestPrice;
		const bool bAwp =
			AwpPrice >= 0 && Money >= AwpPrice + GameMode->VestHelmetPrice && BotRandom.FRand() < AwpChance;
		if (bAwp)
		{
			TryBuy(TEXT("awp"));
		}
		else if (RiflePrice >= 0 && Money >= RiflePrice + Armor)
		{
			TryBuy(Rifle);
		}
		else if (SmgPrice >= 0 && Money >= SmgPrice + Armor)
		{
			TryBuy(TEXT("mp5"));
		}
		else if (PistolPrice >= 0 && Money >= PistolPrice + Armor)
		{
			TryBuy(TEXT("deagle"));
		}
	}
	// A bought weapon comes with its clip only (CS 1.6): the boxes of its ammunition until the reserve is full.
	for (int32 Price = GameMode->GetPrice(*Self, TEXT("primammo")); Price >= 0 && BotState->GetMoney() >= Price;
		Price = GameMode->GetPrice(*Self, TEXT("primammo")))
	{
		const int32 Before = BotState->GetMoney();
		TryBuy(TEXT("primammo"));
		if (BotState->GetMoney() == Before)
		{
			break;
		}
	}
	// Kevlar and a helmet (the helmet alone on full kevlar), else kevlar.
	const int32 HelmetPrice = GameMode->GetPrice(*Self, TEXT("vesthelm"));
	if (HelmetPrice >= 0 && BotState->GetMoney() >= HelmetPrice)
	{
		TryBuy(TEXT("vesthelm"));
	}
	else if (BotState->GetMoney() >= GameMode->VestPrice)
	{
		TryBuy(TEXT("vest"));
	}
	if (Self->GetTeam() == EShooterTeam::CT && BotState->GetMoney() >= GameMode->DefuserPrice)
	{
		TryBuy(TEXT("defuser"));
	}
	// The grenades with what is left, in their order (GetPrice refuses one more than CS lets a player carry).
	for (const FString& Item : GrenadeBuyOrder)
	{
		const int32 Price = GameMode->GetPrice(*Self, Item);
		if (Price >= 0 && BotState->GetMoney() >= Price)
		{
			TryBuy(*Item);
		}
	}
	Self->EquipBestWeapon();
	return Bought;
}

EBTNodeResult AShooterAIController::TaskEngage(float DeltaTime)
{
	AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr || Enemy == nullptr)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = EngageTaskName;
	// A throw not out yet gives way to the fight (a thrown one's turn away ends too).
	if (bThrowing)
	{
		EndThrow();
	}
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
	const float AngleLeft = AimToward(Wanted, *Weapon, DeltaTime);

	// CS's bots move as they fight: a rifle at range crouches and stops, the AWP stands still, the rest strafe.
	const float Distance = FVector::Dist(EnemyFeet, Self->GetActorLocation());
	const bool bSniper = Weapon->IsA<AShooterWeapon_AWP>();
	const bool bRifle = Weapon->IsA<AShooterWeapon_AK47>() || Weapon->IsA<AShooterWeapon_M4A1>();
	if (bRifle && Distance >= CrouchFireDistance)
	{
		if (!bCombatCrouch)
		{
			Self->Crouch();
			bCombatCrouch = true;
		}
	}
	else
	{
		EndCombatCrouch();
		if (!bSniper)
		{
			UpdateStrafe(EnemyFeet);
		}
	}

	// The AWP zooms on a far target before it fires.
	if (AShooterWeapon_AWP* Sniper = Cast<AShooterWeapon_AWP>(Weapon))
	{
		if (!Sniper->IsZoomed() && Distance >= SniperZoomDistance)
		{
			Sniper->SetZoomLevel(1);
		}
	}

	// Hurt badly: the team hears it once a round (CS's "Need backup."), as soon as the radio lets it.
	if (!bAskedForBackup && Self->GetHealth() < NeedBackupHealth)
	{
		bAskedForBackup = SayOnRadio(EShooterRadioMessage::NeedBackup);
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

void AShooterAIController::UpdateStrafe(const FVector& EnemyFeet)
{
	const AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr)
	{
		return;
	}
	// Left and right across the line to the enemy, each way for a time from the stream (the first way drawn too).
	const float Now = GetWorldTime();
	if (StrafeDirection == 0.0f || Now >= NextStrafeChangeTime)
	{
		StrafeDirection = StrafeDirection == 0.0f ? (BotRandom.FRand() < 0.5f ? -1.0f : 1.0f) : -StrafeDirection;
		NextStrafeChangeTime = Now + BotRandom.FRandRange(StrafeMinTime, StrafeMaxTime);
	}
	const FVector ToEnemy = (EnemyFeet - Self->GetActorLocation()).GetSafeNormal2D();
	// The right of the line (UE's axes: +Y is the right of +X).
	StrafeWish = FVector(-ToEnemy.Y, ToEnemy.X, 0.0f) * StrafeDirection;
	bStrafingThisTick = !ToEnemy.IsNearlyZero();
}

void AShooterAIController::EndCombatCrouch()
{
	if (!bCombatCrouch)
	{
		return;
	}
	bCombatCrouch = false;
	if (AShooterCharacter* Self = GetShooterPawn())
	{
		Self->UnCrouch();
	}
}

// Grenades

float AShooterAIController::ComputeThrowPitch(float Horizontal, float Height, float Speed, float Gravity)
{
	// The low arc of the ballistic throw: tan(pitch) = (v^2 - sqrt(v^4 - g (g x^2 + 2 y v^2))) / (g x).
	constexpr float OutOfReachPitch = 45.0f;
	const float SpeedSquared = Speed * Speed;
	const float Discriminant = (SpeedSquared * SpeedSquared) -
		(Gravity * ((Gravity * Horizontal * Horizontal) + (2.0f * Height * SpeedSquared)));
	if (Horizontal < 1.0f || Gravity <= 0.0f || Discriminant < 0.0f)
	{
		return OutOfReachPitch;
	}
	return FMath::RadiansToDegrees(FMath::Atan((SpeedSquared - FMath::Sqrt(Discriminant)) / (Gravity * Horizontal)));
}

AShooterWeapon_Projectile* AShooterAIController::FindGrenade(const UClass* GrenadeClass) const
{
	const AShooterCharacter* Self = GetShooterPawn();
	return Self != nullptr ? Cast<AShooterWeapon_Projectile>(Self->FindWeaponOfClass(GrenadeClass)) : nullptr;
}

bool AShooterAIController::ThrowGrenadeAt(AShooterWeapon_Projectile* Grenade, const FVector& TargetLocation)
{
	AShooterCharacter* Self = GetShooterPawn();
	UWorld* World = GetWorld();
	if (Self == nullptr || World == nullptr || Grenade == nullptr || bThrowing || !Self->IsAlive() ||
		!Self->GetInventory().Contains(Grenade))
	{
		return false;
	}
	// The aim point, off by the throw's error; the pitch of the low arc there, less the weapon's own tilt.
	const FVector Aimed = TargetLocation +
		FVector(BotRandom.FRandRange(-1.0f, 1.0f) * GrenadeThrowError,
			BotRandom.FRandRange(-1.0f, 1.0f) * GrenadeThrowError, 0.0f);
	const FVector Eyes = Self->GetFirstPersonCameraComponent()->GetComponentLocation();
	const FVector Delta = Aimed - Eyes;
	const float Gravity = FMath::Max(1.0f, -World->GetGravityZ());
	const float Pitch = ComputeThrowPitch(Delta.Size2D(), Delta.Z, Grenade->ThrowSpeed, Gravity);
	const float Yaw = Delta.Rotation().Yaw;
	// Not into a wall at its nose: it would bounce back.
	FCollisionQueryParams Params(FName(TEXT("BotGrenadeThrow")), false, Self);
	FHitResult Hit;
	const FVector ThrowDirection = FRotator(Pitch, Yaw, 0.0f).Vector();
	if (UGameplayStatics::LineTraceSingleByChannel(
			*World, Hit, Eyes, Eyes + (ThrowDirection * ThrowClearance), ECC_Visibility, Params))
	{
		return false;
	}
	ThrowingGrenade = Grenade;
	ThrowTarget = Aimed;
	ThrowRotation = FRotator(FMath::Clamp(Pitch - Grenade->ThrowPitch, -89.0f, 89.0f), Yaw, 0.0f);
	ThrowStartTime = GetWorldTime();
	TurnAwayEndTime = -1.0f;
	bThrowing = true;
	bThrown = false;
	Tree.GetBlackboard().SetValueAsBool(IsThrowingKey, true);
	return true;
}

void AShooterAIController::EndThrow()
{
	bThrowing = false;
	bThrown = false;
	ThrowingGrenade.Reset();
	TurnAwayEndTime = -1.0f;
}

void AShooterAIController::ConsiderGrenade()
{
	AShooterCharacter* Self = GetShooterPawn();
	const AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterGameState* State = GameMode != nullptr ? GameMode->GetShooterGameState() : nullptr;
	const float Now = GetWorldTime();
	if (bThrowing || Self == nullptr || State == nullptr || Now < NextGrenadeTime || Now < NextGrenadeCheckTime ||
		!Self->IsAlive() || Self->IsFrozen() || Self->IsBlind() || Self->IsPlanting() || Self->IsDefusing() ||
		GameMode->bBotStop || State->GetRoundState() != EShooterRoundState::Live)
	{
		return;
	}
	NextGrenadeCheckTime = Now + GrenadeCheckInterval;
	AShooterWeapon_Projectile* HEGrenade = FindGrenade(AShooterWeapon_HEGrenade::StaticClass());
	AShooterWeapon_Projectile* Flashbang = FindGrenade(AShooterWeapon_Flashbang::StaticClass());
	AShooterWeapon_Projectile* Smoke = FindGrenade(AShooterWeapon_SmokeGrenade::StaticClass());
	if (HEGrenade == nullptr && Flashbang == nullptr && Smoke == nullptr)
	{
		return;
	}
	const FVector Feet = Self->GetActorLocation();

	// The way in, once a round: a terrorist nearing the round's site flashes it (else smokes it), a counter-terrorist
	// nearing the planted bomb flashes it (else sends the HE).
	if (bUsesEntryGrenade && !bEntryGrenadeDone)
	{
		FVector Entry = FVector::ZeroVector;
		AShooterWeapon_Projectile* EntryGrenade = nullptr;
		const EShooterBombState BombState = State->GetBombState();
		const AShooterBomb* Bomb = GameMode->GetBomb();
		if (Self->GetTeam() == EShooterTeam::T && BombState != EShooterBombState::Planted &&
			GameMode->GetBombSiteLocation(GameMode->GetTerroristTargetSite(), Entry))
		{
			EntryGrenade = Flashbang != nullptr ? Flashbang : Smoke;
		}
		else if (Self->GetTeam() == EShooterTeam::CT && BombState == EShooterBombState::Planted && Bomb != nullptr)
		{
			Entry = Bomb->GetActorLocation();
			EntryGrenade = Flashbang != nullptr ? Flashbang : HEGrenade;
		}
		// A flashbang or a smoke hurts nobody: it may go nearer than the HE.
		const float EntryDistance = FVector::Dist2D(Feet, Entry);
		const float NearestEntry = EntryGrenade != nullptr && EntryGrenade->IsA<AShooterWeapon_HEGrenade>()
			? MinGrenadeDistance
			: MinFlashDistance;
		if (EntryGrenade != nullptr && EntryDistance <= EntryGrenadeDistance && EntryDistance >= NearestEntry)
		{
			bEntryGrenadeDone = ThrowGrenadeAt(EntryGrenade, Entry);
			return;
		}
	}

	// An enemy seen or heard over there (the blackboard's noise): the HE, else a flashbang, else the smoke, when the
	// spot's draw says so.
	const UBlackboardComponent& Board = Tree.GetBlackboard();
	if (NoiseHeardTime < 0.0f || Now - NoiseHeardTime > NoiseMemory || !Board.IsValueSet(NoiseLocationKey))
	{
		return;
	}
	const FVector Spot = Board.GetValueAsVector(NoiseLocationKey);
	const float SpotDistance = FVector::Dist(Feet, Spot);
	if (SpotDistance < MinGrenadeDistance || SpotDistance > MaxGrenadeDistance ||
		FVector::DistSquared(Spot, LastGrenadeSpot) < FMath::Square(GrenadeSpotSpacing))
	{
		return;
	}
	LastGrenadeSpot = Spot;
	if (BotRandom.FRand() >= GrenadeChance)
	{
		return;
	}
	AShooterWeapon_Projectile* Grenade = HEGrenade != nullptr ? HEGrenade : Flashbang != nullptr ? Flashbang : Smoke;
	(void)ThrowGrenadeAt(Grenade, Spot);
}

EBTNodeResult AShooterAIController::TaskThrowGrenade(float DeltaTime)
{
	AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr || !bThrowing)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = ThrowGrenadeTaskName;
	StandStill();
	const float Now = GetWorldTime();
	if (!bThrown)
	{
		AShooterWeapon_Projectile* Grenade = ThrowingGrenade.Get();
		if (Grenade == nullptr || !Self->GetInventory().Contains(Grenade) || Now - ThrowStartTime > ThrowTimeout)
		{
			EndThrow();
			Self->EquipBestWeapon();
			return EBTNodeResult::Failed;
		}
		// Draw it, turn to the throw, and throw once it is out and aimed.
		if (Self->GetWeapon() != Grenade)
		{
			ReleaseTrigger();
			Self->EquipWeapon(Grenade);
		}
		const float AngleLeft = AimToward(ThrowRotation, *Grenade, DeltaTime);
		if (AngleLeft > ThrowAimTolerance || !Grenade->CanFire())
		{
			return EBTNodeResult::Running;
		}
		const bool bFlashbang = Grenade->IsA<AShooterWeapon_Flashbang>();
		const float Fuse = Grenade->FuseTime;
		const int32 ShotsBefore = Grenade->GetShotsFired();
		// The last grenade of its kind leaves the inventory with the throw (the weak pointer lets go of it).
		Self->StartWeaponFire();
		Self->StopWeaponFire();
		if (ThrowingGrenade.IsValid() && ThrowingGrenade->GetShotsFired() == ShotsBefore)
		{
			return EBTNodeResult::Running;
		}
		bThrown = true;
		NextGrenadeTime = Now + GrenadeCooldown;
		Self->EquipBestWeapon();
		if (!bFlashbang)
		{
			EndThrow();
			return EBTNodeResult::Succeeded;
		}
		TurnAwayEndTime = Now + Fuse + FlashTurnAwayMargin;
		return EBTNodeResult::Running;
	}
	// Its back to its flashbang until it has gone off (CS's players turn away).
	if (Now >= TurnAwayEndTime)
	{
		EndThrow();
		return EBTNodeResult::Succeeded;
	}
	if (AShooterWeapon* Weapon = Self->GetWeapon())
	{
		(void)AimToward(FRotator(0.0f, ThrowRotation.Yaw + 180.0f, 0.0f), *Weapon, DeltaTime);
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
	CurrentTask = DefuseTaskName;
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
	CurrentTask = PlantTaskName;
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
	CurrentTask = FetchBombTaskName;
	ReleaseTrigger();
	MoveToGoal(Bomb->GetActorLocation());
	return EBTNodeResult::Running;
}

EBTNodeResult AShooterAIController::TaskEscort(float DeltaTime)
{
	const AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterBomb* Bomb = GameMode != nullptr ? GameMode->GetBomb() : nullptr;
	const AShooterCharacter* Carrier = Bomb != nullptr ? Bomb->GetCarrier() : nullptr;
	const AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr || Carrier == nullptr)
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = EscortTaskName;
	ReleaseTrigger();
	if (FVector::DistSquared2D(Self->GetActorLocation(), Carrier->GetActorLocation()) <= FMath::Square(EscortDistance))
	{
		HoldAndLookAround(DeltaTime);
		return EBTNodeResult::Running;
	}
	MoveToGoal(Carrier->GetActorLocation());
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
		// Nothing here: forget it, and tell the team (CS's "Sector clear.").
		NoiseHeardTime = -1.0f;
		Board.ClearValue(NoiseLocationKey);
		(void)SayOnRadio(EShooterRadioMessage::SectorClear);
		return EBTNodeResult::Failed;
	}
	CurrentTask = InvestigateTaskName;
	ReleaseTrigger();
	MoveToGoal(Noise);
	return EBTNodeResult::Running;
}

EBTNodeResult AShooterAIController::TaskHunt(float DeltaTime)
{
	const AShooterCharacter* Self = GetShooterPawn();
	const AShooterGameMode* GameMode = GetShooterGameMode();
	FVector Goal = FVector::ZeroVector;
	const EShooterTeam EnemyTeam =
		Self != nullptr && Self->GetTeam() == EShooterTeam::CT ? EShooterTeam::T : EShooterTeam::CT;
	if (Self == nullptr || GameMode == nullptr || !GameMode->GetTeamSpawnLocation(EnemyTeam, Goal))
	{
		return EBTNodeResult::Failed;
	}
	CurrentTask = HuntTaskName;
	ReleaseTrigger();
	// Toward the enemy's spawn: the senses turn the first contact into an engagement or an investigation.
	if (FVector::DistSquared2D(Self->GetActorLocation(), Goal) <= FMath::Square(GoalReachedDistance))
	{
		HoldAndLookAround(DeltaTime);
		return EBTNodeResult::Succeeded;
	}
	MoveToGoal(Goal);
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
	CurrentTask = ObjectiveTaskName;
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
		// The terrorists go for the round's site; the CT split over the sites and rotate (SiteRotation).
		const TArray<FName>& Sites = GameMode->GetBombSiteNames();
		const FName Site = Self->GetTeam() == EShooterTeam::T
			? GameMode->GetTerroristTargetSite()
			: (Sites.Num() > 0 ? Sites[(GetTeamIndex() + SiteRotation) % Sites.Num()] : NAME_None);
		if (!GameMode->GetBombSiteLocation(Site, Goal))
		{
			StandStill();
			return EBTNodeResult::Succeeded;
		}
	}
	if (FVector::DistSquared2D(Self->GetActorLocation(), Goal) <= FMath::Square(GoalReachedDistance))
	{
		HoldAndLookAround(DeltaTime);
		// A CT that held its site RotateTime with no contact moves on to the next one.
		const float Now = GetWorldTime();
		if (HoldingSinceTime < 0.0f)
		{
			HoldingSinceTime = Now;
		}
		else if (Self->GetTeam() == EShooterTeam::CT && State->GetBombState() != EShooterBombState::Planted &&
			RotateTime > 0.0f && Now - HoldingSinceTime >= RotateTime)
		{
			++SiteRotation;
			HoldingSinceTime = -1.0f;
		}
		return EBTNodeResult::Succeeded;
	}
	HoldingSinceTime = -1.0f;
	MoveToGoal(Goal);
	return EBTNodeResult::Running;
}

// The radio

bool AShooterAIController::SayOnRadio(EShooterRadioMessage Message, const FVector* Location)
{
	AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterGameState* State = GameMode != nullptr ? GameMode->GetShooterGameState() : nullptr;
	const AShooterCharacter* Self = GetShooterPawn();
	if (State == nullptr || Self == nullptr ||
		State->WasRadioSentSince(Self->GetTeam(), Message, GetWorldTime() - RadioRepeatTime))
	{
		return false;
	}
	return GameMode->SendRadioMessage(this, Message, Location);
}

void AShooterAIController::OnRadioMessage(const FShooterRadioEntry& Entry)
{
	const AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr || !Self->IsAlive() || Entry.Message != EShooterRadioMessage::EnemySpotted)
	{
		return;
	}
	// A teammate's enemy near this bot, while it fights nobody, has nothing else to look at and holds no site (a
	// counter-terrorist at its site stays there): it goes to look.
	const float Now = GetWorldTime();
	const bool bBusy =
		Enemy != nullptr || (NoiseHeardTime >= 0.0f && Now - NoiseHeardTime <= NoiseMemory) || HoldingSinceTime >= 0.0f;
	if (!bBusy && FVector::DistSquared2D(Self->GetActorLocation(), Entry.Location) <= FMath::Square(RadioReportRange))
	{
		LookAt(Entry.Location);
	}
}

void AShooterAIController::AnswerRadioRequest(const FShooterRadioEntry& Entry)
{
	AShooterGameMode* GameMode = GetShooterGameMode();
	const AShooterCharacter* Self = GetShooterPawn();
	if (GameMode == nullptr || Self == nullptr || !Self->IsAlive())
	{
		return;
	}
	// The answer goes out even when a teammate just answered another request.
	(void)GameMode->SendRadioMessage(this,
		Entry.Message == EShooterRadioMessage::ReportIn ? EShooterRadioMessage::ReportingIn
														: EShooterRadioMessage::Affirmative);
	if (Entry.Message == EShooterRadioMessage::NeedBackup || Entry.Message == EShooterRadioMessage::TakingFire)
	{
		LookAt(Entry.Location);
	}
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
}

void AShooterAIController::HoldAndLookAround(float DeltaTime)
{
	StandStill();
	FRotator Look = GetControlRotation();
	constexpr float LookAroundRate = 30.0f;
	Look.Yaw += LookAroundRate * DeltaTime;
	Look.Pitch = 0.0f;
	SetControlRotation(Look);
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

float AShooterAIController::AimToward(const FRotator& Wanted, AShooterWeapon& Weapon, float DeltaTime)
{
	// The kick of a hitscan weapon (the weapon puts it on the control rotation with each shot and takes it off as it
	// recovers). A new engagement or another weapon starts from the kick as it is; after that, the bot pulls down its
	// share of what the shots since the last tick added.
	float Kick = 0.0f;
	if (AShooterWeapon_Instant* Instant = Cast<AShooterWeapon_Instant>(&Weapon))
	{
		if (!bAimedLastTick || RecoilWeapon.Get() != Instant)
		{
			LastRecoilKick = Instant->GetRecoilToRecover();
		}
		const float NewKick = FMath::Max(0.0f, Instant->GetRecoilToRecover() - LastRecoilKick);
		Instant->CompensateRecoil(NewKick * GetRecoilCompensation());
		Kick = Instant->GetRecoilToRecover();
	}
	RecoilWeapon = &Weapon;
	LastRecoilKick = Kick;
	bAimedThisTick = true;

	// The bot's own aim is what the kick sits on; it turns toward Wanted and the kick goes back on top.
	FRotator Aim = GetControlRotation();
	Aim.Pitch -= Kick;
	const float MaxStep = AimTurnRate * FMath::Max(0.1f, Difficulty) * DeltaTime;
	Aim.Yaw += FMath::Clamp(FRotator::NormalizeAxis(Wanted.Yaw - Aim.Yaw), -MaxStep, MaxStep);
	Aim.Pitch += FMath::Clamp(FRotator::NormalizeAxis(Wanted.Pitch - Aim.Pitch), -MaxStep, MaxStep);
	Aim.Pitch = FMath::Clamp(Aim.Pitch, -89.0f, 89.0f);
	const float AngleLeft = FMath::Max(FMath::Abs(FRotator::NormalizeAxis(Wanted.Yaw - Aim.Yaw)),
		FMath::Abs(FRotator::NormalizeAxis(Wanted.Pitch - Aim.Pitch)));
	FRotator Kicked = Aim;
	Kicked.Pitch = FMath::Clamp(Aim.Pitch + Kick, -89.0f, 89.0f);
	SetControlRotation(Kicked);
	return AngleLeft;
}

void AShooterAIController::Tick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_ShooterBotTick);
	Super::Tick(DeltaSeconds);
	AShooterCharacter* Self = GetShooterPawn();
	if (Self == nullptr)
	{
		return;
	}
	UpdateBlackboard();
	bAimedLastTick = bAimedThisTick;
	bAimedThisTick = false;
	bStrafingThisTick = false;
	(void)Tree.Tick(DeltaSeconds);
	// Only the engagement crouches the pawn for its fire.
	if (CurrentTask != EngageTaskName)
	{
		EndCombatCrouch();
	}
	// The steering along the path (AAIController's path following); a pawn that stands has no target. The strafe
	// replaces the standing pawn's wish, at the walk key's speed.
	const AShooterGameMode* GameMode = GetShooterGameMode();
	if (Self->IsAlive() && !Self->IsFrozen() && (GameMode == nullptr || !GameMode->bBotStop))
	{
		(void)TickAI(DeltaSeconds);
		if (bStrafingThisTick)
		{
			Self->AddMovementInput(StrafeWish);
		}
	}
	else if (bStrafedLastTick)
	{
		Self->AddMovementInput(FVector::ZeroVector);
	}
	if (bStrafeWalking && bStrafingThisTick != bStrafedLastTick)
	{
		Self->SetWalking(bStrafingThisTick);
	}
	bStrafedLastTick = bStrafingThisTick;
}
