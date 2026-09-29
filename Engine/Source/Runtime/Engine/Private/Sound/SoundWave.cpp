#include "Sound/SoundWave.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "HAL/LowLevelMemTracker.h"

#if WITH_EDITORONLY_DATA
	#include "SpuAdpcmEncoder.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSoundWave, Log, All);

USoundBase::USoundBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USoundWave::USoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITORONLY_DATA

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
	InvalidateCompressedData();
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

bool USoundWave::CacheCompressedData()
{
	if (HasCompressedData())
	{
		return true;
	}
	const int32 NumFrames = GetNumFrames();
	if (NumFrames <= 0 || NumChannels <= 0 || SampleRate <= 0)
	{
		UE_LOG(LogSoundWave, Error, "%s has no samples to make its ADPCM from", *GetPathName());
		return false;
	}
	FSpuAdpcmSettings Settings;
	Settings.SampleRate = CompressionSampleRate;
	Settings.bLooping = bLooping;
	Settings.LoopStartFrame = LoopStartFrame;
	FSpuAdpcmCompressed Compressed;
	const bool bCompressed = FSpuAdpcmEncoder::Compress(
		static_cast<const int16*>(RawPCMData.LockReadOnly()), NumFrames, NumChannels, SampleRate, Settings, Compressed);
	RawPCMData.Unlock();
	if (!bCompressed ||
		!SetCompressedData(
			Compressed.Blocks.GetData(), Compressed.Blocks.Num(), Compressed.SampleRate, Compressed.LoopStartFrame))
	{
		UE_LOG(LogSoundWave, Error, "%s: its ADPCM could not be made", *GetPathName());
		return false;
	}
	return true;
}

void USoundWave::InvalidateCompressedData()
{
	CompressedData.RemoveBulkData();
	CompressedSampleRate = 0;
	CompressedLoopStartFrame = INDEX_NONE;
}

#endif // WITH_EDITORONLY_DATA

bool USoundWave::HasCompressedData() const
{
	return CompressedSampleRate > 0 && CompressedData.GetBulkDataSize() > 0;
}

FSpuAdpcmSound USoundWave::LockCompressedData() const
{
	FSpuAdpcmSound Sound;
	if (!HasCompressedData())
	{
		return Sound;
	}
	Sound.Blocks = static_cast<const uint8*>(CompressedData.LockReadOnly());
	Sound.NumBlocks = static_cast<int32>(CompressedData.GetBulkDataSize() / FSpuAdpcm::BytesPerBlock);
	Sound.SampleRate = CompressedSampleRate;
	Sound.LoopStartFrame = CompressedLoopStartFrame;
	return Sound;
}

void USoundWave::UnlockCompressedData() const
{
	if (HasCompressedData())
	{
		CompressedData.Unlock();
	}
}

int32 USoundWave::GetCompressedDataSize() const
{
	return static_cast<int32>(CompressedData.GetBulkDataSize());
}

bool USoundWave::SetCompressedData(const uint8* Blocks, int32 NumBytes, int32 InSampleRate, int32 InLoopStartFrame)
{
	FSpuAdpcmSound Sound;
	Sound.Blocks = Blocks;
	Sound.NumBlocks = NumBytes / FSpuAdpcm::BytesPerBlock;
	Sound.SampleRate = InSampleRate;
	Sound.LoopStartFrame = InLoopStartFrame;
	if (NumBytes % FSpuAdpcm::BytesPerBlock != 0 || !Sound.IsValid())
	{
		return false;
	}
	(void)CompressedData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(CompressedData.Realloc(NumBytes), Blocks, static_cast<SIZE_T>(NumBytes));
	CompressedData.Unlock();
	CompressedSampleRate = InSampleRate;
	CompressedLoopStartFrame = InLoopStartFrame;
	return true;
}

int32 USoundWave::GetSoundBuffer()
{
	if (GEngine == nullptr)
	{
		return INDEX_NONE;
	}
	FAudioDevice& Device = GEngine->GetAudioDevice();
	if (!Device.IsInitialized() || Device.IsSilent())
	{
		return INDEX_NONE;
	}
	// One attempt per device: a sound that did not fit is not tried again at every play.
	if (SoundBufferSerial == Device.GetSerial())
	{
		return SoundBuffer;
	}
	SoundBufferSerial = Device.GetSerial();
	SoundBuffer = INDEX_NONE;
#if WITH_EDITORONLY_DATA
	// Uncooked content: the ADPCM the cook would make.
	if (!HasCompressedData() && !CacheCompressedData())
	{
		return INDEX_NONE;
	}
#endif
	const FSpuAdpcmSound Sound = LockCompressedData();
	SoundBuffer = Device.AcquireSoundBuffer(FName(*GetPathName()), Sound);
	UnlockCompressedData();
	return SoundBuffer;
}

void USoundWave::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Audio);
	Super::Serialize(Ar);
	if (Ar.IsFilterEditorOnly())
	{
		// Cooked: the SPU2 ADPCM, the one format of every platform.
		FName Format(FSpuAdpcm::FormatName);
		Ar << Format;
		Ar << CompressedSampleRate;
		Ar << CompressedLoopStartFrame;
		if (Ar.IsSaving() && !HasCompressedData())
		{
			UE_LOG(LogSoundWave, Error, "%s is saved cooked without its ADPCM (CacheCompressedData)", *GetPathName());
		}
		CompressedData.Serialize(Ar, this);
		if (Ar.IsLoading() && Format != FName(FSpuAdpcm::FormatName))
		{
			UE_LOG(LogSoundWave, Error, "%s was cooked as %s, not %s: cook it again", *GetPathName(),
				*Format.ToString(), FSpuAdpcm::FormatName);
			CompressedData.RemoveBulkData();
			CompressedSampleRate = 0;
		}
		return;
	}
#if WITH_EDITORONLY_DATA
	RawPCMData.Serialize(Ar, this);
#else
	UE_LOG(LogSoundWave, Error, "%s is not cooked: this build plays cooked sounds only", *GetPathName());
#endif
}

void USoundWave::PostLoad()
{
	Super::PostLoad();
	// Resident from its load (the SPU2's RAM on the PS2), not uploaded at its first play.
	(void)GetSoundBuffer();
}

void USoundWave::BeginDestroy()
{
	if (GEngine != nullptr && SoundBuffer != INDEX_NONE && SoundBufferSerial == GEngine->GetAudioDevice().GetSerial())
	{
		GEngine->GetAudioDevice().ReleaseSoundBuffer(SoundBuffer);
	}
	SoundBuffer = INDEX_NONE;
	SoundBufferSerial = 0;
	Super::BeginDestroy();
}
