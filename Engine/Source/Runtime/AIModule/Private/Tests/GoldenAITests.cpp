#include "AI/Navigation/NavigationSystem.h"
#include "AIController.h"
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "Tests/LegacyGolden.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenAIControllerArrivesTest, "System.AIModule.Golden.AIControllerArrives",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenAIControllerArrivesTest::RunTest(const FString& Parameters)
{
	// An AI-driven character follows a NavMesh path around a pillar to its goal: the character every 12 frames and
	// the frame the controller reports arrival. Walk speed and gravity keep their metre-tuned engine defaults.
	constexpr float DeltaTime = 1.0f / 60.0f;
	constexpr int32 Frames = 240;
	constexpr int32 SampleEvery = 12;

	UWorld World;
	FPhysScene& Physics = World.GetPhysicsScene();
	const int32 PillarId = Physics.AddBody({0, EBodyType::Static, 1.0f, true});
	Physics.GetBodies()[PillarId].Position = LegacyGolden::ToWorldPosition(FVector(0.0f, 1.0f, 0.2f));
	Physics.GetBodies()[PillarId].HalfExtents = LegacyGolden::ToWorldExtent(FVector(0.6f, 1.0f, 0.9f));

	UNavigationSystem Nav;
	Nav.SetCellSize(LegacyGolden::ToWorldLength(0.5f));
	Nav.SetAgentRadius(LegacyGolden::ToWorldLength(0.45f));
	Nav.BuildFromPhysScene(Physics, LegacyGolden::ToWorldLength(0.0f), LegacyGolden::ToWorldLength(8.0f));
	if (!TestTrue("Nav mesh built", Nav.HasNavMesh()))
	{
		return false;
	}

	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->GetCharacterMovement().FloorZ = LegacyGolden::ToWorldLength(0.0f);
	Character->Reset(
		LegacyGolden::ToWorldPosition(FVector(-4.0f, 0.0f, 0.3f)), LegacyGolden::ToWorldActorRotation(0.0f));

	AAIController Ai;
	Ai.Possess(Character);
	Ai.SetNavigationSystem(&Nav);
	Ai.SetArriveRadius(LegacyGolden::ToWorldLength(0.4f));
	Ai.MoveToLocation(LegacyGolden::ToWorldPosition(FVector(4.0f, 0.0f, -0.2f)));
	const int32 PathPointCount = Ai.PathPoints().Num();

	TArray<FVector> Samples;
	int32 ArrivalFrame = 0;
	for (int32 Frame = 1; Frame <= Frames; ++Frame)
	{
		const FVector Wish = Ai.TickAI(DeltaTime);
		if (ArrivalFrame == 0 && Wish.IsZero())
		{
			ArrivalFrame = Frame;
		}
		Character->PerformMovement(Physics, DeltaTime, nullptr);
		if (Frame % SampleEvery == 0)
		{
			Samples.Add(Character->GetActorLocation());
		}
	}

	static const int32 ExpectedPathPointsAndArrival[2] = {17, 118};
	static const FVector ExpectedSamples[Frames / SampleEvery] = {FVector(-3.29715729f, 0.0f, -0.260497272f),
		FVector(-2.64043784f, 0.0f, -0.875725925f), FVector(-1.98094821f, 0.0f, -1.47926927f),
		FVector(-1.11013615f, 0.0f, -1.68607068f), FVector(-0.211626902f, 0.0f, -1.73451304f),
		FVector(0.688274145f, 0.0f, -1.74616838f), FVector(1.55937076f, 0.0f, -1.63849616f),
		FVector(2.29301953f, 0.0f, -1.1227839f), FVector(3.08262086f, 0.0f, -0.715165675f),
		FVector(3.65931892f, 0.0f, -0.368032455f), FVector(3.65931892f, 0.0f, -0.368032455f),
		FVector(3.65931892f, 0.0f, -0.368032455f), FVector(3.65931892f, 0.0f, -0.368032455f),
		FVector(3.65931892f, 0.0f, -0.368032455f), FVector(3.65931892f, 0.0f, -0.368032455f),
		FVector(3.65931892f, 0.0f, -0.368032455f), FVector(3.65931892f, 0.0f, -0.368032455f),
		FVector(3.65931892f, 0.0f, -0.368032455f), FVector(3.65931892f, 0.0f, -0.368032455f),
		FVector(3.65931892f, 0.0f, -0.368032455f)};
	LegacyGolden::CheckInts(
		*this, "PathPointsAndArrival", TArray<int32>{PathPointCount, ArrivalFrame}, ExpectedPathPointsAndArrival, 2);
	LegacyGolden::CheckPositions(*this, "Samples", Samples, ExpectedSamples, Frames / SampleEvery, 1.0e-3f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
