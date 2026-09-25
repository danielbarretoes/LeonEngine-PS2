#include "Sound/SoundWave.h"

USoundBase::USoundBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USoundWave::USoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USoundWave::SetPCMData(const int16* Samples, int32 NumFrames, int32 InNumChannels, int32 InSampleRate)
{
	if (InNumChannels <= 0 || InSampleRate <= 0 || NumFrames < 0)
	{
		return false;
	}
	NumChannels = InNumChannels;
	SampleRate = InSampleRate;
	Duration = static_cast<float>(NumFrames) / static_cast<float>(InSampleRate);
	const int64 NumBytes = static_cast<int64>(NumFrames) * InNumChannels * static_cast<int64>(sizeof(int16));
	(void)RawPCMData.Lock(LOCK_READ_WRITE);
	void* Data = RawPCMData.Realloc(NumBytes);
	if (NumBytes > 0)
	{
		if (Samples != nullptr)
		{
			FMemory::Memcpy(Data, Samples, static_cast<SIZE_T>(NumBytes));
		}
		else
		{
			FMemory::Memzero(Data, static_cast<SIZE_T>(NumBytes));
		}
	}
	RawPCMData.Unlock();
	return true;
}

int32 USoundWave::GetNumFrames() const
{
	const int64 BytesPerFrame = static_cast<int64>(FMath::Max(NumChannels, 1)) * static_cast<int64>(sizeof(int16));
	return static_cast<int32>(RawPCMData.GetBulkDataSize() / BytesPerFrame);
}

void USoundWave::GetPCMData(TArray<int16>& OutSamples) const
{
	const int32 NumSamples = static_cast<int32>(RawPCMData.GetBulkDataSize() / static_cast<int64>(sizeof(int16)));
	OutSamples.SetNumUninitialized(NumSamples);
	if (NumSamples > 0)
	{
		FMemory::Memcpy(OutSamples.GetData(), RawPCMData.LockReadOnly(), NumSamples * sizeof(int16));
		RawPCMData.Unlock();
	}
}

FSoundWavePCM USoundWave::GetPCMView(TArray<int16>& OutSamples) const
{
	GetPCMData(OutSamples);
	FSoundWavePCM View;
	View.Samples = OutSamples.GetData();
	View.NumChannels = NumChannels;
	View.NumFrames = NumChannels > 0 ? OutSamples.Num() / NumChannels : 0;
	View.SampleRate = SampleRate;
	return View;
}

void USoundWave::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	RawPCMData.Serialize(Ar, this);
}
