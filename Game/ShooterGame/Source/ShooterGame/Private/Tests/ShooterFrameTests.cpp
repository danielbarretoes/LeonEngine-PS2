#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/PointLight.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterHUD.h"
#include "ShooterPawnSensingComponent.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/StrongObjectPtr.h"
#include "Weapons/ShooterWeapon_Instant.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-shipping N20's tests: what ShooterGame no longer does every frame on the EE. The game mode's registries replace
// the walks over the level's actors, the bots trace only to living enemies and take turns to look, the muzzle flashes
// reuse a pool of lights, and the HUD formats its text only when it changes.

namespace
{

	constexpr float FrameTime = 1.0f / 60.0f;

	void TickFrames(UWorld& World, int32 Frames, float Step = FrameTime)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(Step);
		}
	}

	/** A floor 80 m square, its top at Z = 0. */
	void SpawnFloor(UWorld& World)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
	}

	/** A character on the floor at Feet facing Yaw, possessed by a brainless bot of Team (its senses tick alone). */
	AShooterCharacter* SpawnShooter(UWorld& World, const FVector& Feet, float Yaw, EShooterTeam Team)
	{
		AShooterCharacter* Character = World.SpawnActor<AShooterCharacter>(Feet, FRotator(0.0f, Yaw, 0.0f));
		AShooterAIController* Controller = World.SpawnActor<AShooterAIController>();
		Controller->SetActorTickEnabled(false);
		if (AShooterPlayerState* State = Controller->GetPlayerState<AShooterPlayerState>())
		{
			State->SetTeam(Team);
		}
		Controller->Possess(Character);
		Controller->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
		return Character;
	}

	/**
	 * The open map of the bot tests: a floor, five starts a team (CT at X = -1500, T at X = 1500) in their buy zones,
	 * bomb site A in the middle; short phases; NumCT and NumT bots.
	 */
	AShooterGameMode* SetUpBotMatch(UWorld& World, int32 NumCT, int32 NumT)
	{
		SpawnFloor(World);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Y = (static_cast<float>(Index) - 2.0f) * 150.0f;
			APlayerStart* CTStart = World.SpawnActor<APlayerStart>(FVector(-1500.0f, Y, 92.0f), FRotator::ZeroRotator);
			CTStart->PlayerStartTag = FName(TEXT("CT"));
			APlayerStart* TStart =
				World.SpawnActor<APlayerStart>(FVector(1500.0f, Y, 92.0f), FRotator(0.0f, 180.0f, 0.0f));
			TStart->PlayerStartTag = FName(TEXT("T"));
		}
		auto SpawnZone = [&World](const FVector& Center, const FVector& Size, const TCHAR* Kind, const TCHAR* Name)
		{
			ATriggerVolume* Zone = World.SpawnActor<ATriggerVolume>(
				ATriggerVolume::StaticClass(), FTransform(FQuat::Identity, Center, Size / 100.0f));
			Zone->Tags.Add(FName(Kind));
			Zone->Tags.Add(FName(Name));
		};
		SpawnZone(FVector(-1500.0f, 0.0f, 150.0f), FVector(600.0f, 1000.0f, 300.0f), TEXT("BuyZone"), TEXT("CT"));
		SpawnZone(FVector(1500.0f, 0.0f, 150.0f), FVector(600.0f, 1000.0f, 300.0f), TEXT("BuyZone"), TEXT("T"));
		SpawnZone(FVector(0.0f, 0.0f, 150.0f), FVector(600.0f, 600.0f, 300.0f), TEXT("BombSite"), TEXT("A"));
		AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
		GameMode->bFillTeamsWithBots = false;
		GameMode->FreezeTime = 0.5f;
		GameMode->RoundTime = 60.0f;
		GameMode->RoundRestartDelay = 0.5f;
		GameMode->RandomSeed = 3;
		(void)GameMode->AddBots(EShooterTeam::CT, NumCT);
		(void)GameMode->AddBots(EShooterTeam::T, NumT);
		return GameMode;
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

	/** The bots' senses, and how many looks each has made. */
	TArray<UShooterPawnSensingComponent*> GetBotSenses(const AShooterGameMode& GameMode)
	{
		TArray<UShooterPawnSensingComponent*> Senses;
		for (const AShooterCharacter* Pawn : GameMode.GetPawns())
		{
			if (const AShooterAIController* Bot = Cast<AShooterAIController>(Pawn->GetController()))
			{
				Senses.Add(Bot->GetPawnSensing());
			}
		}
		return Senses;
	}

	/** The point lights in the level. */
	int32 CountPointLights(const UWorld& World)
	{
		int32 Count = 0;
		World.ForEach<APointLight>([&Count](APointLight&) { ++Count; });
		return Count;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRegistryMapOnDeLeonTest, "ShooterGame.Registry.MapOnDeLeon",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRegistryMapOnDeLeonTest::RunTest(const FString& Parameters)
{
	// Once de_leon is loaded the game mode holds its bomb sites (sorted, with their floors), both buy zones and the ten
	// team starts, found once; the pawns join as they begin play.
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	FString Error;
	if (!TestEqual("Browse",
			static_cast<int32>(
				Engine->Browse(Context, FURL(nullptr, TEXT("/Game/Maps/de_leon"), TRAVEL_Absolute), Error)),
			static_cast<int32>(EBrowseReturnVal::Success)))
	{
		AddError(Error);
		Engine->PreExit();
		return false;
	}
	UWorld* World = Engine->GetGameWorld();
	AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	if (!TestNotNull("ShooterGameMode", GameMode))
	{
		Engine->PreExit();
		return false;
	}
	const TArray<FName>& Sites = GameMode->GetBombSiteNames();
	TestEqual("Two bomb sites", Sites.Num(), 2);
	TestTrue("A then B", Sites.Num() == 2 && Sites[0] == FName(TEXT("A")) && Sites[1] == FName(TEXT("B")));
	TestTrue("The same array every call (no copy, no sort)", &GameMode->GetBombSiteNames() == &Sites);
	int32 NumBuyZones = 0;
	for (const ATriggerVolume* Zone : GameMode->GetZones())
	{
		NumBuyZones += Zone->ActorHasTag(AShooterGameMode::BuyZoneTag) ? 1 : 0;
	}
	TestEqual("Two buy zones", NumBuyZones, 2);
	TestTrue("Ten team starts", GameMode->GetPlayerStarts().Num() >= 10);
	for (const FName& Site : Sites)
	{
		FVector Floor = FVector::ZeroVector;
		TestTrue(
			*FString::Printf(TEXT("Site %s's floor"), *Site.ToString()), GameMode->GetBombSiteLocation(Site, Floor));
		const ATriggerVolume* Zone = GameMode->FindZone(Floor, AShooterGameMode::BombSiteTag, Site);
		TestTrue(*FString::Printf(TEXT("Its middle is in site %s"), *Site.ToString()),
			Zone != nullptr && AShooterGameMode::GetZoneName(*Zone, AShooterGameMode::BombSiteTag) == Site);
	}
	FVector Spawn = FVector::ZeroVector;
	TestTrue("The CT spawn", GameMode->GetTeamSpawnLocation(EShooterTeam::CT, Spawn) && Spawn.X > 0.0f);
	TestTrue("The T spawn", GameMode->GetTeamSpawnLocation(EShooterTeam::T, Spawn) && Spawn.X < 0.0f);
	(void)GameMode->FillTeamsWithBots();
	for (int32 Frame = 0; Frame < 2; ++Frame)
	{
		Engine->Tick(FrameTime, false);
	}
	int32 NumCT = 0;
	int32 NumT = 0;
	GameMode->CountPawns(NumCT, NumT);
	TestEqual("The pawns of both teams are registered", GameMode->GetPawns().Num(), NumCT + NumT);
	TestEqual("Five a side", NumCT + NumT, 10);
	Engine->PreExit();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRegistryPickupsTest, "ShooterGame.Registry.PickupsAndPawns",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRegistryPickupsTest::RunTest(const FString& Parameters)
{
	// A weapon on the floor is a pickup until a pawn takes it or it is destroyed; a pawn is registered from its
	// BeginPlay to its EndPlay; a volume spawned after the game mode joins the zones.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
	AShooterCharacter* Dropper = SpawnShooter(World, FVector(0.0f, 0.0f, 0.0f), 0.0f, EShooterTeam::CT);
	TestTrue("The pawn is registered", GameMode->GetPawns().Contains(Dropper));
	AShooterWeapon* Rifle = Dropper->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("ak47")));
	TestNotNull("A rifle", Rifle);
	TestTrue("Dropped", Dropper->DropWeapon(Rifle) && Rifle->IsDropped());
	TestTrue("A pickup", GameMode->GetPickups().Contains(Rifle));
	(void)Rifle->Destroy();
	TestFalse("A destroyed pickup leaves the registry", GameMode->GetPickups().Contains(Rifle));

	AShooterWeapon* Second = Dropper->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("ak47")));
	TestTrue("Dropped again", Dropper->DropWeapon(Second));
	TestTrue("Another pickup", GameMode->GetPickups().Contains(Second));
	// A pawn without a primary walks onto it after the pickup delay.
	AShooterCharacter* Taker =
		SpawnShooter(World, Second->GetActorLocation() + FVector(0.0f, 0.0f, 5.0f), 0.0f, EShooterTeam::T);
	TickFrames(World, FMath::CeilToInt((Second->PickupDelay + 0.2f) / FrameTime));
	TestTrue("Taken", Taker->GetWeaponInSlot(EShooterWeaponSlot::Primary) == Second && !Second->IsDropped());
	TestFalse("A taken pickup leaves the registry", GameMode->GetPickups().Contains(Second));

	(void)Taker->Destroy();
	TestFalse("A destroyed pawn leaves the registry", GameMode->GetPawns().Contains(Taker));

	ATriggerVolume* Late = World.SpawnActor<ATriggerVolume>(ATriggerVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(0.0f, 0.0f, 150.0f), FVector(6.0f, 6.0f, 3.0f)));
	Late->Tags.Add(AShooterGameMode::BombSiteTag);
	Late->Tags.Add(FName(TEXT("B")));
	TestTrue("A spawned volume joins the zones", GameMode->GetZones().Contains(Late));
	TestTrue("It is a site", GameMode->GetBombSiteNames().Contains(FName(TEXT("B"))));
	TestTrue("Found at a pawn's feet",
		GameMode->FindZone(FVector::ZeroVector, AShooterGameMode::BombSiteTag, NAME_None) == Late);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsSensingFilterTest, "ShooterGame.Bots.SensingFilter",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsSensingFilterTest::RunTest(const FString& Parameters)
{
	// A bot traces to nobody it would ignore: a teammate, a dead enemy and the spectator stand in its view and cost no
	// line of sight trace; a living enemy does, and is seen. It listens to the enemy's noises only.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
	GameMode->bBotStop = true;
	AShooterCharacter* Bot = SpawnShooter(World, FVector(0.0f, 0.0f, 0.0f), 0.0f, EShooterTeam::CT);
	UShooterPawnSensingComponent* Sensing = Cast<AShooterAIController>(Bot->GetController())->GetPawnSensing();
	const AShooterCharacter* Teammate = SpawnShooter(World, FVector(600.0f, -200.0f, 0.0f), 180.0f, EShooterTeam::CT);
	AShooterCharacter* DeadEnemy = SpawnShooter(World, FVector(700.0f, 200.0f, 0.0f), 180.0f, EShooterTeam::T);
	DeadEnemy->Suicide();
	const ASpectatorPawn* Spectator =
		World.SpawnActor<ASpectatorPawn>(FVector(500.0f, 0.0f, 150.0f), FRotator(0.0f, 180.0f, 0.0f));
	TestFalse("Not the teammate", Sensing->ShouldCheckVisibilityOf(Teammate));
	TestFalse("Not the dead", Sensing->ShouldCheckVisibilityOf(DeadEnemy));
	TestFalse("Not the spectator", Sensing->ShouldCheckVisibilityOf(Spectator));
	const int32 TracesBefore = Sensing->GetNumSightTraces();
	Sensing->UpdateAISensing();
	TestEqual("No trace to them", Sensing->GetNumSightTraces(), TracesBefore);

	AShooterCharacter* Enemy = SpawnShooter(World, FVector(800.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
	TestTrue("The living enemy", Sensing->ShouldCheckVisibilityOf(Enemy));
	Sensing->UpdateAISensing();
	TestTrue("It is traced to", Sensing->GetNumSightTraces() > TracesBefore);
	TestTrue("And heard", Sensing->ShouldCheckAudibilityOf(Enemy));
	TestFalse("A teammate's shot is not listened to", Sensing->ShouldCheckAudibilityOf(Teammate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsSensingStaggerTest, "ShooterGame.Bots.SensingStagger",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsSensingStaggerTest::RunTest(const FString& Parameters)
{
	// Ten bots look ten times a second each, spread over the interval: at 60 fps one or two look in a frame (they all
	// looked in the same frame before). At 30 fps the frame's budget (MaxSensingUpdatesPerFrame) holds them to two a
	// frame too, and the bots look in turn: each as often as the others.
	for (const float Step : {1.0f / 60.0f, 1.0f / 30.0f})
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpBotMatch(World, 5, 5);
		GameMode->bBotStop = true;
		TickUntilLive(World, *GameMode);
		const TArray<UShooterPawnSensingComponent*> Senses = GetBotSenses(*GameMode);
		TestEqual("Ten bots", Senses.Num(), 10);
		TArray<int32> Looks;
		for (const UShooterPawnSensingComponent* Sensing : Senses)
		{
			Looks.Add(Sensing->GetNumSightUpdates());
		}
		const TArray<int32> LooksAtStart = Looks;
		const int32 Frames = FMath::RoundToInt(1.0f / Step);
		int32 MostInAFrame = 0;
		int32 FramesWithLooks = 0;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(Step);
			int32 InThisFrame = 0;
			for (int32 Index = 0; Index < Senses.Num(); ++Index)
			{
				InThisFrame += Senses[Index]->GetNumSightUpdates() - Looks[Index];
				Looks[Index] = Senses[Index]->GetNumSightUpdates();
			}
			MostInAFrame = FMath::Max(MostInAFrame, InThisFrame);
			FramesWithLooks += InThisFrame > 0 ? 1 : 0;
			TestTrue(
				"The frame's budget", GameMode->GetSensingUpdatesThisFrame() <= GameMode->MaxSensingUpdatesPerFrame);
		}
		const FString At = FString::Printf(TEXT(" at %d fps"), Frames);
		TestTrue(*(TEXT("At most two bots look in a frame") + At), MostInAFrame <= 2);
		TestTrue(*(TEXT("The looks spread over the frames") + At), FramesWithLooks >= Frames / 2);
		int32 Total = 0;
		for (int32 Index = 0; Index < Senses.Num(); ++Index)
		{
			Total += Looks[Index] - LooksAtStart[Index];
		}
		TestTrue(*(TEXT("Every bot looked") + At),
			!Senses.ContainsByPredicate(
				[](const UShooterPawnSensingComponent* Sensing) { return Sensing->GetNumSightUpdates() == 0; }));
		// In turn: every bot looks as often as the others (10 Hz each at 60 fps, the cadence kept; fewer at 30 fps).
		int32 FewestLooks = MAX_int32;
		int32 MostLooks = 0;
		for (int32 Index = 0; Index < Senses.Num(); ++Index)
		{
			const int32 BotLooks = Looks[Index] - LooksAtStart[Index];
			FewestLooks = FMath::Min(FewestLooks, BotLooks);
			MostLooks = FMath::Max(MostLooks, BotLooks);
		}
		TestTrue(*(TEXT("The bots look as often as each other") + At), MostLooks - FewestLooks <= 1);
		if (Frames == 60)
		{
			TestTrue(TEXT("Ten looks a second at 60 fps"), FewestLooks >= 9 && MostLooks <= 11);
		}
		TestTrue(*(TEXT("At most two looks a frame") + At), Total <= 2 * Frames);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameEffectsMuzzleFlashPoolTest, "ShooterGame.Effects.MuzzleFlashPool",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameEffectsMuzzleFlashPoolTest::RunTest(const FString& Parameters)
{
	// Each shot lights a muzzle flash from the world's pool: once warm, a rifle's long burst spawns no light actor, and
	// the flashes go out when their time is up.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterCharacter* Shooter = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::None);
	AShooterWeapon* Rifle = Shooter->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("ak47")));
	Shooter->EquipWeapon(Rifle);
	TickFrames(World, FMath::CeilToInt((Rifle->EquipDuration + 0.1f) / FrameTime));
	TestEqual("No light yet", CountPointLights(World), 0);
	Shooter->StartWeaponFire();
	TickFrames(World, 30);
	const int32 ShotsWarm = Rifle->GetShotsFired();
	const int32 LightsWarm = CountPointLights(World);
	TestTrue("Shots fired", ShotsWarm >= 5);
	TestTrue("The pool's lights", LightsWarm > 0 && LightsWarm <= UWorld::MaxPooledPointLights);
	TickFrames(World, 90);
	Shooter->StopWeaponFire();
	TestTrue("More shots", Rifle->GetShotsFired() > ShotsWarm + 10);
	TestEqual("No light spawned after the warm-up", CountPointLights(World), LightsWarm);
	TickFrames(World, 10);
	int32 LightsOn = 0;
	World.ForEach<APointLight>(
		[&LightsOn](APointLight& Light) { LightsOn += Light.GetPointLightComponent()->IsVisible() ? 1 : 0; });
	TestEqual("The flashes went out", LightsOn, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDTextCacheTest, "ShooterGame.HUD.TextCache",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDTextCacheTest::RunTest(const FString& Parameters)
{
	// The HUD formats a line of text only when what it shows changes: drawn again with nothing changed, it formats
	// nothing; a new score formats that score's line only, a kill the feed's line.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, 1, 1);
	GameMode->bBotStop = true;
	TickUntilLive(World, *GameMode);
	AShooterHUD* HUD = World.SpawnActor<AShooterHUD>();
	auto Paint = [HUD]()
	{
		FCanvas Canvas(1280, 720);
		HUD->Paint(Canvas);
	};
	Paint();
	const int32 First = HUD->GetNumTextFormats();
	TestTrue("The first frame formats its lines", First > 0);
	Paint();
	Paint();
	TestEqual("Nothing changed: nothing formatted", HUD->GetNumTextFormats(), First);
	AShooterGameState* State = GameMode->GetShooterGameState();
	State->AddTeamScore(EShooterTeam::CT);
	Paint();
	TestEqual("A new score: one line", HUD->GetNumTextFormats(), First + 1);
	FShooterKillFeedEntry Entry;
	Entry.KillerName = TEXT("Albert");
	Entry.VictimName = TEXT("Allen");
	Entry.WeaponName = TEXT("ak47");
	Entry.Time = World.GetTimeSeconds();
	State->AddKillFeedEntry(Entry);
	Paint();
	const int32 AfterKill = HUD->GetNumTextFormats();
	TestEqual("A kill: its three parts", AfterKill, First + 1 + 3);
	Paint();
	TestEqual("Then nothing again", HUD->GetNumTextFormats(), AfterKill);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
