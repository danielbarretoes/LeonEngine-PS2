#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** A static 1 m box body centred at Center, added without a component. */
	int32 AddChannelTestBox(FPhysScene& Scene, const FVector& Center, EBodyType Type = EBodyType::Static)
	{
		const int32 Id = Scene.AddBody({static_cast<SIZE_T>(100 + Scene.GetBodies().Num()), Type, 1.0f, true});
		Scene.GetBodies()[Id].Position = Center;
		Scene.GetBodies()[Id].HalfExtents = FVector(50.0f);
		return Id;
	}

	/** The horizontal line the channel tests trace: along +Y at 100 cm, through the origin. */
	const FVector ChannelTraceStart(0.0f, -500.0f, 100.0f);
	const FVector ChannelTraceEnd(0.0f, 500.0f, 100.0f);

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionChannelResponseContainerTest,
	"System.Engine.CollisionChannel.ResponseContainer",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionChannelResponseContainerTest::RunTest(const FString& Parameters)
{
	// UE's container: every channel blocks, one member per channel, the weaker response wins in a min container.
	FCollisionResponseContainer Container;
	TestEqual("Default blocks", Container.GetResponse(ECC_GameTraceChannel18), ECR_Block);
	TestTrue("Set changes", Container.SetResponse(ECC_Visibility, ECR_Ignore));
	TestFalse("Set again does not", Container.SetResponse(ECC_Visibility, ECR_Ignore));
	TestEqual("The named member", Container.Visibility.GetValue(), ECR_Ignore);
	TestEqual("Other channels keep blocking", Container.GetResponse(ECC_Camera), ECR_Block);
	TestEqual("Past the container", Container.GetResponse(ECC_MAX), ECR_Ignore);

	FCollisionResponseContainer Overlaps(ECR_Overlap);
	const FCollisionResponseContainer Min = FCollisionResponseContainer::CreateMinContainer(Container, Overlaps);
	TestEqual("Min: ignore", Min.GetResponse(ECC_Visibility), ECR_Ignore);
	TestEqual("Min: overlap", Min.GetResponse(ECC_Pawn), ECR_Overlap);

	TestTrue("Replace overlaps", Overlaps.ReplaceChannels(ECR_Overlap, ECR_Block));
	TestTrue("Now equal to the default", Overlaps == FCollisionResponseContainer());
	TestTrue("Set all", Overlaps.SetAllChannels(ECR_Ignore));
	TestEqual("All ignore", Overlaps.GetResponse(ECC_WorldStatic), ECR_Ignore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionChannelRawBodyDefaultsTest, "System.Engine.CollisionChannel.RawBodyDefaults",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionChannelRawBodyDefaultsTest::RunTest(const FString& Parameters)
{
	// A body added without a component keeps the pre-P17 channel filter as its default responses.
	FPhysScene Scene;
	const FBodyInstance Static = Scene.GetBodies()[AddChannelTestBox(Scene, FVector::ZeroVector)];
	const FBodyInstance Dynamic =
		Scene.GetBodies()[AddChannelTestBox(Scene, FVector(500.0f, 0.0f, 0.0f), EBodyType::Dynamic)];
	TestEqual("Static object type", Static.ObjectType.GetValue(), ECC_WorldStatic);
	TestEqual("Static ignores WorldDynamic traces", Static.GetResponseToChannel(ECC_WorldDynamic), ECR_Ignore);
	TestEqual("Static blocks Visibility", Static.GetResponseToChannel(ECC_Visibility), ECR_Block);
	TestEqual("Dynamic object type", Dynamic.ObjectType.GetValue(), ECC_WorldDynamic);
	TestEqual("Dynamic ignores WorldStatic traces", Dynamic.GetResponseToChannel(ECC_WorldStatic), ECR_Ignore);
	TestEqual("Dynamic blocks a game channel", Dynamic.GetResponseToChannel(ECC_GameTraceChannel1), ECR_Block);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionChannelTracesFollowResponsesTest,
	"System.Engine.CollisionChannel.TracesFollowResponses",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionChannelTracesFollowResponsesTest::RunTest(const FString& Parameters)
{
	// Two boxes on the line: the near one's response to the channel decides whether it blocks, is touched or is
	// passed through; the far one always blocks.
	FPhysScene Scene;
	const int32 Near = AddChannelTestBox(Scene, FVector(0.0f, -200.0f, 100.0f));
	const int32 Far = AddChannelTestBox(Scene, FVector(0.0f, 200.0f, 100.0f));
	const SIZE_T NearID = Scene.GetBodies()[Near].ComponentID;
	const SIZE_T FarID = Scene.GetBodies()[Far].ComponentID;

	FHitResult Hit;
	TestTrue("Block: hit", Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Camera));
	TestTrue("Block: the near box", Hit.ComponentID == NearID);
	TestEqual("Block: its body index", Hit.BodyIndex, Near);

	(void)Scene.GetBodies()[Near].CollisionResponses.SetResponse(ECC_Camera, ECR_Overlap);
	TArray<FHitResult> Hits;
	TestTrue(
		"Overlap: multi hits", Scene.LineTraceMultiByChannel(Hits, ChannelTraceStart, ChannelTraceEnd, ECC_Camera));
	if (TestEqual("Overlap: two hits", Hits.Num(), 2))
	{
		TestFalse("Overlap: the near one only touches", Hits[0].bBlockingHit);
		TestTrue("Overlap: the far one blocks", Hits[1].bBlockingHit);
	}
	TestTrue(
		"Overlap: single hit", Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Camera));
	TestTrue("Overlap: single skips the touch", Hit.ComponentID == FarID);

	(void)Scene.GetBodies()[Near].CollisionResponses.SetResponse(ECC_Camera, ECR_Ignore);
	TestTrue("Ignore: multi", Scene.LineTraceMultiByChannel(Hits, ChannelTraceStart, ChannelTraceEnd, ECC_Camera));
	TestEqual("Ignore: one hit", Hits.Num(), 1);
	TestTrue("Ignore: another channel still blocks",
		Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Visibility) &&
			Hit.ComponentID == NearID);

	// The query's response to the object type: ignoring WorldStatic objects skips both boxes.
	FCollisionResponseParams IgnoreStatic;
	(void)IgnoreStatic.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Ignore);
	TestFalse("Query ignores the object type",
		Scene.LineTraceSingleByChannel(
			Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Visibility, {}, nullptr, IgnoreStatic));

	// A body whose collision is not query-enabled is never hit.
	Scene.GetBodies()[Far].bQueryEnabled = false;
	TestFalse("Not query-enabled", Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Camera));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionChannelObjectTypeQueryTest, "System.Engine.CollisionChannel.ObjectTypeQuery",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionChannelObjectTypeQueryTest::RunTest(const FString& Parameters)
{
	// An object query looks for object types, whatever the responses (UE: LineTraceSingleByObjectType).
	FPhysScene Scene;
	const int32 Static = AddChannelTestBox(Scene, FVector(0.0f, -200.0f, 100.0f));
	const int32 Dynamic = AddChannelTestBox(Scene, FVector(0.0f, 200.0f, 100.0f), EBodyType::Dynamic);
	(void)Scene.GetBodies()[Static].CollisionResponses.SetAllChannels(ECR_Ignore);

	FHitResult Hit;
	TestTrue("WorldStatic objects",
		Scene.LineTraceSingleByObjectType(
			Hit, ChannelTraceStart, ChannelTraceEnd, FCollisionObjectQueryParams(ECC_WorldStatic)) &&
			Hit.BodyIndex == Static);
	TestTrue("WorldDynamic objects",
		Scene.LineTraceSingleByObjectType(
			Hit, ChannelTraceStart, ChannelTraceEnd, FCollisionObjectQueryParams(ECC_WorldDynamic)) &&
			Hit.BodyIndex == Dynamic);
	TestFalse("Pawn objects",
		Scene.LineTraceSingleByObjectType(
			Hit, ChannelTraceStart, ChannelTraceEnd, FCollisionObjectQueryParams(ECC_Pawn)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionChannelComponentSettingsTest,
	"System.Engine.CollisionChannel.ComponentSettings",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionChannelComponentSettingsTest::RunTest(const FString& Parameters)
{
	// A component's object type and responses become its body's, and changing them recreates the body.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ABlockingVolume* Wall = World.SpawnActor<ABlockingVolume>(
		ABlockingVolume::StaticClass(), FTransform(FQuat::Identity, FVector(0.0f, 0.0f, 100.0f), FVector(1.0f)));
	UBoxComponent* Brush = Cast<UBoxComponent>(Wall->GetRootComponent());
	if (!TestNotNull("Brush", Brush))
	{
		return false;
	}
	FPhysScene& Scene = World.GetPhysicsScene();
	FHitResult Hit;
	TestTrue("Blocks by default",
		Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_GameTraceChannel2));
	TestTrue("The hit names the actor", Hit.GetActor() == Wall);
	TestTrue("The hit names the component", Hit.GetComponent() == Brush);

	Brush->SetCollisionObjectType(ECC_WorldDynamic);
	Brush->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	const int32 BodyIndex = Scene.FindComponentBody(*Brush);
	if (!TestTrue("Body recreated", BodyIndex != INDEX_NONE))
	{
		return false;
	}
	TestEqual("Object type", Scene.GetBodies()[BodyIndex].ObjectType.GetValue(), ECC_WorldDynamic);
	TestFalse("Ignores the game channel now",
		Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_GameTraceChannel2));

	// Ignored actors and components (UE: FCollisionQueryParams::AddIgnoredActor / AddIgnoredComponent).
	FCollisionQueryParams IgnoreWall(FName(TEXT("Test")), false, Wall);
	TestFalse("Ignored actor",
		Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Visibility, IgnoreWall));
	FCollisionQueryParams IgnoreBrush;
	IgnoreBrush.AddIgnoredComponent(Brush);
	TestFalse("Ignored component",
		Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Visibility, IgnoreBrush));
	TestTrue("Not ignored", Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Visibility));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionChannelTracesHitPawnCapsuleTest,
	"System.Engine.CollisionChannel.TracesHitPawnCapsule",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionChannelTracesHitPawnCapsuleTest::RunTest(const FString& Parameters)
{
	// A character's capsule is a query-only Pawn body standing on the feet: traces on the world and game channels hit
	// the capsule's surface; Visibility and Pawn traces pass through (the Pawn profile, and Leon's pawn separation).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(0.0f, 0.0f, 0.0f));
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	FPhysScene& Scene = World.GetPhysicsScene();
	const int32 BodyIndex = Scene.FindComponentBody(*Capsule);
	if (!TestTrue("The capsule has a body", BodyIndex != INDEX_NONE))
	{
		return false;
	}
	const FBodyInstance& Body = Scene.GetBodies()[BodyIndex];
	TestTrue("Capsule shape", Body.CollisionShape == EBodyCollisionShape::Capsule);
	TestEqual("Pawn object", Body.ObjectType.GetValue(), ECC_Pawn);
	TestFalse("Query only", Body.bPhysicsEnabled);
	TestEqual("Centre above the feet", Body.Position.Z, 92.5f, 1.0e-3f);

	FHitResult Hit;
	TestTrue("A game channel hits it",
		UGameplayStatics::LineTraceSingleByChannel(
			World, Hit, ChannelTraceStart, ChannelTraceEnd, ECC_GameTraceChannel1));
	TestTrue("The character", Hit.GetActor() == Character);
	TestTrue("The capsule", Hit.GetComponent() == Capsule);
	TestEqual("On the capsule's surface", Hit.ImpactPoint.Y, -35.0f, 0.05f);
	TestEqual("Facing the trace", Hit.ImpactNormal.Y, -1.0f, 1.0e-3f);
	TestFalse(
		"Visibility passes", Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Visibility));
	TestFalse("Pawn passes", Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Pawn));

	// Above the head and through the round top: the caps are round, not the bounding box's corners.
	TestFalse("Over the head",
		Scene.LineTraceSingleByChannel(Hit, FVector(0.0f, -500.0f, 190.0f), FVector(0.0f, 500.0f, 190.0f), ECC_Camera));
	TestFalse("Through the bounding box's corner, past the round top",
		Scene.LineTraceSingleByChannel(
			Hit, FVector(30.0f, -500.0f, 180.0f), FVector(30.0f, 500.0f, 180.0f), ECC_Camera));

	// The body follows the character when it moves (SendPhysicsTransform after the movement).
	Character->Reset(FVector(0.0f, 300.0f, 0.0f));
	TestTrue("Moved: hit further", Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Camera));
	TestEqual("Moved: new surface", Hit.ImpactPoint.Y, 265.0f, 0.05f);

	// A sphere sweep stops a radius away from the capsule.
	TestTrue("Sphere sweep",
		Scene.SweepSingleByChannel(Hit, FVector(0.0f, -500.0f, 100.0f), FVector(0.0f, 500.0f, 100.0f), FQuat::Identity,
			ECC_Camera, FCollisionShape::MakeSphere(10.0f)));
	TestEqual("Sphere centre", Hit.Location.Y, 255.0f, 0.05f);

	// The character never hits its own capsule, and an ignored actor is left out.
	FCollisionQueryParams IgnoreCharacter;
	IgnoreCharacter.AddIgnoredActor(Character);
	TestFalse("Ignored character",
		Scene.LineTraceSingleByChannel(Hit, ChannelTraceStart, ChannelTraceEnd, ECC_Camera, IgnoreCharacter));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionChannelCharacterMovesPastCapsulesTest,
	"System.Engine.CollisionChannel.CharacterMovesPastCapsules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionChannelCharacterMovesPastCapsulesTest::RunTest(const FString& Parameters)
{
	// The movement ignores its own capsule and the other pawns' (the world separates pawns, not the movement): a
	// character walking through another one's place is not stopped by its body.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Walker = World.SpawnActor<ACharacter>();
	Walker->Reset(FVector(0.0f, 0.0f, 0.0f));
	ACharacter* Other = World.SpawnActor<ACharacter>();
	Other->Reset(FVector(300.0f, 0.0f, 0.0f)); // on the walker's path
	FPhysScene& Scene = World.GetPhysicsScene();
	const float Speed = Walker->GetCharacterMovement().MaxWalkSpeed;
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Walker->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Walker->PerformMovement(Scene, 1.0f / 60.0f);
		Walker->GetCapsuleComponent()->SendPhysicsTransform();
	}
	TestEqual("Walked at full speed", Walker->GetActorLocation().X, Speed, 0.5f);
	TestEqual("Stayed on the floor", Walker->GetActorLocation().Z, 0.0f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionChannelProfileNamesChannelsTest,
	"System.Engine.CollisionChannel.ProfileNamesChannels",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionChannelProfileNamesChannelsTest::RunTest(const FString& Parameters)
{
	// UCollisionProfile names the engine channels and the config's game channels, and sets their default responses.
	UCollisionProfile* Profile = NewObject<UCollisionProfile>();
	FCustomChannelSetup Weapon;
	Weapon.Channel = ECC_GameTraceChannel1;
	Weapon.DefaultResponse = ECR_Block;
	Weapon.bTraceType = true;
	Weapon.Name = FName(TEXT("Weapon"));
	FCustomChannelSetup Trigger;
	Trigger.Channel = ECC_GameTraceChannel2;
	Trigger.DefaultResponse = ECR_Overlap;
	Trigger.Name = FName(TEXT("Trigger"));
	Profile->DefaultChannelResponses = {Weapon, Trigger};

	TestTrue("Engine channel name",
		Profile->ReturnChannelNameFromContainerIndex(ECC_Visibility) == FName(TEXT("Visibility")));
	TestTrue("Game channel name",
		Profile->ReturnChannelNameFromContainerIndex(ECC_GameTraceChannel1) == FName(TEXT("Weapon")));
	TestTrue("Unnamed channel", Profile->ReturnChannelNameFromContainerIndex(ECC_GameTraceChannel9) == NAME_None);
	TestEqual("Index of a name", Profile->ReturnContainerIndexFromChannelName(FName(TEXT("Trigger"))),
		static_cast<int32>(ECC_GameTraceChannel2));

	Profile->LoadProfileConfig();
	TestEqual("Default response from the config",
		FCollisionResponseContainer::GetDefaultResponseContainer().GetResponse(ECC_GameTraceChannel2), ECR_Overlap);
	FPhysScene Scene;
	const FBodyInstance Body = Scene.GetBodies()[AddChannelTestBox(Scene, FVector::ZeroVector)];
	TestEqual("A new body starts from it", Body.GetResponseToChannel(ECC_GameTraceChannel2), ECR_Overlap);

	// Back to the engine's config.
	UCollisionProfile::Get()->LoadProfileConfig();
	TestEqual("Restored", FCollisionResponseContainer::GetDefaultResponseContainer().GetResponse(ECC_GameTraceChannel2),
		ECR_Block);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
