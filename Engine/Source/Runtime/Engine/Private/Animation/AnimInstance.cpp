#include "Animation/AnimInstance.h"

#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/MemStack.h"

namespace
{

	/** A pose on the frame's stack; declare its FMemMark first. */
	typedef TArray<FTransform, TMemStackAllocator<>> FStackPose;

	/** The most wraps or section jumps one update follows (a longer frame drops the rest). */
	constexpr int32 MaxLoopsPerUpdate = 16;

	/** Montages that started to blend out, broadcast once the instances are no longer being walked. */
	typedef TArray<TPair<UAnimMontage*, bool>, TInlineAllocator<4>> FBlendingOutList;

} // namespace

const FName UAnimInstance::DefaultSlotName(TEXT("DefaultSlot"));
const FName UAnimInstance::UpperBodySlotName(TEXT("UpperBody"));

UAnimInstance::UAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

AActor* UAnimInstance::GetOwningActor() const
{
	return OwningMesh != nullptr ? OwningMesh->GetOwner() : nullptr;
}

int32 UAnimInstance::GetNumBones() const
{
	return Skeleton != nullptr ? Skeleton->GetReferenceSkeleton().GetNum() : 0;
}

void UAnimInstance::SetSkeleton(const USkeleton* InSkeleton)
{
	Skeleton = InSkeleton;
	RebuildBoneWeights();
}

void UAnimInstance::SetUpperBodyBranchBone(FName BoneName)
{
	UpperBodyBranchBone = BoneName;
	RebuildBoneWeights();
}

void UAnimInstance::RebuildBoneWeights()
{
	UpperBodyBoneWeights.Reset();
	if (Skeleton == nullptr || UpperBodyBranchBone.IsNone())
	{
		return;
	}
	const FReferenceSkeleton& Bones = Skeleton->GetReferenceSkeleton();
	const int32 Branch = Bones.FindBoneIndex(UpperBodyBranchBone);
	if (Branch == INDEX_NONE)
	{
		return;
	}
	UpperBodyBoneWeights.SetNumZeroed(Bones.GetNum());
	FAnimationRuntime::FillBranchBoneWeights(Bones, Branch, UpperBodyBoneWeights);
}

// Players and notifies

void UAnimInstance::QueueNotifies(const UAnimSequenceBase* Sequence, const UAnimSequenceBase* Source, float Position,
	float DeltaTime, float Length, bool bLoop)
{
	if (Sequence == nullptr || Sequence->Notifies.Num() == 0 || DeltaTime <= 0.0f || Length <= 1.0e-4f)
	{
		return;
	}
	TArray<const FAnimNotifyEvent*, TInlineAllocator<8>> Crossed;
	if (!bLoop)
	{
		const float End = FMath::Min(Position + DeltaTime, Length);
		Sequence->GetAnimNotifiesFromDeltaPositions(Position, End, End >= Length, Crossed);
	}
	else
	{
		float From = FMath::Clamp(Position, 0.0f, Length);
		float Remaining = DeltaTime;
		for (int32 Loop = 0; Loop < MaxLoopsPerUpdate && Remaining > 0.0f; ++Loop)
		{
			const float To = From + Remaining;
			if (To < Length)
			{
				Sequence->GetAnimNotifiesFromDeltaPositions(From, To, false, Crossed);
				break;
			}
			// To the end, then around from the start.
			Sequence->GetAnimNotifiesFromDeltaPositions(From, Length, false, Crossed);
			Remaining -= Length - From;
			From = 0.0f;
			// A notify at 0 fires as the loop comes round, once.
			if (Remaining <= 0.0f)
			{
				break;
			}
		}
	}
	for (const FAnimNotifyEvent* Event : Crossed)
	{
		NotifyQueue.Add({Event, Source});
	}
}

void UAnimInstance::AdvanceSequencePlayer(
	const UAnimSequenceBase* Sequence, float& InOutTime, float DeltaTime, bool bQueueNotifies)
{
	if (Sequence == nullptr || DeltaTime <= 0.0f)
	{
		return;
	}
	const float Length = Sequence->GetPlayLength();
	if (bQueueNotifies)
	{
		QueueNotifies(Sequence, Sequence, InOutTime, DeltaTime, Length, Sequence->bLoop);
	}
	InOutTime += DeltaTime;
	if (Length <= 1.0e-4f)
	{
		return;
	}
	if (Sequence->bLoop)
	{
		InOutTime = FMath::Fmod(InOutTime, Length);
	}
	else
	{
		InOutTime = FMath::Min(InOutTime, Length);
	}
}

void UAnimInstance::TriggerAnimNotifies()
{
	NumNotifiesFiredLastUpdate = NotifyQueue.Num();
	// A handler may play a montage (which queues nothing now) but not update this instance again: the queue is stable.
	for (int32 Index = 0; Index < NotifyQueue.Num(); ++Index)
	{
		const FQueuedAnimNotify Queued = NotifyQueue[Index];
		const FAnimNotifyEvent& Event = *Queued.Event;
		UAnimSequenceBase* Animation = const_cast<UAnimSequenceBase*>(Queued.Animation);
		if (Event.Notify != nullptr)
		{
			Event.Notify->Notify(OwningMesh, Animation);
		}
		const FName Name =
			!Event.NotifyName.IsNone() || Event.Notify == nullptr ? Event.NotifyName : Event.Notify->GetNotifyName();
		OnAnimNotify.Broadcast(Name, Queued.Animation);
	}
	NotifyQueue.Reset();
}

// Locomotion

void UAnimInstance::UpdateLocomotion(float DeltaTime)
{
	if (LocomotionBlendInterpSpeed <= 1.0e-6f)
	{
		BlendInput = BlendInputTarget;
	}
	else
	{
		const float T = 1.0f - FMath::Exp(-LocomotionBlendInterpSpeed * DeltaTime);
		BlendInput += (BlendInputTarget - BlendInput) * T;
	}

	LocomotionSamples.Reset();
	if (BlendSpace == nullptr)
	{
		return;
	}
	BlendSpace->GetSamplesFromBlendInput(BlendInput, LocomotionSamples);
	const float Length = BlendSpace->GetAnimationLengthFromSampleData(LocomotionSamples);
	if (Length <= 1.0e-4f || DeltaTime <= 0.0f)
	{
		return;
	}
	// One normalized time for every sample (UE's length-based sync): the weighted length sets the pace.
	const float Previous = LocomotionNormalizedTime;
	const float DeltaNormalized = DeltaTime / Length;
	LocomotionNormalizedTime = FMath::Fmod(Previous + DeltaNormalized, 1.0f);

	// The highest weighted sample's notifies (UE's default trigger mode for blend spaces).
	const int32 Highest = UBlendSpaceBase::GetHighestWeightedSample(LocomotionSamples);
	const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
	const UAnimSequence* Leader = Samples.IsValidIndex(Highest) ? Samples[Highest].Animation : nullptr;
	if (Leader != nullptr)
	{
		const float LeaderLength = Leader->GetPlayLength();
		QueueNotifies(Leader, Leader, Previous * LeaderLength, DeltaNormalized * LeaderLength, LeaderLength, true);
	}
}

void UAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	UpdateLocomotion(DeltaTime);
}

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	NotifyQueue.Reset();
	NativeUpdateAnimation(DeltaSeconds);
	UpdateMontages(DeltaSeconds);
	TriggerAnimNotifies();
}

// Montages

float UAnimInstance::Montage_Play(UAnimMontage* Montage, float InPlayRate, float InTimeToStartMontageAt)
{
	if (Montage == nullptr || Montage->Animation == nullptr || InPlayRate <= 0.0f)
	{
		return 0.0f;
	}
	// The montages on the slot blend out as this one blends in.
	FBlendingOutList BlendingOut;
	for (FAnimMontageInstance& Other : MontageInstances)
	{
		if (Other.Montage != nullptr && Other.Montage->SlotName == Montage->SlotName && !Other.IsStopped())
		{
			BeginMontageBlendOut(Other, Montage->BlendInTime, true);
			BlendingOut.Add(TPair<UAnimMontage*, bool>(Other.Montage, true));
		}
	}
	FAnimMontageInstance& Instance = MontageInstances.AddDefaulted_GetRef();
	Instance.Montage = Montage;
	Instance.PlayRate = InPlayRate;
	Instance.Position = FMath::Clamp(InTimeToStartMontageAt, 0.0f, Montage->GetPlayLength());
	Instance.TargetWeight = 1.0f;
	Instance.BlendStartWeight = 0.0f;
	Instance.BlendTime = FMath::Max(Montage->BlendInTime, 0.0f);
	Instance.BlendElapsed = 0.0f;
	Instance.Weight = Instance.BlendTime > 0.0f ? 0.0f : 1.0f;
	for (const TPair<UAnimMontage*, bool>& Entry : BlendingOut)
	{
		OnMontageBlendingOut.Broadcast(Entry.Key, Entry.Value);
	}
	return Montage->GetPlayLength();
}

void UAnimInstance::BeginMontageBlendOut(FAnimMontageInstance& Instance, float BlendOutTime, bool bInterrupted)
{
	if (Instance.IsStopped())
	{
		return;
	}
	Instance.TargetWeight = 0.0f;
	Instance.BlendStartWeight = Instance.Weight;
	Instance.BlendTime = FMath::Max(BlendOutTime, 0.0f);
	Instance.BlendElapsed = 0.0f;
	Instance.bInterrupted = Instance.bInterrupted || bInterrupted;
	if (Instance.BlendTime <= 0.0f)
	{
		Instance.Weight = 0.0f;
	}
}

void UAnimInstance::Montage_Stop(float InBlendOutTime, const UAnimMontage* Montage)
{
	FBlendingOutList BlendingOut;
	for (FAnimMontageInstance& Instance : MontageInstances)
	{
		if ((Montage == nullptr || Instance.Montage == Montage) && !Instance.IsStopped())
		{
			BeginMontageBlendOut(Instance, InBlendOutTime, true);
			BlendingOut.Add(TPair<UAnimMontage*, bool>(Instance.Montage, true));
		}
	}
	for (const TPair<UAnimMontage*, bool>& Entry : BlendingOut)
	{
		OnMontageBlendingOut.Broadcast(Entry.Key, Entry.Value);
	}
}

FAnimMontageInstance* UAnimInstance::FindMontageInstance(const UAnimMontage* Montage)
{
	for (int32 Index = MontageInstances.Num() - 1; Index >= 0; --Index)
	{
		FAnimMontageInstance& Instance = MontageInstances[Index];
		if (Montage != nullptr ? Instance.Montage == Montage : !Instance.IsStopped())
		{
			return &Instance;
		}
	}
	return nullptr;
}

const FAnimMontageInstance* UAnimInstance::FindMontageInstance(const UAnimMontage* Montage) const
{
	return const_cast<UAnimInstance*>(this)->FindMontageInstance(Montage);
}

bool UAnimInstance::Montage_IsPlaying(const UAnimMontage* Montage) const
{
	const FAnimMontageInstance* Instance = FindMontageInstance(Montage);
	return Instance != nullptr && !Instance->IsStopped();
}

bool UAnimInstance::Montage_IsActive(const UAnimMontage* Montage) const
{
	return Montage != nullptr ? FindMontageInstance(Montage) != nullptr : MontageInstances.Num() > 0;
}

float UAnimInstance::Montage_GetPosition(const UAnimMontage* Montage) const
{
	const FAnimMontageInstance* Instance = FindMontageInstance(Montage);
	return Instance != nullptr ? Instance->Position : 0.0f;
}

UAnimMontage* UAnimInstance::GetCurrentActiveMontage() const
{
	const FAnimMontageInstance* Instance = FindMontageInstance(nullptr);
	return Instance != nullptr ? Instance->Montage : nullptr;
}

float UAnimInstance::GetSlotMontageGlobalWeight(FName SlotNodeName) const
{
	float Weight = 0.0f;
	for (const FAnimMontageInstance& Instance : MontageInstances)
	{
		if (Instance.Montage != nullptr && Instance.Montage->SlotName == SlotNodeName)
		{
			Weight = FMath::Max(Weight, Instance.Weight);
		}
	}
	return Weight;
}

void UAnimInstance::Montage_JumpToSection(FName SectionName, const UAnimMontage* Montage)
{
	FAnimMontageInstance* Instance = FindMontageInstance(Montage);
	if (Instance == nullptr || Instance->IsStopped())
	{
		return;
	}
	const int32 Section = Instance->Montage->GetSectionIndex(SectionName);
	if (Section == INDEX_NONE)
	{
		return;
	}
	float Start = 0.0f;
	float End = 0.0f;
	Instance->Montage->GetSectionStartAndEndTime(Section, Start, End);
	Instance->Position = Start;
	Instance->bPlaying = true;
}

void UAnimInstance::Montage_SetNextSection(FName SectionNameToChange, FName NextSection, const UAnimMontage* Montage)
{
	FAnimMontageInstance* Instance = FindMontageInstance(Montage);
	if (Instance == nullptr)
	{
		return;
	}
	const int32 From = Instance->Montage->GetSectionIndex(SectionNameToChange);
	if (From == INDEX_NONE)
	{
		return;
	}
	Instance->NextSectionOverrideFrom = From;
	Instance->NextSectionOverrideTo = Instance->Montage->GetSectionIndex(NextSection);
}

int32 UAnimInstance::GetNextSection(const FAnimMontageInstance& Instance, int32 SectionIndex) const
{
	const UAnimMontage& Montage = *Instance.Montage;
	if (SectionIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	if (Instance.NextSectionOverrideFrom == SectionIndex)
	{
		return Instance.NextSectionOverrideTo;
	}
	const FName Link = Montage.CompositeSections[SectionIndex].NextSectionName;
	if (!Link.IsNone())
	{
		return Montage.GetSectionIndex(Link);
	}
	return SectionIndex + 1 < Montage.CompositeSections.Num() ? SectionIndex + 1 : INDEX_NONE;
}

float UAnimInstance::GetMontageTimeToEnd(const FAnimMontageInstance& Instance) const
{
	const UAnimMontage& Montage = *Instance.Montage;
	int32 Section = Montage.GetSectionIndexFromPosition(Instance.Position);
	if (Section == INDEX_NONE)
	{
		return Montage.GetPlayLength() - Instance.Position;
	}
	float Start = 0.0f;
	float End = 0.0f;
	Montage.GetSectionStartAndEndTime(Section, Start, End);
	float Time = End - Instance.Position;
	for (int32 Step = 0; Step < Montage.CompositeSections.Num(); ++Step)
	{
		Section = GetNextSection(Instance, Section);
		if (Section == INDEX_NONE)
		{
			return Time;
		}
		Montage.GetSectionStartAndEndTime(Section, Start, End);
		Time += End - Start;
	}
	// More steps than sections: a loop.
	return TNumericLimits<float>::Max();
}

void UAnimInstance::UpdateMontages(float DeltaTime)
{
	if (MontageInstances.Num() == 0)
	{
		return;
	}
	FBlendingOutList BlendingOut;
	for (FAnimMontageInstance& Instance : MontageInstances)
	{
		UAnimMontage& Montage = *Instance.Montage;

		// The weight's blend.
		if (Instance.BlendElapsed < Instance.BlendTime)
		{
			Instance.BlendElapsed = FMath::Min(Instance.BlendElapsed + DeltaTime, Instance.BlendTime);
			const float Alpha = Instance.BlendElapsed / Instance.BlendTime;
			Instance.Weight = FMath::Lerp(Instance.BlendStartWeight, Instance.TargetWeight, Alpha);
		}
		else
		{
			Instance.Weight = Instance.TargetWeight;
		}

		// The position, through the sections.
		if (Instance.bPlaying && DeltaTime > 0.0f)
		{
			const bool bNotifies = !Instance.bInterrupted;
			float Remaining = DeltaTime * Instance.PlayRate;
			for (int32 Step = 0; Step < MaxLoopsPerUpdate && Remaining > 0.0f; ++Step)
			{
				const int32 Section = Montage.GetSectionIndexFromPosition(Instance.Position);
				float Start = 0.0f;
				float End = Montage.GetPlayLength();
				if (Section != INDEX_NONE)
				{
					Montage.GetSectionStartAndEndTime(Section, Start, End);
				}
				const int32 Next = GetNextSection(Instance, Section);
				const bool bEndsMontage = Next == INDEX_NONE;
				const float Advance = FMath::Min(Remaining, End - Instance.Position);
				if (bNotifies)
				{
					const bool bReachesEnd = Instance.Position + Remaining >= End;
					// The clip's notifies and the montage's own, the end's included when the montage ends there.
					TArray<const FAnimNotifyEvent*, TInlineAllocator<8>> Crossed;
					const float To = Instance.Position + Advance;
					const bool bIncludeEnd = bReachesEnd && bEndsMontage;
					if (Montage.Animation != nullptr)
					{
						Montage.Animation->GetAnimNotifiesFromDeltaPositions(
							Instance.Position, To, bIncludeEnd, Crossed);
						for (const FAnimNotifyEvent* Event : Crossed)
						{
							NotifyQueue.Add({Event, Montage.Animation});
						}
						Crossed.Reset();
					}
					Montage.GetAnimNotifiesFromDeltaPositions(Instance.Position, To, bIncludeEnd, Crossed);
					for (const FAnimNotifyEvent* Event : Crossed)
					{
						NotifyQueue.Add({Event, &Montage});
					}
				}
				if (Instance.Position + Remaining < End)
				{
					Instance.Position += Remaining;
					Remaining = 0.0f;
					break;
				}
				Remaining -= End - Instance.Position;
				if (bEndsMontage)
				{
					Instance.Position = End;
					Instance.bPlaying = false;
					break;
				}
				float NextStart = 0.0f;
				float NextEnd = 0.0f;
				Montage.GetSectionStartAndEndTime(Next, NextStart, NextEnd);
				Instance.Position = NextStart;
			}
		}

		// The automatic blend out: it starts BlendOutTime before the end, and lasts what is left.
		if (!Instance.IsStopped())
		{
			const float TimeToEnd = Instance.bPlaying ? GetMontageTimeToEnd(Instance) / Instance.PlayRate : 0.0f;
			if (TimeToEnd <= Montage.BlendOutTime + 1.0e-5f)
			{
				BeginMontageBlendOut(Instance, TimeToEnd, false);
				BlendingOut.Add(TPair<UAnimMontage*, bool>(Instance.Montage, false));
			}
		}
	}
	for (const TPair<UAnimMontage*, bool>& Entry : BlendingOut)
	{
		OnMontageBlendingOut.Broadcast(Entry.Key, Entry.Value);
	}

	// The montages whose weight reached 0 end, in the order they were played.
	for (int32 Index = 0; Index < MontageInstances.Num();)
	{
		const FAnimMontageInstance& Instance = MontageInstances[Index];
		if (Instance.IsStopped() && Instance.BlendElapsed >= Instance.BlendTime)
		{
			UAnimMontage* Ended = Instance.Montage;
			const bool bInterrupted = Instance.bInterrupted;
			MontageInstances.RemoveAt(Index);
			OnMontageEnded.Broadcast(Ended, bInterrupted);
			continue;
		}
		++Index;
	}
}

// Evaluation

void UAnimInstance::GetReferencePose(TArrayView<FTransform> OutPose) const
{
	if (Skeleton == nullptr)
	{
		return;
	}
	const TArray<FTransform>& RefPose = Skeleton->GetReferenceSkeleton().RefBonePose;
	check(OutPose.Num() == RefPose.Num());
	for (int32 Bone = 0; Bone < RefPose.Num(); ++Bone)
	{
		OutPose[Bone] = RefPose[Bone];
	}
}

void UAnimInstance::SampleSequencePose(const UAnimSequence* Sequence, float Time, TArrayView<FTransform> OutPose) const
{
	if (Sequence == nullptr || Sequence->GetNumberOfFrames() <= 0 || Sequence->GetNumberOfTracks() != GetNumBones())
	{
		GetReferencePose(OutPose);
		return;
	}
	Sequence->GetBonePose(Time, OutPose);
}

void UAnimInstance::SampleLocomotionPose(TArrayView<FTransform> OutPose) const
{
	SampleBlendSpacePose(BlendSpace, LocomotionSamples, LocomotionNormalizedTime, OutPose);
}

void UAnimInstance::SampleBlendSpacePose(const UBlendSpaceBase* Space, const FBlendSampleDataArray& SampleData,
	float NormalizedTime, TArrayView<FTransform> OutPose) const
{
	const TArray<FBlendSample>* Samples = Space != nullptr ? &Space->GetBlendSamples() : nullptr;
	if (Samples == nullptr || SampleData.Num() == 0)
	{
		GetReferencePose(OutPose);
		return;
	}
	auto SampleAt = [&](const FBlendSampleData& Data, TArrayView<FTransform> Out)
	{
		const UAnimSequence* Clip =
			Samples->IsValidIndex(Data.SampleDataIndex) ? (*Samples)[Data.SampleDataIndex].Animation : nullptr;
		SampleSequencePose(Clip, Clip != nullptr ? NormalizedTime * Clip->GetPlayLength() : 0.0f, Out);
	};
	if (SampleData.Num() == 1)
	{
		SampleAt(SampleData[0], OutPose);
		return;
	}
	FMemMark Mark(FMemStack::Get());
	const int32 NumBones = OutPose.Num();
	FStackPose Poses;
	Poses.SetNum(NumBones * SampleData.Num());
	TArrayView<const FTransform> Views[3];
	float Weights[3];
	const int32 NumSamples = FMath::Min(SampleData.Num(), 3);
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		TArrayView<FTransform> Pose(Poses.GetData() + (Index * NumBones), NumBones);
		SampleAt(SampleData[Index], Pose);
		Views[Index] = Pose;
		Weights[Index] = SampleData[Index].TotalWeight;
	}
	FAnimationRuntime::BlendPosesTogether(
		TArrayView<const TArrayView<const FTransform>>(Views, NumSamples), Weights, OutPose);
}

void UAnimInstance::EvaluateBasePose(TArrayView<FTransform> OutPose) const
{
	SampleLocomotionPose(OutPose);
}

void UAnimInstance::ApplySlotsAndAimOffset(TArrayView<FTransform> InOutPose) const
{
	const int32 NumBones = InOutPose.Num();
	FMemMark Mark(FMemStack::Get());
	FStackPose SlotPose;
	for (const FAnimMontageInstance& Instance : MontageInstances)
	{
		if (Instance.Weight <= 0.0f || Instance.Montage == nullptr)
		{
			continue;
		}
		const bool bUpperBody = Instance.Montage->SlotName == UpperBodySlotName;
		if (bUpperBody && UpperBodyBoneWeights.Num() != NumBones)
		{
			// An upper-body montage without a branch bone has nothing to move.
			continue;
		}
		SlotPose.SetNum(NumBones, false);
		SampleSequencePose(Instance.Montage->Animation, Instance.Position, SlotPose);
		if (bUpperBody)
		{
			FAnimationRuntime::BlendPosesPerBoneFilter(InOutPose, SlotPose, UpperBodyBoneWeights, Instance.Weight);
		}
		else
		{
			FAnimationRuntime::BlendTwoPosesTogether(InOutPose, SlotPose, Instance.Weight, InOutPose);
		}
	}

	if (AimOffset == nullptr)
	{
		return;
	}
	FBlendSampleDataArray AimSamples;
	AimOffset->GetSamplesFromBlendInput(FVector(AimPitch, 0.0f, 0.0f), AimSamples);
	const UAnimSequence* Base = AimOffset->GetAdditiveBasePose();
	if (AimSamples.Num() == 0 || Base == nullptr)
	{
		return;
	}
	// The aim poses (their first frames) blended, then made additive against the base pose.
	const TArray<FBlendSample>& Samples = AimOffset->GetBlendSamples();
	FStackPose Aim;
	Aim.SetNum(NumBones * (AimSamples.Num() + 1));
	TArrayView<FTransform> Additive(Aim.GetData() + (NumBones * AimSamples.Num()), NumBones);
	if (AimSamples.Num() == 1)
	{
		SampleSequencePose(Samples[AimSamples[0].SampleDataIndex].Animation, 0.0f, Additive);
	}
	else
	{
		TArrayView<const FTransform> Views[3];
		float Weights[3];
		const int32 NumSamples = FMath::Min(AimSamples.Num(), 3);
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			TArrayView<FTransform> Pose(Aim.GetData() + (Index * NumBones), NumBones);
			SampleSequencePose(Samples[AimSamples[Index].SampleDataIndex].Animation, 0.0f, Pose);
			Views[Index] = Pose;
			Weights[Index] = AimSamples[Index].TotalWeight;
		}
		FAnimationRuntime::BlendPosesTogether(
			TArrayView<const TArrayView<const FTransform>>(Views, NumSamples), Weights, Additive);
	}
	TArrayView<FTransform> BasePose(Aim.GetData(), NumBones);
	SampleSequencePose(Base, 0.0f, BasePose);
	FAnimationRuntime::ConvertPoseToAdditive(Additive, BasePose);
	FAnimationRuntime::AccumulateAdditivePose(InOutPose, Additive, 1.0f,
		UpperBodyBoneWeights.Num() == NumBones ? TArrayView<const float>(UpperBodyBoneWeights)
											   : TArrayView<const float>());
}

void UAnimInstance::EvaluatePose(TArray<FTransform>& OutPose) const
{
	const int32 NumBones = GetNumBones();
	if (NumBones <= 0)
	{
		OutPose.Reset();
		return;
	}
	OutPose.SetNum(NumBones, false);
	EvaluateBasePose(OutPose);
	ApplySlotsAndAimOffset(OutPose);
}

void UAnimInstance::GetSkinMatrices(TArray<FMatrix>& OutSkin) const
{
	OutSkin.Reset();
	if (Skeleton == nullptr)
	{
		return;
	}
	TArray<FTransform> Pose;
	EvaluatePose(Pose);
	TArray<FMatrix> ComponentSpace;
	FAnimationRuntime::FillUpComponentSpaceTransforms(Skeleton->GetReferenceSkeleton(), Pose, ComponentSpace);
	FAnimationRuntime::GetSkinMatrices(Skeleton->GetReferenceSkeleton(), ComponentSpace, OutSkin);
}
