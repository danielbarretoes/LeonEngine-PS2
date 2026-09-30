#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavigationWaypoint.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/Level.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterCharacterMovement.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "Tests/ScopedTestWorld.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_Knife.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-polish P3's tests of the bots (AShooterAIController's class comment): the knife's rush, a spent weapon swapped
// for one on the floor, the lookouts of a site watched with no enemy in sight (and the same moves with the same seed),
// and a ladder climbed up and down on the way to them.

namespace
{

	constexpr float FrameTime = 1.0f / 60.0f;

	void TickFrames(UWorld& World, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(FrameTime);
		}
	}

	/** A wall box of Size cm, its bottom on the floor, centred at X / Y. */
	void SpawnWall(UWorld& World, float X, float Y, const FVector& Size)
	{
		(void)World.SpawnActor<ABlockingVolume>(
			ABlockingVolume::StaticClass(), FTransform(FQuat::Identity, FVector(X, Y, Size.Z * 0.5f), Size / 100.0f));
	}

	ATriggerVolume* SpawnZone(
		UWorld& World, const FVector& Center, const FVector& Size, const TCHAR* Kind, const TCHAR* Name = nullptr)
	{
		ATriggerVolume* Zone = World.SpawnActor<ATriggerVolume>(
			ATriggerVolume::StaticClass(), FTransform(FQuat::Identity, Center, Size / 100.0f));
		Zone->Tags.Add(FName(Kind));
		if (Name != nullptr)
		{
			Zone->Tags.Add(FName(Name));
		}
		return Zone;
	}

	/**
	 * The open test map of the bot tests (a floor, CT starts at X = -1500, T starts at X = 1500 behind a wall at X =
	 * 1000, their buy zones), bomb site A at SiteCenter (a box of SiteSize); short phases; one bot a team.
	 */
	AShooterGameMode* SetUpBotMatch(UWorld& World, const FVector& SiteCenter, const FVector& SiteSize)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
		SpawnWall(World, 1000.0f, 0.0f, FVector(100.0f, 3000.0f, 400.0f));
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Y = (static_cast<float>(Index) - 2.0f) * 150.0f;
			APlayerStart* CTStart = World.SpawnActor<APlayerStart>(FVector(-1500.0f, Y, 92.0f), FRotator::ZeroRotator);
			CTStart->PlayerStartTag = FName(TEXT("CT"));
			APlayerStart* TStart =
				World.SpawnActor<APlayerStart>(FVector(1500.0f, Y, 92.0f), FRotator(0.0f, 180.0f, 0.0f));
			TStart->PlayerStartTag = FName(TEXT("T"));
		}
		(void)SpawnZone(
			World, FVector(-1500.0f, 0.0f, 150.0f), FVector(600.0f, 1000.0f, 300.0f), TEXT("BuyZone"), TEXT("CT"));
		(void)SpawnZone(
			World, FVector(1500.0f, 0.0f, 150.0f), FVector(600.0f, 1000.0f, 300.0f), TEXT("BuyZone"), TEXT("T"));
		(void)SpawnZone(World, SiteCenter, SiteSize, TEXT("BombSite"), TEXT("A"));
		AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
		GameMode->bFillTeamsWithBots = false;
		GameMode->FreezeTime = 0.5f;
		GameMode->RoundTime = 90.0f;
		GameMode->RoundRestartDelay = 0.5f;
		GameMode->BuyTime = 3.0f;
		GameMode->RandomSeed = 3;
		(void)GameMode->AddBots(EShooterTeam::CT, 1);
		(void)GameMode->AddBots(EShooterTeam::T, 1);
		return GameMode;
	}

	AShooterCharacter* GetAlivePawn(UWorld& World, EShooterTeam Team)
	{
		for (AActor* Actor : World.PersistentLevel->Actors)
		{
			AShooterCharacter* Pawn = Cast<AShooterCharacter>(Actor);
			if (Pawn != nullptr && !Pawn->IsPendingKillPending() && Pawn->IsAlive() && Pawn->GetTeam() == Team)
			{
				return Pawn;
			}
		}
		return nullptr;
	}

	void TickUntilLive(UWorld& World, const AShooterGameMode& GameMode)
	{
		for (int32 Frame = 0; Frame < 600; ++Frame)
		{
			const AShooterGameState* State = GameMode.GetShooterGameState();
			if (State != nullptr && State->GetRoundState() == EShooterRoundState::Live)
			{
				return;
			}
			World.Tick(FrameTime);
		}
	}

	/** Switches a bot's brain off (its pawn stays where it is put). */
	void Freeze(AShooterCharacter& Pawn)
	{
		if (AShooterAIController* Bot = Cast<AShooterAIController>(Pawn.GetController()))
		{
			Bot->SetActorTickEnabled(false);
		}
	}

	AShooterAIController* GetBot(const AShooterCharacter& Pawn)
	{
		return Cast<AShooterAIController>(Pawn.GetController());
	}

	/** A waypoint 50 cm above Floor (as the maps place them), with its flags. */
	ANavigationWaypoint* SpawnWaypoint(UWorld& World, const FVector& Floor, const TCHAR* Flag = nullptr)
	{
		ANavigationWaypoint* Waypoint =
			World.SpawnActor<ANavigationWaypoint>(Floor + FVector(0.0f, 0.0f, 50.0f), FRotator::ZeroRotator);
		if (Flag != nullptr)
		{
			Waypoint->Flags.Add(FName(Flag));
		}
		return Waypoint;
	}

	/** A frozen terrorist plants the bomb at Location (site A must hold it); true once planted. */
	bool PlantBomb(UWorld& World, AShooterGameMode& GameMode, AShooterCharacter& Planter, const FVector& Location)
	{
		Freeze(Planter);
		Planter.Reset(Location);
		TickFrames(World, 2);
		if (!Planter.StartUse())
		{
			return false;
		}
		TickFrames(World, 60 * 4);
		return GameMode.GetBomb() != nullptr && GameMode.GetBomb()->GetBombState() == EShooterBombState::Planted;
	}

	/** The radio's last Message from Team, or null. */
	const FShooterRadioEntry* FindRadio(const AShooterGameMode& GameMode, EShooterRadioMessage Message)
	{
		const TArray<FShooterRadioEntry>& Log = GameMode.GetShooterGameState()->GetRadioLog();
		for (int32 Index = Log.Num() - 1; Index >= 0; --Index)
		{
			if (Log[Index].Message == Message)
			{
				return &Log[Index];
			}
		}
		return nullptr;
	}

	FVector GetLookAt(const AShooterAIController& Bot)
	{
		return Bot.GetBlackboard().IsValueSet(AShooterAIController::NoiseLocationKey)
			? Bot.GetBlackboard().GetValueAsVector(AShooterAIController::NoiseLocationKey)
			: FVector(1.0e9f, 1.0e9f, 1.0e9f);
	}

	/** The world's graph of its waypoints, auto-linked as the map import links them. */
	void BuildGraph(UWorld& World)
	{
		(void)UNavigationSystem::AutoLinkWaypoints(World, FWaypointLinkParams::FromConfig());
		World.GetNavigationSystem().Build(World);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsKnifeRushTest, "ShooterGame.Bots.KnifeRushesAndKills",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsKnifeRushTest::RunTest(const FString& Parameters)
{
	// A CT bot with its knife only (and a flashbang, which a fight never draws) sees a terrorist 8 m ahead with its
	// back turned: it runs at it, cuts only within the knife's reach, and kills it with a stab in the back.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, FVector(0.0f, 2500.0f, 150.0f), FVector(600.0f, 600.0f, 300.0f));
	TickUntilLive(World, *GameMode);
	AShooterCharacter* CT = GetAlivePawn(World, EShooterTeam::CT);
	AShooterCharacter* T = GetAlivePawn(World, EShooterTeam::T);
	if (!TestNotNull("A CT bot", CT) || !TestNotNull("A terrorist bot", T))
	{
		return false;
	}
	Freeze(*T);
	CT->DestroyInventory();
	AShooterWeapon_Knife* Knife =
		Cast<AShooterWeapon_Knife>(CT->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("knife"))));
	(void)CT->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("flashbang")));
	if (!TestNotNull("A knife", Knife))
	{
		return false;
	}
	const FVector Start = CT->GetActorLocation();
	T->Reset(FVector(Start.X + 800.0f, Start.Y, 0.0f), FRotator(0.0f, 0.0f, 0.0f));
	GetBot(*T)->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
	const AShooterAIController* Bot = GetBot(*CT);
	const float Radius = T->GetCapsule().GetCapsuleRadius();
	int32 CutsOutOfReach = 0;
	bool bEngaged = false;
	bool bOnlyTheKnife = true;
	bool bRan = false;
	for (int32 Frame = 0; Frame < 60 * 10 && T->IsAlive(); ++Frame)
	{
		const int32 CutsBefore = Knife->GetShotsFired();
		const float Reach = FVector::Dist2D(CT->GetActorLocation(), T->GetActorLocation()) - Radius;
		World.Tick(FrameTime);
		CutsOutOfReach += Knife->GetShotsFired() > CutsBefore && Reach > Knife->SlashRange ? 1 : 0;
		const bool bEngagingNow = Bot->GetCurrentTask() == FName(TEXT("Engage"));
		bEngaged |= bEngagingNow;
		bOnlyTheKnife &= !bEngagingNow || CT->GetWeapon() == Knife;
		// Running (a knife's 250 u/s), not walking or standing, on its way in.
		bRan |= bEngagingNow && CT->GetCharacterMovement().Velocity.Size2D() > 500.0f;
	}
	TestTrue("It engaged", bEngaged);
	TestTrue("With the knife, not the flashbang", bOnlyTheKnife);
	TestTrue("It ran at the terrorist", bRan);
	TestEqual("No cut out of reach", CutsOutOfReach, 0);
	TestFalse("The terrorist is dead", T->IsAlive());
	TestTrue("A stab in the back", Knife->WasLastAttackStab() && Knife->WasLastAttackBackstab());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsPicksUpAWeaponTest, "ShooterGame.Bots.PicksUpAWeaponOutOfAmmo",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsPicksUpAWeaponTest::RunTest(const FString& Parameters)
{
	// A CT bot whose pistol is spent (nobody in sight) goes for a loaded Glock on the floor 8 m away and takes it, its
	// spent pistol dropped for it (AShooterWeapon::CanBePickedUpBy); a pawn that is not a bot's would not swap.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, FVector(0.0f, 2500.0f, 150.0f), FVector(600.0f, 600.0f, 300.0f));
	TickUntilLive(World, *GameMode);
	AShooterCharacter* CT = GetAlivePawn(World, EShooterTeam::CT);
	AShooterCharacter* T = GetAlivePawn(World, EShooterTeam::T);
	if (!TestNotNull("A CT bot", CT) || !TestNotNull("A terrorist bot", T))
	{
		return false;
	}
	Freeze(*T);
	AShooterWeapon* Spent = CT->GetWeaponInSlot(EShooterWeaponSlot::Secondary);
	if (!TestNotNull("A pistol", Spent) || !TestNull("No primary", CT->GetWeaponInSlot(EShooterWeaponSlot::Primary)))
	{
		return false;
	}
	Spent->SetAmmo(0, 0);
	UClass* GlockClass = AShooterWeapon::FindWeaponClass(TEXT("glock"));
	AShooterWeapon* Loaded = World.SpawnActor<AShooterWeapon>(GlockClass, FVector::ZeroVector, FRotator::ZeroRotator);
	Loaded->SetAmmo(Loaded->AmmoPerClip, Loaded->AmmoPerClip);
	Loaded->OnDropped(CT->GetActorLocation() + FVector(0.0f, 800.0f, 0.0f), 0.0f);
	TestTrue("On the floor", GameMode->GetPickups().Contains(Loaded));
	TestTrue("The bot may swap its spent pistol for it", Loaded->CanBePickedUpBy(*CT));

	// A pawn without a bot keeps CS's walk-over: its spent pistol stays, the slot is not free.
	AShooterCharacter* Other =
		World.SpawnActor<AShooterCharacter>(FVector(-3000.0f, -3000.0f, 0.0f), FRotator::ZeroRotator);
	AShooterWeapon* OtherPistol = Other->GiveWeapon(GlockClass);
	OtherPistol->SetAmmo(0, 0);
	TestFalse("Not a bot's pawn", Loaded->CanBePickedUpBy(*Other));

	const AShooterAIController* Bot = GetBot(*CT);
	bool bWentForIt = false;
	for (int32 Frame = 0; Frame < 60 * 15 && CT->GetWeaponInSlot(EShooterWeaponSlot::Secondary) != Loaded; ++Frame)
	{
		World.Tick(FrameTime);
		bWentForIt |= Bot->GetCurrentTask() == FName(TEXT("PickUp")) && Bot->GetPickupTarget() == Loaded;
	}
	TestTrue("It went for it", bWentForIt);
	TestTrue("It took it", CT->GetWeaponInSlot(EShooterWeaponSlot::Secondary) == Loaded);
	TickFrames(World, 1);
	TestTrue("Its spent pistol on the floor", Spent->IsDropped() && !CT->GetInventory().Contains(Spent));
	TestTrue("Nothing more to go for", Bot->GetPickupTarget() == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsVisitsLookoutsTest, "ShooterGame.Bots.VisitsLookouts",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsVisitsLookoutsTest::RunTest(const FString& Parameters)
{
	// Site A with three waypoints flagged Lookout around it: the site's lookouts, each watching first the way the other
	// team comes (the counter-terrorists toward the terrorists' spawn, the terrorists the other way). A CT bot with no
	// enemy in sight goes from lookout to lookout and turns between their directions there; with the same seed it
	// walks the same way again.
	auto Play = [this](TArray<FVector>& OutTrail, int32& OutVisited, int32& OutSpots, float& OutTurned)
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpBotMatch(World, FVector(0.0f, 0.0f, 150.0f), FVector(600.0f, 600.0f, 300.0f));
		const FVector Spots[] = {
			FVector(400.0f, 400.0f, 0.0f), FVector(-400.0f, 400.0f, 0.0f), FVector(400.0f, -400.0f, 0.0f)};
		for (const FVector& Spot : Spots)
		{
			(void)SpawnWaypoint(World, Spot, TEXT("Lookout"));
		}
		(void)SpawnWaypoint(World, FVector(-1500.0f, 0.0f, 0.0f));
		(void)SpawnWaypoint(World, FVector(700.0f, 0.0f, 0.0f));
		BuildGraph(World);
		const TArray<FShooterLookout>& Lookouts = GameMode->GetBombSiteLookouts(TEXT("A"));
		TestEqual("Three lookouts", Lookouts.Num(), 3);
		for (const FShooterLookout& Lookout : Lookouts)
		{
			const TArray<float, TInlineAllocator<4>>& CTYaws = Lookout.GetWatchYaws(EShooterTeam::CT);
			const TArray<float, TInlineAllocator<4>>& TYaws = Lookout.GetWatchYaws(EShooterTeam::T);
			TestTrue("A CT watches toward the terrorists first",
				CTYaws.Num() > 0 && FMath::Abs(FRotator::NormalizeAxis(CTYaws[0])) < 60.0f);
			TestTrue("A terrorist toward the counter-terrorists",
				TYaws.Num() > 0 && FMath::Abs(FRotator::NormalizeAxis(TYaws[0] - 180.0f)) < 60.0f);
		}

		TickUntilLive(World, *GameMode);
		AShooterCharacter* CT = GetAlivePawn(World, EShooterTeam::CT);
		AShooterCharacter* T = GetAlivePawn(World, EShooterTeam::T);
		if (!TestNotNull("A CT bot", CT) || !TestNotNull("A terrorist bot", T))
		{
			return;
		}
		Freeze(*T);
		AShooterAIController* Bot = GetBot(*CT);
		Bot->RotateTime = 0.0f;
		Bot->LookoutMinTime = 1.0f;
		Bot->LookoutMaxTime = 2.0f;
		TSet<int32> Visited;
		float MinYaw = 1000.0f;
		float MaxYaw = -1000.0f;
		for (int32 Frame = 0; Frame < 60 * 30; ++Frame)
		{
			World.Tick(FrameTime);
			if (Frame % 30 == 0)
			{
				OutTrail.Add(CT->GetActorLocation());
			}
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (FVector::Dist2D(CT->GetActorLocation(), Spots[Index]) <= Bot->GoalReachedDistance &&
					!Bot->HasMoveTarget())
				{
					Visited.Add(Index);
					const float Yaw = FRotator::NormalizeAxis(Bot->GetControlRotation().Yaw);
					MinYaw = FMath::Min(MinYaw, Yaw);
					MaxYaw = FMath::Max(MaxYaw, Yaw);
				}
			}
		}
		TestEqual("The objective", Bot->GetCurrentTask().ToString(), FString(TEXT("Objective")));
		OutVisited = Bot->GetNumLookoutsVisited();
		OutSpots = Visited.Num();
		OutTurned = MaxYaw - MinYaw;
	};
	TArray<FVector> Trail;
	int32 NumVisited = 0;
	int32 NumSpots = 0;
	float Turned = 0.0f;
	Play(Trail, NumVisited, NumSpots, Turned);
	TestTrue(*FString::Printf(TEXT("Lookouts visited (%d)"), NumVisited), NumVisited >= 4);
	TestTrue(*FString::Printf(TEXT("Different lookouts (%d)"), NumSpots), NumSpots >= 2);
	TestTrue(*FString::Printf(TEXT("Turned between the directions (%.0f degrees)"), static_cast<double>(Turned)),
		Turned >= 20.0f);
	TArray<FVector> Replay;
	int32 ReplayVisited = 0;
	int32 ReplaySpots = 0;
	float ReplayTurned = 0.0f;
	Play(Replay, ReplayVisited, ReplaySpots, ReplayTurned);
	bool bSame = Replay.Num() == Trail.Num();
	for (int32 Index = 0; bSame && Index < Trail.Num(); ++Index)
	{
		bSame = Trail[Index].Equals(Replay[Index], 0.0f);
	}
	TestTrue("The same seed walks the same way", bSame && ReplayVisited == NumVisited);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsClimbsALadderTest, "ShooterGame.Bots.ClimbsALadder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsClimbsALadderTest::RunTest(const FString& Parameters)
{
	// Site A on the roof of a 4 m block with a ladder on its face: the waypoint graph links the ladder's foot and top
	// (both flagged Ladder), and a CT bot with no enemy in sight walks to the ladder, climbs it facing it, walks onto
	// the roof to the site's lookouts; going to the one at the ladder's foot it climbs down, looking down, and steps
	// off it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	// The block: X 100 to 500, Y -300 to 300, its roof at Z = 400; the site on the roof.
	SpawnWall(World, 300.0f, 0.0f, FVector(400.0f, 600.0f, 400.0f));
	AShooterGameMode* GameMode = SetUpBotMatch(World, FVector(300.0f, 0.0f, 550.0f), FVector(400.0f, 600.0f, 300.0f));
	// The ladder: X 80 to 100 against the block's face, Y -50 to 50, the floor to the roof.
	ATriggerVolume* Ladder = SpawnZone(
		World, FVector(90.0f, 0.0f, 200.0f), FVector(20.0f, 100.0f, 400.0f), *AShooterGameMode::LadderTag.ToString());
	(void)SpawnWaypoint(World, FVector(-600.0f, 0.0f, 0.0f));
	const ANavigationWaypoint* Foot = SpawnWaypoint(World, FVector(20.0f, 0.0f, 0.0f), TEXT("Ladder"));
	const ANavigationWaypoint* Top = SpawnWaypoint(World, FVector(160.0f, 0.0f, 400.0f), TEXT("Ladder"));
	(void)SpawnWaypoint(World, FVector(380.0f, 0.0f, 400.0f));
	BuildGraph(World);
	TestTrue("The ladder is linked both ways", Foot->Links.Contains(Top) && Top->Links.Contains(Foot));
	TickUntilLive(World, *GameMode);
	AShooterCharacter* CT = GetAlivePawn(World, EShooterTeam::CT);
	AShooterCharacter* T = GetAlivePawn(World, EShooterTeam::T);
	if (!TestNotNull("A CT bot", CT) || !TestNotNull("A terrorist bot", T))
	{
		return false;
	}
	Freeze(*T);
	AShooterAIController* Bot = GetBot(*CT);
	Bot->RotateTime = 0.0f;
	Bot->LookoutMinTime = 1.0f;
	Bot->LookoutMaxTime = 2.0f;
	const UShooterCharacterMovement& Move = *CT->GetShooterCharacterMovement();
	bool bClimbedUp = false;
	int32 ClimbFrames = 0;
	int32 FacingFrames = 0;
	bool bOnTheRoof = false;
	bool bClimbedDown = false;
	bool bLookedDown = false;
	bool bOffAtTheFoot = false;
	for (int32 Frame = 0; Frame < 60 * 40 && !bOffAtTheFoot; ++Frame)
	{
		const float Z = CT->GetActorLocation().Z;
		World.Tick(FrameTime);
		const bool bOnLadder = Move.IsOnLadder() && Move.GetLadder() == Ladder;
		const float Climb = CT->GetActorLocation().Z - Z;
		if (bOnLadder && Climb > 1.0f)
		{
			bClimbedUp = true;
			// Facing the ladder's face (+X), level (the frame it grabs the ladder is the walk's).
			++ClimbFrames;
			FacingFrames += FMath::Abs(FRotator::NormalizeAxis(Bot->GetControlRotation().Yaw)) < 5.0f &&
					FMath::Abs(Bot->GetControlRotation().Pitch) < 5.0f
				? 1
				: 0;
		}
		bOnTheRoof |= bClimbedUp && CT->IsMovingOnGround() && CT->GetActorLocation().Z > 390.0f;
		if (bOnTheRoof && bOnLadder && Climb < -1.0f)
		{
			bClimbedDown = true;
			bLookedDown |= Bot->GetControlRotation().Pitch < -45.0f;
		}
		bOffAtTheFoot |=
			bClimbedDown && !Move.IsOnLadder() && CT->IsMovingOnGround() && CT->GetActorLocation().Z < 5.0f;
	}
	TestTrue("It climbed the ladder", bClimbedUp);
	TestTrue(*FString::Printf(TEXT("Facing it (%d of %d frames)"), FacingFrames, ClimbFrames),
		FacingFrames >= ClimbFrames - 1);
	TestTrue("Onto the roof", bOnTheRoof);
	TestTrue("It climbed down", bClimbedDown);
	TestTrue("Looking down", bLookedDown);
	TestTrue("And stepped off at the foot", bOffAtTheFoot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsHoldThePlantedBombTest,
	"ShooterGame.Bots.TerroristsHoldThePlantedBomb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsHoldThePlantedBombTest::RunTest(const FString& Parameters)
{
	// ps2-polish P3b: the bomb planted at site A, the terrorist bot holds near it (within PostPlantHoldRadius), calls
	// "Hold this position.", and a teammate's report 20 m from the bomb does not draw it away.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, FVector(0.0f, 0.0f, 150.0f), FVector(600.0f, 600.0f, 300.0f));
	TickUntilLive(World, *GameMode);
	AShooterCharacter* CT = GetAlivePawn(World, EShooterTeam::CT);
	AShooterCharacter* T = GetAlivePawn(World, EShooterTeam::T);
	if (!TestNotNull("A CT bot", CT) || !TestNotNull("A terrorist bot", T))
	{
		return false;
	}
	// The CT far out of sight, still.
	Freeze(*CT);
	CT->Reset(FVector(-3000.0f, 3000.0f, 0.0f));
	if (!TestTrue("Planted", PlantBomb(World, *GameMode, *T, FVector(100.0f, 50.0f, 0.0f))))
	{
		return false;
	}
	AShooterAIController* Bot = GetBot(*T);
	Bot->SetActorTickEnabled(true);
	const FVector BombLocation = GameMode->GetBomb()->GetActorLocation();
	FShooterRadioEntry Report;
	Report.Team = EShooterTeam::T;
	Report.Message = EShooterRadioMessage::EnemySpotted;
	Report.Location = BombLocation + FVector(-2000.0f, 0.0f, 0.0f);
	float Farthest = 0.0f;
	for (int32 Frame = 0; Frame < 60 * 12; ++Frame)
	{
		if (Frame == 60)
		{
			Bot->OnRadioMessage(Report);
		}
		World.Tick(FrameTime);
		Farthest = FMath::Max(Farthest, FVector::Dist2D(T->GetActorLocation(), BombLocation));
	}
	TestTrue(*FString::Printf(TEXT("Near the bomb (%.0f cm at most)"), static_cast<double>(Farthest)),
		Farthest <= Bot->PostPlantHoldRadius + Bot->GoalReachedDistance);
	TestFalse("The far report is not followed", GetLookAt(*Bot).Equals(Report.Location, 1.0f));
	const FShooterRadioEntry* Hold = FindRadio(*GameMode, EShooterRadioMessage::HoldThisPosition);
	TestTrue("Hold this position.", Hold != nullptr && Hold->Team == EShooterTeam::T);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsDefuserEngagedTest, "ShooterGame.Bots.TerroristsEngageTheDefuser",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsDefuserEngagedTest::RunTest(const FString& Parameters)
{
	// ps2-polish P3b: the terrorist holds 9 m from the planted bomb looking away; a counter-terrorist starts defusing
	// behind it: the terrorist hears the defuse (AShooterBomb::DefuseNoiseLoudness), goes for the bomb and engages the
	// defuser, who never finishes.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, FVector(0.0f, 0.0f, 150.0f), FVector(600.0f, 600.0f, 300.0f));
	TickUntilLive(World, *GameMode);
	AShooterCharacter* CT = GetAlivePawn(World, EShooterTeam::CT);
	AShooterCharacter* T = GetAlivePawn(World, EShooterTeam::T);
	if (!TestNotNull("A CT bot", CT) || !TestNotNull("A terrorist bot", T))
	{
		return false;
	}
	Freeze(*CT);
	CT->Reset(FVector(-3000.0f, 3000.0f, 0.0f));
	if (!TestTrue("Planted", PlantBomb(World, *GameMode, *T, FVector(0.0f, 0.0f, 0.0f))))
	{
		return false;
	}
	AShooterBomb* Bomb = GameMode->GetBomb();
	const FVector BombLocation = Bomb->GetActorLocation();
	AShooterAIController* Bot = GetBot(*T);
	Bot->PostPlantHoldRadius = 1000.0f;
	// The terrorist 9 m off, facing away from the bomb, its brain frozen until the defuse starts.
	T->Reset(BombLocation + FVector(900.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f));
	Bot->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
	CT->Reset(BombLocation + FVector(-50.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f));
	CT->SetDefuseKit(false);
	TickFrames(World, 2);
	if (!TestTrue("Defusing", CT->StartUse()))
	{
		return false;
	}
	Bot->SetActorTickEnabled(true);
	bool bHeard = false;
	for (int32 Frame = 0; Frame < 30 && !bHeard; ++Frame)
	{
		World.Tick(FrameTime);
		bHeard = GetLookAt(*Bot).Equals(BombLocation, 100.0f);
	}
	TestTrue("It heard the defuse at the bomb", bHeard);
	bool bEngaged = false;
	for (int32 Frame = 0; Frame < 60 * 8 && CT->IsAlive(); ++Frame)
	{
		World.Tick(FrameTime);
		bEngaged |= Bot->GetEnemy() == CT;
	}
	TestTrue("It engaged the defuser", bEngaged);
	TestTrue("The defuse never ended", Bomb->GetBombState() == EShooterBombState::Planted || !CT->IsAlive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsRetakeGathersTest, "ShooterGame.Bots.RetakeGathers",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsRetakeGathersTest::RunTest(const FString& Parameters)
{
	// ps2-polish P3b: the bomb planted at site A and the terrorist dead, two counter-terrorists far apart: the first to
	// reach the staging point (RetakeStagingDistance toward the CT spawn) waits there for the second, they call "Go go
	// go!" and go in together; the defuse starts only after both gathered.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, FVector(0.0f, 0.0f, 150.0f), FVector(600.0f, 600.0f, 300.0f));
	(void)GameMode->AddBots(EShooterTeam::CT, 1);
	TickUntilLive(World, *GameMode);
	TArray<AShooterCharacter*> CTs;
	AShooterCharacter* T = nullptr;
	for (AShooterCharacter* Pawn : GameMode->GetPawns())
	{
		if (Pawn->IsAlive() && Pawn->GetTeam() == EShooterTeam::CT)
		{
			CTs.Add(Pawn);
		}
		else if (Pawn->IsAlive() && Pawn->GetTeam() == EShooterTeam::T)
		{
			T = Pawn;
		}
	}
	if (!TestEqual("Two CT bots", CTs.Num(), 2) || !TestNotNull("A terrorist bot", T))
	{
		return false;
	}
	for (AShooterCharacter* Pawn : CTs)
	{
		Freeze(*Pawn);
	}
	if (!TestTrue("Planted", PlantBomb(World, *GameMode, *T, FVector(0.0f, 0.0f, 0.0f))))
	{
		return false;
	}
	// A second terrorist would keep the round going when the planter dies: the planter lives, far away and still.
	T->Reset(FVector(3500.0f, -3500.0f, 0.0f));
	CTs[0]->Reset(FVector(-1800.0f, 0.0f, 0.0f));
	CTs[1]->Reset(FVector(-1800.0f, 2600.0f, 0.0f));
	AShooterBomb* Bomb = GameMode->GetBomb();
	for (AShooterCharacter* Pawn : CTs)
	{
		Pawn->SetDefuseKit(true);
		GetBot(*Pawn)->SetActorTickEnabled(true);
	}
	const AShooterAIController* First = GetBot(*CTs[0]);
	bool bGathered = false;
	bool bWentInAlone = false;
	for (int32 Frame = 0; Frame < 60 * 30 && Bomb->GetBombState() == EShooterBombState::Planted; ++Frame)
	{
		World.Tick(FrameTime);
		const FVector Staging = First->GetRetakeStaging();
		const bool bBothThere = FVector::Dist2D(CTs[0]->GetActorLocation(), Staging) <=
				First->RetakeGroupRadius + First->GoalReachedDistance &&
			FVector::Dist2D(CTs[1]->GetActorLocation(), Staging) <=
				First->RetakeGroupRadius + First->GoalReachedDistance;
		bGathered |= bBothThere;
		bWentInAlone |= !bGathered && (CTs[0]->IsDefusing() || CTs[1]->IsDefusing());
	}
	TestTrue("They gathered", bGathered);
	TestFalse("Nobody defused alone first", bWentInAlone);
	TestTrue("Go go go!", FindRadio(*GameMode, EShooterRadioMessage::GoGoGo) != nullptr);
	TestTrue("Defused", Bomb->GetBombState() == EShooterBombState::Defused);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
