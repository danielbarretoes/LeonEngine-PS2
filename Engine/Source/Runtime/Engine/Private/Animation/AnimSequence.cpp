#include "Animation/AnimSequence.h"

#include "Animation/AnimNotify.h"
#include "AssetBulkData.h"
#include "EngineLogs.h"
#include "HAL/LowLevelMemTracker.h"

UAnimationAsset::UAnimationAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAnimSequenceBase::UAnimSequenceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UAnimSequenceBase::AddNotify(FName InNotifyName, float Time, UAnimNotify* InNotify)
{
	FAnimNotifyEvent Event;
	Event.TriggerTime = FMath::Clamp(Time, 0.0f, FMath::Max(SequenceLength, 0.0f));
	Event.NotifyName = InNotifyName;
	Event.Notify = InNotify;
	int32 Index = Notifies.Num();
	while (Index > 0 && Notifies[Index - 1].TriggerTime > Event.TriggerTime)
	{
		--Index;
	}
	Notifies.Insert(Event, Index);
	return Index;
}

UAnimNotify::UAnimNotify(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimNotify::Notify(USkeletalMeshComponent* /*MeshComp*/, UAnimSequenceBase* /*Animation*/)
{
}

FName UAnimNotify::GetNotifyName() const
{
	return GetClass()->GetFName();
}

UAnimSequence::UAnimSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UAnimSequence::SetFromRawAnimSequence(const FRawAnimSequence& Raw)
{
	return SetFromRawAnimSequence(Raw, FAnimCompressionSettings::Load());
}

bool UAnimSequence::SetFromRawAnimSequence(const FRawAnimSequence& Raw, const FAnimCompressionSettings& Settings)
{
	SequenceLength = Raw.SequenceLength;
	FrameRate = Raw.FrameRate;
	bLoop = Raw.bLoop;
	Notifies.Reset();
	for (const FRawAnimNotify& RawNotify : Raw.Notifies)
	{
		(void)AddNotify(RawNotify.NotifyName, RawNotify.Time);
	}
	if (!FAnimCompression::Compress(Raw, Settings, CompressedData))
	{
		NumFrames = 0;
		return false;
	}
	NumFrames = CompressedData.NumFrames;
	return true;
}

bool UAnimSequence::IsFinished(float TimeSeconds) const
{
	if (bLoop || SequenceLength <= 1.0e-4f)
	{
		return false;
	}
	return TimeSeconds >= (SequenceLength - 1.0e-4f);
}

float UAnimSequence::GetFrameAtTime(float TimeSeconds) const
{
	float Time = TimeSeconds;
	if (SequenceLength > 1.0e-4f)
	{
		if (bLoop)
		{
			Time = FMath::Fmod(Time, SequenceLength);
			if (Time < 0.0f)
			{
				Time += SequenceLength;
			}
		}
		else
		{
			Time = FMath::Clamp(Time, 0.0f, SequenceLength);
		}
	}
	const float LastFrame = float(FMath::Max(NumFrames - 1, 0));
	return FMath::Clamp(Time * FrameRate, 0.0f, LastFrame);
}

void UAnimSequence::GetBonePose(float TimeSeconds, TArray<FTransform>& OutPose) const
{
	if (NumFrames <= 0 || CompressedData.IsEmpty())
	{
		OutPose.Reset();
		return;
	}
	CompressedData.GetBonePose(GetFrameAtTime(TimeSeconds), OutPose);
}

void UAnimSequence::GetBonePose(float TimeSeconds, TArrayView<FTransform> OutPose) const
{
	if (NumFrames > 0 && !CompressedData.IsEmpty())
	{
		CompressedData.GetBonePose(GetFrameAtTime(TimeSeconds), OutPose);
	}
}

void UAnimSequence::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Animation);
	Super::Serialize(Ar);
	if (!SerializeBulkPayload(Ar, this, TrackBulkData, [this](FArchive& PayloadAr) { PayloadAr << CompressedData; }))
	{
		UE_LOG(LogEngine, Error, "UAnimSequence %s: damaged keys", *GetPathName());
		CompressedData = FCompressedAnimSequence();
	}
}
