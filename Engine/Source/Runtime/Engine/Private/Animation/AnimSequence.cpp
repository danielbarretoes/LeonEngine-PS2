#include "Animation/AnimSequence.h"

#include "AssetBulkData.h"
#include "EngineLogs.h"

UAnimationAsset::UAnimationAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAnimSequenceBase::UAnimSequenceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAnimSequence::UAnimSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimSequence::SetRawAnimationData(TArray<FRawAnimSequenceTrack> InTracks)
{
	RawAnimationData = MoveTemp(InTracks);
	NumFrames = RawAnimationData.Num() > 0 ? RawAnimationData[0].Keys.Num() : 0;
}

void UAnimSequence::SetFromRawAnimSequence(const FRawAnimSequence& Raw)
{
	SequenceLength = Raw.SequenceLength;
	FrameRate = Raw.FrameRate;
	bLoop = Raw.bLoop;
	SetRawAnimationData(Raw.Tracks);
}

bool UAnimSequence::IsFinished(float TimeSeconds) const
{
	if (bLoop || SequenceLength <= 1.0e-4f)
	{
		return false;
	}
	return TimeSeconds >= (SequenceLength - 1.0e-4f);
}

void UAnimSequence::GetBonePose(float TimeSeconds, TArray<FMatrix>& OutBoneWorld) const
{
	const int32 FrameCount = NumFrames;
	const int32 LocalBoneCount = FrameCount > 0 ? RawAnimationData.Num() : 0;
	OutBoneWorld.Init(FMatrix::Identity, LocalBoneCount);
	if (LocalBoneCount <= 0 || FrameCount <= 0)
	{
		return;
	}

	float T = TimeSeconds;
	if (SequenceLength > 1.0e-4f)
	{
		if (bLoop)
		{
			T = FMath::Fmod(T, SequenceLength);
			if (T < 0.0f)
			{
				T += SequenceLength;
			}
		}
		else
		{
			T = FMath::Clamp(T, 0.0f, SequenceLength);
		}
	}
	const float FrameF = T * FrameRate;
	int32 F0 = 0;
	int32 F1 = 0;
	float Alpha = 0.0f;
	if (bLoop)
	{
		F0 = static_cast<int32>(FrameF) % FrameCount;
		F1 = (F0 + 1) % FrameCount;
		Alpha = FrameF - FMath::FloorToFloat(FrameF);
	}
	else
	{
		const float MaxFrame = static_cast<float>(FrameCount - 1);
		const float Clamped = FMath::Min(FrameF, MaxFrame);
		F0 = static_cast<int32>(Clamped);
		F1 = FMath::Min(F0 + 1, FrameCount - 1);
		Alpha = Clamped - FMath::FloorToFloat(Clamped);
	}

	for (int32 I = 0; I < LocalBoneCount; ++I)
	{
		const TArray<FMatrix>& Keys = RawAnimationData[I].Keys;
		if (!Keys.IsValidIndex(F0) || !Keys.IsValidIndex(F1))
		{
			continue;
		}
		// Matrix lerp is approximate but fine for a micro blend-space / crossfade.
		OutBoneWorld[I] = Keys[F0] * (1.0f - Alpha) + Keys[F1] * Alpha;
	}
}

void UAnimSequence::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (!SerializeBulkPayload(Ar, this, TrackBulkData, [this](FArchive& PayloadAr) { PayloadAr << RawAnimationData; }))
	{
		UE_LOG(LogEngine, Error, "UAnimSequence %s: damaged tracks", *GetPathName());
		RawAnimationData.Empty();
	}
}
