#include "Camera/CameraComponent.h"
#include "CanvasTypes.h"
#include "Components/CapsuleComponent.h"
#include "CoreMinimal.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterCharacter.h"
#include "ShooterCharacterMovement.h"
#include "ShooterGameMode.h"
#include "ShooterHUD.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// ShooterGame's automation tests (ShooterGameTests.exe, the project's test program; RunTests.bat runs them after the
// engine's). They run with the project's config (Game/ShooterGame/Config).

namespace
{

	/** A player start with a team tag, its capsule's centre 92 cm above Feet (UE's start). */
	APlayerStart* SpawnTeamStart(UWorld& World, const FVector& Feet, const TCHAR* Tag)
	{
		APlayerStart* Start = World.SpawnActor<APlayerStart>(Feet + FVector(0.0f, 0.0f, 92.0f), FRotator::ZeroRotator);
		Start->PlayerStartTag = FName(Tag);
		return Start;
	}

	/** Five CT starts along +Y at X = -1000 and five T starts at X = 1000, 150 cm apart, and an untagged start. */
	void SpawnTeamStarts(UWorld& World, TArray<APlayerStart*>& OutCT, TArray<APlayerStart*>& OutT)
	{
		(void)World.SpawnActor<APlayerStart>(FVector(0.0f, 0.0f, 92.0f), FRotator::ZeroRotator);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Y = static_cast<float>(Index) * 150.0f;
			OutCT.Add(SpawnTeamStart(World, FVector(-1000.0f, Y, 0.0f), TEXT("CT")));
			OutT.Add(SpawnTeamStart(World, FVector(1000.0f, Y, 0.0f), TEXT("T")));
		}
	}

	/** The world's game mode (UWorld::SetGameMode: the authority, whose PlayerStateClass the controllers take). */
	AShooterGameMode* SetShooterGameMode(UWorld& World)
	{
		return Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
	}

	/** The pawns of the world's shooter characters. */
	TArray<AShooterCharacter*> GetShooterCharacters(UWorld& World)
	{
		TArray<AShooterCharacter*> Characters;
		for (AActor* Actor : World.PersistentLevel->Actors)
		{
			if (AShooterCharacter* Character = Cast<AShooterCharacter>(Actor))
			{
				Characters.Add(Character);
			}
		}
		return Characters;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameTeamsChooseTeamTest, "ShooterGame.Teams.ChooseTeam",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameTeamsChooseTeamTest::RunTest(const FString& Parameters)
{
	// A player takes ?team= when given, else the smaller team, CT on a tie.
	FScopedTestWorld TestWorld;
	AShooterGameMode* GameMode = SetShooterGameMode(*TestWorld);
	TestTrue("Tie: CT", GameMode->ChooseTeam(FString()) == EShooterTeam::CT);
	TestTrue("Asked for T", GameMode->ChooseTeam(TEXT("?Name=Tester?team=T")) == EShooterTeam::T);
	TestTrue("Asked for CT, any case", GameMode->ChooseTeam(TEXT("?team=ct")) == EShooterTeam::CT);

	AShooterPlayerState* State = TestWorld->SpawnActor<AShooterPlayerState>();
	State->SetTeam(EShooterTeam::CT);
	GameMode->GetGameState().AddPlayerState(State);
	TestEqual("CT has one", GameMode->GetTeamSize(EShooterTeam::CT), 1);
	TestTrue("The smaller team: T", GameMode->ChooseTeam(FString()) == EShooterTeam::T);
	TestTrue("Team tags",
		GetShooterTeamTag(EShooterTeam::T) == FName(TEXT("T")) &&
			GetShooterTeamTag(EShooterTeam::CT) == FName(TEXT("CT")) &&
			GetShooterTeamTag(EShooterTeam::None) == NAME_None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameSpawnTenPawnsAtTeamStartsTest, "ShooterGame.Spawn.TenPawnsAtTeamStarts",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameSpawnTenPawnsAtTeamStartsTest::RunTest(const FString& Parameters)
{
	// bot_add_ct 5 and bot_add_t 5: ten pawns, each standing on a different start of its team; a sixth is refused.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	TArray<APlayerStart*> CTStarts;
	TArray<APlayerStart*> TStarts;
	SpawnTeamStarts(World, CTStarts, TStarts);
	AShooterGameMode* GameMode = SetShooterGameMode(World);

	TestTrue("bot_add_ct 5", GameMode->ProcessConsoleExec(TEXT("bot_add_ct 5"), *GLog, nullptr));
	TestTrue("bot_add_t 5", GameMode->ProcessConsoleExec(TEXT("bot_add_t 5"), *GLog, nullptr));
	TestEqual("A sixth CT is refused", GameMode->AddBots(EShooterTeam::CT, 1), 0);

	const TArray<AShooterCharacter*> Characters = GetShooterCharacters(World);
	if (!TestEqual("Ten pawns", Characters.Num(), 10))
	{
		return false;
	}
	int32 NumCT = 0;
	int32 NumT = 0;
	GameMode->CountPawns(NumCT, NumT);
	TestEqual("Five CT", NumCT, 5);
	TestEqual("Five T", NumT, 5);

	TSet<APlayerStart*> Used;
	for (const AShooterCharacter* Character : Characters)
	{
		const TArray<APlayerStart*>& TeamStarts = Character->GetTeam() == EShooterTeam::CT ? CTStarts : TStarts;
		APlayerStart* Found = nullptr;
		for (APlayerStart* Start : TeamStarts)
		{
			const FVector Feet = Start->GetActorLocation() - FVector(0.0f, 0.0f, 92.0f);
			if (FVector::Dist(Feet, Character->GetActorLocation()) < 0.01f)
			{
				Found = Start;
			}
		}
		TestNotNull("On a start of its team", Found);
		if (Found != nullptr)
		{
			TestFalse("A start of its own", Used.Contains(Found));
			Used.Add(Found);
		}
		TestTrue("A bot's controller", Cast<AShooterAIController>(Character->GetController()) != nullptr);
	}
	TestEqual("Ten different starts", Used.Num(), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameSpawnStandsOnTheStartTest, "ShooterGame.Spawn.StandsOnTheStart",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameSpawnStandsOnTheStartTest::RunTest(const FString& Parameters)
{
	// A start's location is its capsule's centre (UE); the character's feet are its capsule's bottom, facing its yaw.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	APlayerStart* Start = World.SpawnActor<APlayerStart>(FVector(300.0f, 200.0f, 92.0f), FRotator(0.0f, 90.0f, 0.0f));
	Start->PlayerStartTag = FName(TEXT("T"));
	AShooterGameMode* GameMode = SetShooterGameMode(World);
	TestEqual("One bot", GameMode->AddBots(EShooterTeam::T, 1), 1);
	const TArray<AShooterCharacter*> Characters = GetShooterCharacters(World);
	if (!TestEqual("One pawn", Characters.Num(), 1))
	{
		return false;
	}
	TestEqual("Feet on the floor", Characters[0]->GetActorLocation().Z, 0.0f, 1.0e-3f);
	TestEqual("Facing the start's yaw", Characters[0]->GetActorRotation().Yaw, 90.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameCharacterMovementTest, "ShooterGame.Character.Movement",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameCharacterMovementTest::RunTest(const FString& Parameters)
{
	// Counter-Strike's movement (ShooterCharacterMovement.h's table): UE's velocity model, 635 cm/s running, the walk
	// key's 52 % from the config, crouching allowed, the first-person camera at the eyes.
	FScopedTestWorld TestWorld;
	AShooterCharacter* Character = TestWorld->SpawnActor<AShooterCharacter>();
	Character->Reset(FVector::ZeroVector);
	UShooterCharacterMovement* Move = Character->GetShooterCharacterMovement();
	if (!TestNotNull("The game's movement class", Move))
	{
		return false;
	}
	TestFalse("UE's velocity model", Move->bInstantVelocity);
	TestEqual("Run speed", Move->GetMaxSpeed(), 635.0f);
	Character->SetWalking(true);
	TestEqual("Walk speed", Move->GetMaxSpeed(), 635.0f * 0.52f, 0.01f);
	Character->SetWalking(false);
	TestTrue("Can crouch", Character->CanCrouch());
	TestEqual("Capsule radius", Character->GetCapsule().GetCapsuleRadius(), 40.0f);
	TestTrue("First-person camera", Character->GetFirstPersonCameraComponent()->bUsePawnControlRotation);
	TestEqual(
		"Eyes", Character->GetFirstPersonCameraComponent()->RelativeLocation.Z, AShooterCharacter::StandingEyeHeight);

	// A second of running from rest on the floor plane: 0.2 s to full speed.
	FPhysScene Scene;
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, 1.0f / 60.0f);
	}
	TestEqual("Full speed", Move->Velocity.X, 635.0f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDDrawsCrosshairTest, "ShooterGame.HUD.DrawsCrosshair",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDDrawsCrosshairTest::RunTest(const FString& Parameters)
{
	// The HUD draws the crosshair's four arms into the frame's canvas (two triangles each).
	FScopedTestWorld TestWorld;
	AShooterHUD* HUD = TestWorld->SpawnActor<AShooterHUD>();
	FCanvas Canvas(1280, 720);
	HUD->Paint(Canvas);
	TArray<FCanvasVertex> Vertices;
	Canvas.GetTriangles(Vertices);
	TestEqual("Four arms", Vertices.Num(), 4 * 6);
	float MinX = 1.0e9f;
	float MaxX = -1.0e9f;
	for (const FCanvasVertex& Vertex : Vertices)
	{
		MinX = FMath::Min(MinX, Vertex.X);
		MaxX = FMath::Max(MaxX, Vertex.X);
	}
	TestEqual("Centred", (MinX + MaxX) * 0.5f, 640.0f, 0.01f);
	TestEqual("Arms' reach", MaxX - 640.0f, HUD->CrosshairGap + HUD->CrosshairLength, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameConfigTest, "ShooterGame.Config.InputAndChannels",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameConfigTest::RunTest(const FString& Parameters)
{
	// The project's config: the key mappings, the mouse sensitivity and the Weapon channel.
	const UInputSettings* Settings = GetDefault<UInputSettings>();
	TArray<FInputActionKeyMapping> Crouch;
	Settings->GetActionMappingByName(TEXT("Crouch"), Crouch);
	TestEqual("Crouch keys", Crouch.Num(), 2);
	TArray<FInputActionKeyMapping> Walk;
	Settings->GetActionMappingByName(TEXT("Walk"), Walk);
	TestTrue("Walk: Left Shift", Walk.Num() == 1 && Walk[0].Key == EKeys::LeftShift);
	TArray<FInputAxisKeyMapping> MoveUp;
	Settings->GetAxisMappingByName(TEXT("MoveUp"), MoveUp);
	TestEqual("No flying", MoveUp.Num(), 0);
	float MouseSensitivity = 0.0f;
	for (const FInputAxisConfigEntry& Entry : Settings->AxisConfig)
	{
		if (Entry.AxisKeyName == FName(TEXT("MouseX")))
		{
			MouseSensitivity = Entry.AxisProperties.Sensitivity;
		}
	}
	TestEqual("Mouse sensitivity", MouseSensitivity, 0.07f, 1.0e-6f);
	TestTrue("The Weapon channel",
		UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(ECC_GameTraceChannel1) == FName(TEXT("Weapon")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
