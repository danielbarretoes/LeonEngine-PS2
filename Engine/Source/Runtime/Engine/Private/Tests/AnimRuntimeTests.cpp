#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineTestTypes.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AutomationTest.h"
#include "Tests/ScopedTestWorld.h"
#include "Tests/SkinnedTestMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

// The animation runtime (Docs/PLANS/ps2-shipping.md N25): 2D blend spaces, notifies, montages, the upper body's layer,
// the aim offset, the pose cache and its throttling, no heap allocation per frame, and the cost of ten characters.

namespace
{

	using FTestChar = FSkinnedTestCharacter;

	/** An anim instance of the test character's skeleton, the input snapping. */
	UAnimInstance& MakeInstance(USkeleton* Skeleton = nullptr)
	{
		UAnimInstance& Anim = *NewObject<UAnimInstance>();
		Anim.SetSkeleton(Skeleton != nullptr ? Skeleton : FTestChar::MakeSkeleton());
		Anim.SetLocomotionBlendInterpSpeed(0.0f);
		return Anim;
	}

	/** A blend space of one looping clip (the locomotion player), for the notify tests. */
	UBlendSpace1D* SingleClipSpace(UAnimSequence* Clip)
	{
		UBlendSpace1D* Space = NewObject<UBlendSpace1D>();
		(void)Space->AddSample(Clip, 0.0f);
		return Space;
	}

	/** A one-second looping clip of the character (rest pose). */
	UAnimSequence* MakeSecondClip()
	{
		return FTestChar::MakeOffsetClip(FTestChar::Root, FVector::ZeroVector, 1.0f);
	}

	/** The names OnAnimNotify broadcasts. */
	struct FNotifyLog
	{
		TArray<FName> Names;

		void Bind(UAnimInstance& Anim)
		{
			Anim.OnAnimNotify.AddLambda([this](FName Name, const UAnimSequenceBase*) { Names.Add(Name); });
		}
		[[nodiscard]] int32 Count(const TCHAR* Name) const
		{
			int32 Found = 0;
			for (const FName& Each : Names)
			{
				Found += Each == FName(Name) ? 1 : 0;
			}
			return Found;
		}
	};

	/** The montages OnMontageEnded broadcasts, and whether each was interrupted. */
	struct FMontageLog
	{
		TArray<UAnimMontage*> Ended;
		TArray<bool> Interrupted;

		void Bind(UAnimInstance& Anim)
		{
			Anim.OnMontageEnded.AddLambda(
				[this](UAnimMontage* Montage, bool bInterrupted)
				{
					Ended.Add(Montage);
					Interrupted.Add(bInterrupted);
				});
		}
	};

	/** A montage of a one-second one-shot clip that raises spine_01 by 10 cm, on Slot. */
	UAnimMontage* MakeMontage(FName Slot, float BlendIn, float BlendOut, int32 Bone = FTestChar::Spine01,
		const FVector& Offset = FVector(0.0f, 0.0f, 10.0f), int32 ExtraBones = 0)
	{
		UAnimSequence* Clip = FTestChar::MakeClip(
			2, [&](int32, TArray<FTransform>& Pose)
			{ Pose[Bone].SetTranslation(Pose[Bone].GetTranslation() + Offset); }, false, ExtraBones);
		Clip->SequenceLength = 1.0f;
		UAnimMontage* Montage = NewObject<UAnimMontage>();
		Montage->SetAnimation(Clip);
		Montage->SlotName = Slot;
		Montage->BlendInTime = BlendIn;
		Montage->BlendOutTime = BlendOut;
		return Montage;
	}

	/** A bone's local translation in the instance's current pose. */
	FVector LocalTranslation(const UAnimInstance& Anim, int32 Bone)
	{
		TArray<FTransform> Pose;
		Anim.EvaluatePose(Pose);
		return Pose.IsValidIndex(Bone) ? Pose[Bone].GetTranslation() : FVector(-1.0e6f);
	}

	/** A skeletal mesh component of the test character on a new actor at Location. */
	USkeletalMeshComponent* SpawnCharacter(UWorld& World, USkeletalMesh* Mesh, const FVector& Location)
	{
		AActor* Actor = World.SpawnActor<AActor>(Location, FRotator(0.0f, 0.0f, 0.0f));
		USkeletalMeshComponent* Skinned = NewObject<USkeletalMeshComponent>(Actor);
		(void)Skinned->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Skinned->RegisterComponent();
		Skinned->SetSkeletalMesh(Mesh);
		return Skinned;
	}

	/** Degrees between two rotations. */
	float AngleBetween(const FQuat& A, const FQuat& B)
	{
		return FMath::RadiansToDegrees(A.AngularDistance(B));
	}

	/**
	 * A full anim graph for a character: a 2D locomotion space (5 samples moving the thighs), an upper-body montage on
	 * a looping section (moving the spine and the hand) and a 5-pose aim offset on the spine.
	 */
	struct FFullGraph
	{
		UBlendSpace* Locomotion = nullptr;
		UAnimMontage* Montage = nullptr;
		UAimOffsetBlendSpace1D* AimOffset = nullptr;

		explicit FFullGraph(int32 ExtraBones = 0, int32 NumFrames = 31)
		{
			Locomotion = NewObject<UBlendSpace>();
			Locomotion->BlendParameters[0].Min = 0.0f;
			Locomotion->BlendParameters[0].Max = 600.0f;
			Locomotion->BlendParameters[1].Min = -180.0f;
			Locomotion->BlendParameters[1].Max = 180.0f;
			const FVector Positions[5] = {FVector(0.0f, 0.0f, 0.0f), FVector(600.0f, -180.0f, 0.0f),
				FVector(600.0f, -90.0f, 0.0f), FVector(600.0f, 90.0f, 0.0f), FVector(600.0f, 180.0f, 0.0f)};
			for (int32 Sample = 0; Sample < 5; ++Sample)
			{
				UAnimSequence* Clip = FTestChar::MakeClip(
					NumFrames,
					[&](int32 Frame, TArray<FTransform>& Pose)
					{
						const float Swing = FMath::Sin(float(Frame) * 0.2f) * 20.0f * float(Sample);
						Pose[FTestChar::ThighL].SetRotation(
							FQuat(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(Swing)));
						Pose[FTestChar::ThighR].SetRotation(
							FQuat(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(-Swing)));
						Pose[FTestChar::Pelvis].SetTranslation(Pose[FTestChar::Pelvis].GetTranslation() +
							FVector(0.0f, 0.0f, FMath::Cos(float(Frame) * 0.4f) * 2.0f));
					},
					true, ExtraBones);
				Clip->AddNotify(TEXT("Footstep_L"), 0.25f);
				Clip->AddNotify(TEXT("Footstep_R"), 0.75f);
				(void)Locomotion->AddSample(Clip, Positions[Sample]);
			}
			Montage = MakeMontage(
				UAnimInstance::UpperBodySlotName, 0.1f, 0.1f, FTestChar::HandR, FVector(10.0f, 0.0f, 0.0f), ExtraBones);
			(void)Montage->AddSection(TEXT("Loop"), 0.0f, TEXT("Loop"));
			Montage->AddNotify(TEXT("Fire"), 0.5f);
			AimOffset = NewObject<UAimOffsetBlendSpace1D>();
			for (int32 Pitch = -90; Pitch <= 90; Pitch += 45)
			{
				(void)AimOffset->AddSample(
					FTestChar::MakeRotatedClip(FTestChar::Spine01,
						FQuat(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(-0.5f * float(Pitch))), ExtraBones),
					float(Pitch));
			}
		}

		/** Sets the graph on Anim, the montage playing. */
		void Apply(UAnimInstance& Anim) const
		{
			Anim.SetBlendSpace(Locomotion);
			Anim.SetBlendSpaceInput(FVector(450.0f, 30.0f, 0.0f));
			Anim.SetUpperBodyBranchBone(TEXT("spine_01"));
			Anim.SetAimOffset(AimOffset);
			Anim.SetAimOffsetPitch(20.0f);
			(void)Anim.Montage_Play(Montage);
		}
	};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendSpace2DInterpolationTest, "System.Engine.Animation.BlendSpace2D.Interpolation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBlendSpace2DInterpolationTest::RunTest(const FString& Parameters)
{
	// Speed (0..600) by direction (-180..180): the four corners and the centre. Each sample's clip moves the root by
	// its coordinates / 100, so a blend's root is the input / 100 wherever the weights are barycentric.
	UBlendSpace& Space = *NewObject<UBlendSpace>();
	Space.BlendParameters[0].Min = 0.0f;
	Space.BlendParameters[0].Max = 600.0f;
	Space.BlendParameters[1].Min = -180.0f;
	Space.BlendParameters[1].Max = 180.0f;
	const FVector Positions[5] = {FVector(0.0f, -180.0f, 0.0f), FVector(600.0f, -180.0f, 0.0f),
		FVector(0.0f, 180.0f, 0.0f), FVector(600.0f, 180.0f, 0.0f), FVector(300.0f, 0.0f, 0.0f)};
	for (const FVector& Position : Positions)
	{
		(void)Space.AddSample(FTestChar::MakeOffsetClip(FTestChar::Root, Position / 100.0f), Position);
	}
	TestEqual("Four triangles around the centre", Space.GetTriangles().Num(), 4);

	FBlendSampleDataArray Samples;
	Space.GetSamplesFromBlendInput(FVector(600.0f, 180.0f, 0.0f), Samples);
	TestTrue("A corner: its sample", Samples.Num() == 1 && Samples[0].SampleDataIndex == 3);
	Space.GetSamplesFromBlendInput(FVector(300.0f, 0.0f, 0.0f), Samples);
	TestTrue("The centre: its sample", Samples.Num() == 1 && Samples[0].SampleDataIndex == 4);
	Space.GetSamplesFromBlendInput(FVector(300.0f, -180.0f, 0.0f), Samples);
	TestTrue("An edge: its two corners, half each",
		Samples.Num() == 2 && Samples[0].SampleDataIndex == 0 && Samples[1].SampleDataIndex == 1 &&
			FMath::IsNearlyEqual(Samples[0].TotalWeight, 0.5f, 1.0e-4f));
	Space.GetSamplesFromBlendInput(FVector(1000.0f, 0.0f, 0.0f), Samples);
	TestTrue("Past the axis: clamped to the right edge's middle",
		Samples.Num() == 2 && Samples[0].SampleDataIndex == 1 && Samples[1].SampleDataIndex == 3);
	Space.GetSamplesFromBlendInput(FVector(450.0f, 60.0f, 0.0f), Samples);
	TestEqual("Inside: three samples", Samples.Num(), 3);
	TestTrue("In the order of the samples",
		Samples.Num() == 3 && Samples[0].SampleDataIndex < Samples[1].SampleDataIndex &&
			Samples[1].SampleDataIndex < Samples[2].SampleDataIndex);

	// The player: the pose's root is the input / 100 (the barycentric blend of the samples' roots).
	UAnimInstance& Anim = MakeInstance();
	Anim.SetBlendSpace(&Space);
	for (const FVector& Input :
		{FVector(450.0f, 60.0f, 0.0f), FVector(100.0f, -150.0f, 0.0f), FVector(300.0f, 0.0f, 0.0f)})
	{
		Anim.SetBlendSpaceInput(Input);
		Anim.UpdateAnimation(1.0f / 30.0f);
		TestTrue(*FString::Printf(TEXT("The pose at %s"), *Input.ToString()),
			LocalTranslation(Anim, FTestChar::Root).Equals(Input / 100.0f, 0.05f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimNotifyOncePerCrossingTest, "System.Engine.Animation.Notifies.OncePerCrossing",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimNotifyOncePerCrossingTest::RunTest(const FString& Parameters)
{
	// A looping second with notifies at 0.25 and 0.75 s, played in steps of 0.1 s: each fires once a loop, in order.
	UAnimSequence* Clip = MakeSecondClip();
	Clip->AddNotify(TEXT("Footstep_L"), 0.25f);
	Clip->AddNotify(TEXT("Footstep_R"), 0.75f);
	UAnimInstance& Anim = MakeInstance();
	Anim.SetBlendSpace(SingleClipSpace(Clip));
	FNotifyLog Log;
	Log.Bind(Anim);
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Anim.UpdateAnimation(0.1f);
	}
	TestEqual("One loop: two", Log.Names.Num(), 2);
	for (int32 Step = 0; Step < 25; ++Step)
	{
		Anim.UpdateAnimation(0.1f);
	}
	TestEqual("3.5 loops: seven", Log.Names.Num(), 7);
	bool bAlternating = true;
	for (int32 Index = 0; Index < Log.Names.Num(); ++Index)
	{
		bAlternating &= Log.Names[Index] == FName(Index % 2 == 0 ? TEXT("Footstep_L") : TEXT("Footstep_R"));
	}
	TestTrue("In order: left, right, left...", bAlternating);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimNotifyWrapTest, "System.Engine.Animation.Notifies.LoopWrap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimNotifyWrapTest::RunTest(const FString& Parameters)
{
	// Notifies at the very start (0) and near the end (0.95 s) of a looping second: the start fires as the player
	// starts and each time it comes round, the end each loop, never twice; a step over the wrap fires both. A step
	// longer than the clip fires each notify once per loop it covers.
	UAnimSequence* Clip = MakeSecondClip();
	Clip->AddNotify(TEXT("Start"), 0.0f);
	Clip->AddNotify(TEXT("End"), 0.95f);
	UAnimInstance& Anim = MakeInstance();
	Anim.SetBlendSpace(SingleClipSpace(Clip));
	FNotifyLog Log;
	Log.Bind(Anim);
	Anim.UpdateAnimation(0.9f);
	TestTrue("The start, once", Log.Names.Num() == 1 && Log.Count(TEXT("Start")) == 1);
	Anim.UpdateAnimation(0.2f);
	TestTrue("Over the wrap: the end, then the start again",
		Log.Names.Num() == 3 && Log.Names[1] == FName(TEXT("End")) && Log.Names[2] == FName(TEXT("Start")));
	Anim.UpdateAnimation(0.05f);
	TestEqual("Nothing crossed", Log.Names.Num(), 3);
	Anim.UpdateAnimation(2.0f);
	TestTrue("Two loops in one step: each twice", Log.Count(TEXT("Start")) == 4 && Log.Count(TEXT("End")) == 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimNotifyMultipleTest, "System.Engine.Animation.Notifies.SeveralInOneUpdate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimNotifyMultipleTest::RunTest(const FString& Parameters)
{
	// One update over three notifies fires the three, in time order (not the order they were added), each once; a
	// notify object's Notify runs as well, and its broadcast name is its class's.
	UAnimSequence* Clip = MakeSecondClip();
	Clip->AddNotify(TEXT("Third"), 0.3f);
	Clip->AddNotify(TEXT("First"), 0.1f);
	UEngineTestAnimNotify* Object = NewObject<UEngineTestAnimNotify>();
	Clip->AddNotify(NAME_None, 0.2f, Object);
	UAnimInstance& Anim = MakeInstance();
	Anim.SetBlendSpace(SingleClipSpace(Clip));
	FNotifyLog Log;
	Log.Bind(Anim);
	Anim.UpdateAnimation(0.5f);
	TestEqual("Three fired", Anim.GetNumNotifiesFiredLastUpdate(), 3);
	TestTrue("In time order",
		Log.Names.Num() == 3 && Log.Names[0] == FName(TEXT("First")) &&
			Log.Names[1] == UEngineTestAnimNotify::StaticClass()->GetFName() && Log.Names[2] == FName(TEXT("Third")));
	TestTrue("The object's Notify, once", Object->NumNotifies == 1 && Object->LastAnimation == Clip);
	Anim.UpdateAnimation(0.2f);
	TestEqual("None again before the loop", Log.Names.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimNotifyZeroLengthTest, "System.Engine.Animation.Notifies.ZeroLength",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimNotifyZeroLengthTest::RunTest(const FString& Parameters)
{
	// An update of no time crosses nothing, however often (a paused game), and a notify at 0 fires once, on the first
	// update that moves; a clip of no length fires nothing and does not stall.
	UAnimSequence* Clip = MakeSecondClip();
	Clip->AddNotify(TEXT("Start"), 0.0f);
	UAnimInstance& Anim = MakeInstance();
	Anim.SetBlendSpace(SingleClipSpace(Clip));
	FNotifyLog Log;
	Log.Bind(Anim);
	for (int32 Step = 0; Step < 5; ++Step)
	{
		Anim.UpdateAnimation(0.0f);
	}
	TestEqual("No time: nothing", Log.Names.Num(), 0);
	Anim.UpdateAnimation(0.1f);
	Anim.UpdateAnimation(0.0f);
	Anim.UpdateAnimation(0.0f);
	TestEqual("The start once", Log.Names.Num(), 1);

	UAnimSequence* Empty = MakeSecondClip();
	Empty->SequenceLength = 0.0f;
	Empty->AddNotify(TEXT("Never"), 0.0f);
	UAnimInstance& Still = MakeInstance();
	Still.SetBlendSpace(SingleClipSpace(Empty));
	FNotifyLog StillLog;
	StillLog.Bind(Still);
	Still.UpdateAnimation(0.5f);
	Still.UpdateAnimation(0.5f);
	TestEqual("A clip of no length: nothing", StillLog.Names.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimMontageBlendTimingTest, "System.Engine.Animation.Montage.BlendTiming",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimMontageBlendTimingTest::RunTest(const FString& Parameters)
{
	// A one-second montage, blend in 0.2 s and out 0.3 s, in steps of 0.05 s: its weight rises linearly to 1 by 0.2 s,
	// stays there until 0.7 s, falls to 0.5 at 0.85 s and ends at 1 s (OnMontageEnded once, not interrupted). Its
	// clip's notify and its own fire once each.
	UAnimMontage* Montage = MakeMontage(UAnimInstance::DefaultSlotName, 0.2f, 0.3f);
	Montage->Animation->AddNotify(TEXT("MagOut"), 0.4f);
	Montage->AddNotify(TEXT("MagIn"), 0.8f);
	UAnimInstance& Anim = MakeInstance();
	FMontageLog Ended;
	Ended.Bind(Anim);
	FNotifyLog Log;
	Log.Bind(Anim);
	TestEqual("Its length", Anim.Montage_Play(Montage), 1.0f, 1.0e-5f);
	TestTrue("Playing", Anim.Montage_IsPlaying(Montage) && Anim.GetCurrentActiveMontage() == Montage);

	const float Step = 0.05f;
	float Weights[21] = {};
	for (int32 Index = 1; Index <= 20; ++Index)
	{
		Anim.UpdateAnimation(Step);
		Weights[Index] = Anim.GetSlotMontageGlobalWeight(UAnimInstance::DefaultSlotName);
		if (Index == 19)
		{
			TestEqual("Not ended before its end", Ended.Ended.Num(), 0);
		}
	}
	TestEqual("0.1 s: half blended in", Weights[2], 0.5f, 1.0e-4f);
	TestEqual("0.2 s: in", Weights[4], 1.0f, 1.0e-5f);
	TestEqual("0.7 s: still in", Weights[14], 1.0f, 1.0e-5f);
	TestEqual("0.85 s: half blended out", Weights[17], 0.5f, 1.0e-3f);
	TestTrue("Ended once at 1 s, not interrupted",
		Ended.Ended.Num() == 1 && Ended.Ended[0] == Montage && !Ended.Interrupted[0]);
	TestFalse("No longer active", Anim.Montage_IsActive(Montage));
	TestTrue("Its notifies once each", Log.Count(TEXT("MagOut")) == 1 && Log.Count(TEXT("MagIn")) == 1);

	// Twice the rate: the same montage ends after half a second.
	(void)Anim.Montage_Play(Montage, 2.0f);
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Anim.UpdateAnimation(Step);
	}
	TestEqual("Rate 2: ended at 0.5 s", Ended.Ended.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimMontageInterruptTest, "System.Engine.Animation.Montage.InterruptAndStop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimMontageInterruptTest::RunTest(const FString& Parameters)
{
	// A montage played on a slot that plays another blends the old one out over the new one's blend in; the old one
	// ends interrupted and fires no more notifies. Montage_Stop ends a montage interrupted after its blend out.
	UAnimMontage* Reload = MakeMontage(UAnimInstance::UpperBodySlotName, 0.1f, 0.1f);
	Reload->AddNotify(TEXT("MagIn"), 0.8f);
	UAnimMontage* Draw = MakeMontage(UAnimInstance::UpperBodySlotName, 0.2f, 0.1f);
	UAnimInstance& Anim = MakeInstance();
	FMontageLog Ended;
	Ended.Bind(Anim);
	FNotifyLog Log;
	Log.Bind(Anim);
	(void)Anim.Montage_Play(Reload);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Anim.UpdateAnimation(0.05f);
	}
	(void)Anim.Montage_Play(Draw);
	TestTrue(
		"The new one is the active one", Anim.GetCurrentActiveMontage() == Draw && !Anim.Montage_IsPlaying(Reload));
	Anim.UpdateAnimation(0.1f);
	TestEqual("Halfway through the new one's blend in: still there", Ended.Ended.Num(), 0);
	Anim.UpdateAnimation(0.1f);
	TestTrue("Its blend in done: the old one ended interrupted",
		Ended.Ended.Num() == 1 && Ended.Ended[0] == Reload && Ended.Interrupted[0]);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Anim.UpdateAnimation(0.1f);
	}
	TestEqual("The interrupted montage's notify never fired", Log.Count(TEXT("MagIn")), 0);

	Anim.Montage_Stop(0.2f, Draw);
	Anim.UpdateAnimation(0.1f);
	TestEqual("Stopping: blending out", Ended.Ended.Num(), 1);
	Anim.UpdateAnimation(0.1f);
	TestTrue("Stopped: ended interrupted", Ended.Ended.Num() == 2 && Ended.Ended[1] == Draw && Ended.Interrupted[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimMontageSectionsTest, "System.Engine.Animation.Montage.Sections",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimMontageSectionsTest::RunTest(const FString& Parameters)
{
	// Start (0 s), Loop (0.5 s, linked to itself), End (0.8 s): the montage stays in Loop until Montage_SetNextSection
	// sends it on to End; Montage_JumpToSection moves it at once.
	UAnimMontage* Plant = MakeMontage(UAnimInstance::DefaultSlotName, 0.1f, 0.1f);
	(void)Plant->AddSection(TEXT("Start"), 0.0f);
	(void)Plant->AddSection(TEXT("End"), 0.8f);
	(void)Plant->AddSection(TEXT("Loop"), 0.5f, TEXT("Loop"));
	TestTrue("Sorted by time", Plant->CompositeSections[1].SectionName == FName(TEXT("Loop")));
	UAnimInstance& Anim = MakeInstance();
	FMontageLog Ended;
	Ended.Bind(Anim);
	(void)Anim.Montage_Play(Plant);
	for (int32 Index = 0; Index < 60; ++Index)
	{
		Anim.UpdateAnimation(0.05f);
	}
	const float Position = Anim.Montage_GetPosition(Plant);
	TestTrue("Three seconds later: still looping the Loop section",
		Ended.Ended.Num() == 0 && Position >= 0.5f && Position < 0.8f);
	Anim.Montage_SetNextSection(TEXT("Loop"), TEXT("End"));
	for (int32 Index = 0; Index < 14; ++Index)
	{
		Anim.UpdateAnimation(0.05f);
	}
	TestTrue("Sent on to End: ended", Ended.Ended.Num() == 1 && !Ended.Interrupted[0]);

	(void)Anim.Montage_Play(Plant);
	Anim.Montage_JumpToSection(TEXT("End"));
	TestEqual("Jumped", Anim.Montage_GetPosition(Plant), 0.8f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimLayeredBlendTest, "System.Engine.Animation.Layers.UpperBodyKeepsTheLegs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimLayeredBlendTest::RunTest(const FString& Parameters)
{
	// The locomotion moves the left thigh 5 cm forward. An UpperBody montage that moves the thigh down 50 cm and the
	// spine up 10 cm changes only the spine (from the branch bone up): the legs keep walking. A DefaultSlot montage
	// takes the whole body.
	UAnimInstance& Anim = MakeInstance();
	Anim.SetBlendSpace(SingleClipSpace(FTestChar::MakeOffsetClip(FTestChar::ThighL, FVector(5.0f, 0.0f, 0.0f))));
	Anim.SetUpperBodyBranchBone(TEXT("spine_01"));
	const FReferenceSkeleton Bones = FTestChar::MakeBones();
	const FVector ThighRest = Bones.RefBonePose[FTestChar::ThighL].GetTranslation();
	const FVector SpineRest = Bones.RefBonePose[FTestChar::Spine01].GetTranslation();

	UAnimSequence* Both = FTestChar::MakeClip(
		2,
		[](int32, TArray<FTransform>& Pose)
		{
			Pose[FTestChar::ThighL].SetTranslation(
				Pose[FTestChar::ThighL].GetTranslation() + FVector(0.0f, 0.0f, -50.0f));
			Pose[FTestChar::Spine01].SetTranslation(
				Pose[FTestChar::Spine01].GetTranslation() + FVector(0.0f, 0.0f, 10.0f));
		},
		false);
	Both->SequenceLength = 1.0f;
	UAnimMontage* Upper = NewObject<UAnimMontage>();
	Upper->SetAnimation(Both);
	Upper->SlotName = UAnimInstance::UpperBodySlotName;
	Upper->BlendInTime = 0.0f;
	(void)Anim.Montage_Play(Upper);
	Anim.UpdateAnimation(0.1f);
	TestTrue("The thigh: the locomotion's",
		LocalTranslation(Anim, FTestChar::ThighL).Equals(ThighRest + FVector(5.0f, 0.0f, 0.0f), 0.05f));
	TestTrue("The spine: the montage's",
		LocalTranslation(Anim, FTestChar::Spine01).Equals(SpineRest + FVector(0.0f, 0.0f, 10.0f), 0.05f));

	Anim.Montage_Stop(0.0f);
	Anim.UpdateAnimation(0.1f);
	UAnimMontage* Full = NewObject<UAnimMontage>();
	Full->SetAnimation(Both);
	Full->BlendInTime = 0.0f;
	(void)Anim.Montage_Play(Full);
	Anim.UpdateAnimation(0.1f);
	TestTrue("A DefaultSlot montage moves the legs too",
		LocalTranslation(Anim, FTestChar::ThighL).Equals(ThighRest + FVector(0.0f, 0.0f, -50.0f), 0.05f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimAimOffsetTest, "System.Engine.Animation.AimOffset.AdditiveAtExtremes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimAimOffsetTest::RunTest(const FString& Parameters)
{
	// Five aim poses pitch spine_01 by half the aim (-90..90 -> 45..-45 degrees about Y). Over a locomotion that turns
	// the spine 30 degrees about Z, the aim offset adds its pitch: at +-90 the extreme pose's delta, at 0 nothing, at
	// 45 the 45 sample's; the thighs never move.
	const FQuat Yaw(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(30.0f));
	UAnimInstance& Anim = MakeInstance();
	Anim.SetBlendSpace(SingleClipSpace(FTestChar::MakeRotatedClip(FTestChar::Spine01, Yaw)));
	Anim.SetUpperBodyBranchBone(TEXT("spine_01"));
	UAimOffsetBlendSpace1D* Aim = NewObject<UAimOffsetBlendSpace1D>();
	for (int32 Pitch = -90; Pitch <= 90; Pitch += 45)
	{
		(void)Aim->AddSample(FTestChar::MakeRotatedClip(FTestChar::Spine01,
								 FQuat(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(-0.5f * float(Pitch)))),
			float(Pitch));
	}
	TestTrue("The base: the sample at 0", Aim->GetAdditiveBasePose() == Aim->GetBlendSamples()[2].Animation);
	Anim.SetAimOffset(Aim);
	auto SpineAt = [&](float Pitch)
	{
		Anim.SetAimOffsetPitch(Pitch);
		Anim.UpdateAnimation(1.0f / 30.0f);
		TArray<FTransform> Pose;
		Anim.EvaluatePose(Pose);
		return Pose;
	};
	auto Expected = [&](float Pitch)
	{ return FQuat(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(-0.5f * Pitch)) * Yaw; };
	for (const float Pitch : {90.0f, -90.0f, 45.0f, 0.0f})
	{
		const TArray<FTransform> Pose = SpineAt(Pitch);
		TestTrue(*FString::Printf(TEXT("Pitch %.0f: the spine"), Pitch),
			AngleBetween(Pose[FTestChar::Spine01].GetRotation(), Expected(Pitch)) < 0.1f);
		TestTrue(*FString::Printf(TEXT("Pitch %.0f: the thigh untouched"), Pitch),
			AngleBetween(Pose[FTestChar::ThighL].GetRotation(), FQuat::Identity) < 1.0e-3f);
	}
	// Past the axis: held at the extreme.
	TestTrue("Pitch 120: clamped to 90",
		AngleBetween(SpineAt(120.0f)[FTestChar::Spine01].GetRotation(), Expected(90.0f)) < 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkeletalMeshPoseCacheTest, "System.Engine.Components.SkeletalMeshPoseCacheReuse",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSkeletalMeshPoseCacheTest::RunTest(const FString& Parameters)
{
	// One evaluation per update: the sockets, the bones and the bounds read the cached matrices, however often.
	FScopedTestWorld TestWorld;
	USkeletalMeshComponent* Skinned = SpawnCharacter(*TestWorld, FTestChar::MakeMesh(), FVector::ZeroVector);
	if (!TestTrue("The character", Skinned->HasValidMesh()))
	{
		return false;
	}
	FFullGraph Graph;
	Graph.Apply(Skinned->GetAnimInstance());
	// Past the montage's 0.1 s blend in.
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		Skinned->TickComponent(1.0f / 30.0f);
	}
	const int32 Evaluations = Skinned->GetNumPoseEvaluations();
	FVector Grip = FVector::ZeroVector;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		Grip = Skinned->GetSocketTransform(TEXT("Weapon_R")).GetLocation();
		(void)Skinned->GetSocketTransform(TEXT("hand_r"));
		(void)Skinned->GetPoseBounds();
	}
	TestEqual("The sockets did not evaluate again", Skinned->GetNumPoseEvaluations(), Evaluations);
	// The hand 10 cm forward from the montage, the socket 10 more, the whole tilted 10 degrees by the aim offset: about
	// 44 cm ahead (34 without the montage).
	TestTrue("The grip follows the montage's hand", Grip.X > 40.0f && Grip.X < 50.0f);
	Skinned->TickComponent(1.0f / 30.0f);
	TestEqual("The next update: one more", Skinned->GetNumPoseEvaluations(), Evaluations + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkeletalMeshThrottlingTest, "System.Engine.Components.SkeletalMeshUpdateRate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSkeletalMeshThrottlingTest::RunTest(const FString& Parameters)
{
	// The update rate optimization: near the view every frame, 100 m away one frame in four (the settings' 15 m steps,
	// at most 4); a mesh AlwaysTickPose skips while not drawn. The anim instance still updates: its montage ends on
	// time and its notifies fire whether the pose is evaluated or not.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	USkeletalMesh* Mesh = FTestChar::MakeMesh();
	USkeletalMeshComponent* Near = SpawnCharacter(World, Mesh, FVector(100.0f, 0.0f, 0.0f));
	USkeletalMeshComponent* Far = SpawnCharacter(World, Mesh, FVector(10000.0f, 0.0f, 0.0f));
	USkeletalMeshComponent* Hidden = SpawnCharacter(World, Mesh, FVector(100.0f, 0.0f, 0.0f));
	for (USkeletalMeshComponent* Skinned : {Near, Far, Hidden})
	{
		Skinned->bEnableUpdateRateOptimizations = true;
		Skinned->LastRenderTime = World.GetTimeSeconds();
		Skinned->TickComponent(1.0f / 30.0f);
	}
	Hidden->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	Hidden->LastRenderTime = -1000.0f;
	UAnimMontage* Montage = MakeMontage(UAnimInstance::DefaultSlotName, 0.0f, 0.0f);
	Montage->AddNotify(TEXT("Beep"), 0.2f);
	FMontageLog Ended;
	Ended.Bind(Hidden->GetAnimInstance());
	FNotifyLog Log;
	Log.Bind(Hidden->GetAnimInstance());
	(void)Hidden->GetAnimInstance().Montage_Play(Montage);

	World.ViewLocationsRenderedLastFrame = {FVector::ZeroVector};
	const int32 NearBefore = Near->GetNumPoseEvaluations();
	const int32 FarBefore = Far->GetNumPoseEvaluations();
	const int32 HiddenBefore = Hidden->GetNumPoseEvaluations();
	for (int32 Frame = 0; Frame < 36; ++Frame)
	{
		Near->LastRenderTime = World.GetTimeSeconds();
		Far->LastRenderTime = World.GetTimeSeconds();
		for (USkeletalMeshComponent* Skinned : {Near, Far, Hidden})
		{
			Skinned->TickComponent(1.0f / 30.0f);
		}
	}
	TestEqual("Near: every frame", Near->GetNumPoseEvaluations() - NearBefore, 36);
	TestEqual("Near: rate 1", Near->GetUpdateRate(), 1);
	TestEqual("100 m away: one frame in four", Far->GetNumPoseEvaluations() - FarBefore, 9);
	TestEqual("100 m away: rate 4", Far->GetUpdateRate(), 4);
	TestEqual("Not drawn: no evaluation", Hidden->GetNumPoseEvaluations() - HiddenBefore, 0);
	TestTrue(
		"Not drawn: the montage still ended, its notify fired", Ended.Ended.Num() == 1 && Log.Count(TEXT("Beep")) == 1);
	// A socket of the skipped mesh reads its last pose, without evaluating.
	(void)Hidden->GetSocketTransform(TEXT("Weapon_R"));
	TestEqual("Its socket: the cached pose", Hidden->GetNumPoseEvaluations() - HiddenBefore, 0);
	Hidden->LastRenderTime = World.GetTimeSeconds();
	Hidden->TickComponent(1.0f / 30.0f);
	TestEqual("Drawn again: evaluated", Hidden->GetNumPoseEvaluations() - HiddenBefore, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimNoHeapAllocationTest, "System.Engine.Animation.NoHeapAllocationPerFrame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAnimNoHeapAllocationTest::RunTest(const FString& Parameters)
{
	// Ten characters with the whole graph (a 2D locomotion space, an upper-body montage looping, the aim offset, the
	// notifies broadcast): once warm, their updates, evaluations, sockets and skin matrices take nothing from GMalloc
	// (the temporaries are on the frame's stack, the caches keep their memory).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	USkeletalMesh* Mesh = FTestChar::MakeMesh();
	FFullGraph Graph;
	TArray<USkeletalMeshComponent*> Characters;
	int32 NotifiesFired = 0;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		USkeletalMeshComponent* Skinned = SpawnCharacter(World, Mesh, FVector(200.0f * float(Index), 0.0f, 0.0f));
		Graph.Apply(Skinned->GetAnimInstance());
		Skinned->GetAnimInstance().OnAnimNotify.AddLambda(
			[&NotifiesFired](FName, const UAnimSequenceBase*) { ++NotifiesFired; });
		Characters.Add(Skinned);
	}
	const auto Frame = [&](int32 FrameIndex)
	{
		for (USkeletalMeshComponent* Skinned : Characters)
		{
			Skinned->GetAnimInstance().SetBlendSpaceInput(
				FVector(300.0f + (200.0f * FMath::Sin(float(FrameIndex) * 0.1f)), 60.0f, 0.0f));
			Skinned->GetAnimInstance().SetAimOffsetPitch(60.0f * FMath::Cos(float(FrameIndex) * 0.05f));
			Skinned->TickComponent(1.0f / 30.0f);
			(void)Skinned->GetSocketTransform(TEXT("Weapon_R"));
		}
		World.SendAllEndOfFrameUpdates();
	};
	for (int32 Warm = 0; Warm < 30; ++Warm)
	{
		Frame(Warm);
	}
	const uint64 Before = FMemory::GetUsage().TotalAllocations;
	const int32 NotifiesBefore = NotifiesFired;
	for (int32 Index = 30; Index < 120; ++Index)
	{
		Frame(Index);
	}
	TestEqual("No heap allocation in 90 frames", FMemory::GetUsage().TotalAllocations, Before);
	TestTrue("The notifies fired meanwhile", NotifiesFired > NotifiesBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimTenCharactersPerfTest, "System.Engine.Animation.Perf.TenCharacters",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAnimTenCharactersPerfTest::RunTest(const FString& Parameters)
{
	// The cost of ten characters' animation (plan N25's measure): each frame, every character's update (a 2D
	// locomotion blend of three 1-second clips, an upper-body montage, a 5-pose aim offset, notifies), its pose's
	// evaluation into component-space matrices and its skin matrices. Logged as "AnimPerf:" (Win64; the EE estimate is
	// in Docs/ARCHITECTURE.md, animation runtime). The CPU skinning of the vertices is the renderer's, not counted.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	// A character's size: the test character's 7 bones and a chain of 25 more under the hand (32).
	constexpr int32 ExtraBones = 25;
	USkeletalMesh* Mesh = FTestChar::MakeMesh(nullptr, ExtraBones);
	if (!TestNotNull("The character", Mesh))
	{
		return false;
	}
	FFullGraph Graph(ExtraBones);
	TArray<USkeletalMeshComponent*> Characters;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		USkeletalMeshComponent* Skinned = SpawnCharacter(World, Mesh, FVector(200.0f * float(Index), 0.0f, 0.0f));
		Graph.Apply(Skinned->GetAnimInstance());
		Characters.Add(Skinned);
	}
	constexpr int32 Frames = 600;
	TArray<FMatrix> Skin;
	const auto Frame = [&](int32 FrameIndex)
	{
		for (USkeletalMeshComponent* Skinned : Characters)
		{
			Skinned->GetAnimInstance().SetBlendSpaceInput(
				FVector(300.0f + (200.0f * FMath::Sin(float(FrameIndex) * 0.1f)), 60.0f, 0.0f));
			Skinned->TickComponent(1.0f / 30.0f);
			FAnimationRuntime::GetSkinMatrices(Mesh->GetRefSkeleton(), Skinned->GetComponentSpaceTransforms(), Skin);
		}
	};
	for (int32 Warm = 0; Warm < 30; ++Warm)
	{
		Frame(Warm);
	}
	const uint64 Start = FPlatformTime::Cycles64();
	for (int32 Index = 0; Index < Frames; ++Index)
	{
		Frame(Index);
	}
	const double Seconds = double(FPlatformTime::Cycles64() - Start) * FPlatformTime::GetSecondsPerCycle64();
	const double MicrosecondsPerFrame = (Seconds * 1.0e6) / double(Frames);
	UE_LOG(LogTemp, Display, "AnimPerf: 10 characters x %d bones, %d frames: %.1f us a frame (%.2f us a character)",
		Mesh->GetRefSkeleton().GetNum(), Frames, MicrosecondsPerFrame, MicrosecondsPerFrame / 10.0);
	TestTrue("Timed", Seconds > 0.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
