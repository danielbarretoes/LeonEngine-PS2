#include "Animation/BlendSpace1D.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/ScopedTestWorld.h"
#include "Tests/SkinnedTestMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

// The skeletal mesh component's pose (Docs/PLANS/ps2-shipping.md N21): evaluated once per update in local space, its
// component-space matrices cached for the sockets, the skin matrices and the pose's bounds.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkeletalMeshComponentPoseTest, "System.Engine.Components.SkeletalMeshPoseAndSockets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSkeletalMeshComponentPoseTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AActor* Actor = World.SpawnActor<AActor>(FVector(1000.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f));
	USkeletalMesh* Mesh = MakeSkinnedTestMesh();
	if (!TestNotNull("The test mesh", Mesh))
	{
		return false;
	}
	USkeletalMeshComponent* Skinned = NewObject<USkeletalMeshComponent>(Actor);
	(void)Skinned->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	Skinned->RegisterComponent();
	Skinned->SetSkeletalMesh(Mesh);

	// Before any update: the skeleton's reference pose.
	TestEqual("Bone by name", Skinned->GetBoneIndex(TEXT("top")), 1);
	TestTrue("Sockets and bones",
		Skinned->DoesSocketExist(TEXT("Tip")) && Skinned->DoesSocketExist(TEXT("top")) &&
			!Skinned->DoesSocketExist(TEXT("hand_r")));
	TestTrue("The socket at rest",
		Skinned->GetSocketTransform(TEXT("Tip")).GetLocation().Equals(FVector(1000.0f, 0.0f, 60.0f), 1.0e-3f));

	// A clip moves the root 30 cm along Y: after the update, the bones and the socket follow it.
	UBlendSpace1D* Locomotion = NewObject<UBlendSpace1D>();
	Locomotion->AddSample(MakeSkinnedTestClip(FVector(0.0f, 30.0f, 0.0f)), 0.0f);
	UAnimInstance& Anim = Skinned->GetAnimInstance();
	Anim.SetBlendSpace(Locomotion);
	Anim.SetLocomotionBlendInterpSpeed(0.0f);
	Skinned->TickComponent(1.0f / 30.0f);
	TestTrue("The socket follows the pose",
		Skinned->GetSocketTransform(TEXT("Tip")).GetLocation().Equals(FVector(1000.0f, 30.0f, 60.0f), 1.0e-2f));
	TestTrue("A bone as a socket",
		Skinned->GetSocketTransform(TEXT("top")).GetLocation().Equals(FVector(1000.0f, 30.0f, 50.0f), 1.0e-2f));

	// The pose is cached: the sockets read the same matrices until the next update.
	const TArray<FMatrix>& Pose = Skinned->GetComponentSpaceTransforms();
	const FMatrix* Before = Pose.GetData();
	(void)Skinned->GetSocketTransform(TEXT("Tip"));
	(void)Skinned->GetSocketTransform(TEXT("top"));
	TestTrue("Not evaluated again", Skinned->GetComponentSpaceTransforms().GetData() == Before && Pose.Num() == 2);

	// The pose's bounds hold the moved cube (Y from -20 to 80 cm, Z from -50 to 50).
	const FBox Bounds = Skinned->GetPoseBounds();
	TestTrue("Bounds around the pose",
		Bounds.IsValid && Bounds.Min.Y <= -20.0f && Bounds.Max.Y >= 80.0f && Bounds.Min.Z <= -50.0f &&
			Bounds.Max.Z >= 50.0f);
	TestTrue("Not around the rest", Bounds.GetCenter().Y > 10.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
