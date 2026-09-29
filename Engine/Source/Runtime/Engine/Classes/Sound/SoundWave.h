#pragma once

#include "CoreMinimal.h"
#include "Serialization/BulkData.h"
#include "Sound/SoundBase.h"
#include "SpuAdpcm.h"
#include "SoundWave.generated.h"

class UAssetImportData;

/**
 * A sound asset (UE: USoundWave). Imported, it keeps its source samples, 16-bit PCM with the channels interleaved
 * (RawPCMData, editor-only: UE's RawData), and the settings its SPU2 ADPCM is made with; cooked, it keeps only that
 * ADPCM (UE: the cooked data of the platform's format), the one format the audio device plays on every platform
 * (Docs/PLANS/ps2-shipping.md N19, FSpuAdpcmSound). An editor build makes the ADPCM from the PCM when it needs it (the
 * cook, and the desktop game on uncooked content) with the AudioCompressor module, so the desktop plays what the PS2
 * plays.
 *
 * UGameplayStatics::PlaySound2D / PlaySoundAtLocation play it through the engine's audio device, which keeps its ADPCM
 * resident (in SPU2 RAM on the PS2) from its load to its destruction (GetSoundBuffer); LeonEd's USoundFactory imports
 * `.wav` files into it.
 */
UCLASS()
class ENGINE_API USoundWave : public USoundBase
{
	GENERATED_BODY()

public:
	USoundWave(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Whether it loops (UE: bLooping): the ADPCM's last block returns to its loop start. */
	UPROPERTY()
	bool bLooping = false;

#if WITH_EDITORONLY_DATA
	/** The source's interleaved channels: 1 mono, 2 stereo (UE: NumChannels). The ADPCM is mono: they are averaged. */
	UPROPERTY()
	int32 NumChannels = 0;

	/** The source's frames per second (UE: SampleRate). */
	UPROPERTY()
	int32 SampleRate = 0;

	/**
	 * The rate the ADPCM is made at (Leon; UE: the platform's sample rate quality): 22 050 Hz by default, for effects;
	 * 44 100 or 48 000 (the SPU2's) only where the highs matter. A lower source keeps its own rate, and 0 keeps the
	 * source's whatever it is.
	 */
	UPROPERTY()
	int32 CompressionSampleRate = 22050;

	/** The source frame a looping sound returns to (bLooping). */
	UPROPERTY()
	int32 LoopStartFrame = 0;

	/** Where the sound was imported from: made by the factory that imported it, dropped by the cook (UE:
	 * AssetImportData). */
	UPROPERTY(Instanced)
	UAssetImportData* AssetImportData = nullptr;

	/**
	 * Replaces the source with NumFrames frames of InNumChannels interleaved PCM16 samples and sets the channels, the
	 * rate and the duration, forgetting the ADPCM (Leon; UE's importer fills RawData). False, and nothing changed, for
	 * no channels or no rate.
	 */
	bool SetPCMData(const int16* Samples, int32 NumFrames, int32 InNumChannels, int32 InSampleRate);

	/** The source's frames: samples per channel. */
	[[nodiscard]] int32 GetNumFrames() const;

	/** Copies the source's interleaved samples out. */
	void GetPCMData(TArray<int16>& OutSamples) const;

	/** The source's samples as bulk data (bytes, little-endian PCM16). */
	[[nodiscard]] const FByteBulkData& GetRawPCMData() const
	{
		return RawPCMData;
	}

	/**
	 * Makes the ADPCM from the source and the settings when there is none (FSpuAdpcmEncoder: mono, resampled to
	 * CompressionSampleRate, the loop on a block; the same bytes every time). True when the sound has its ADPCM; false
	 * (logged) without source samples. The cook calls it for every sound; the audio device's first use of an uncooked
	 * sound too.
	 */
	bool CacheCompressedData();

	/** Forgets the ADPCM (the source or the settings changed); the next CacheCompressedData makes it again. */
	void InvalidateCompressedData();
#endif

	/** Whether it has its ADPCM (cooked, or made by CacheCompressedData). */
	[[nodiscard]] bool HasCompressedData() const;

	/** The ADPCM in place, described for the audio device, until UnlockCompressedData; invalid without one. */
	[[nodiscard]] FSpuAdpcmSound LockCompressedData() const;
	/** Ends a LockCompressedData. */
	void UnlockCompressedData() const;

	/** The ADPCM's bytes: what the sound takes in SPU2 RAM. */
	[[nodiscard]] int32 GetCompressedDataSize() const;

	/** The rate the ADPCM plays at. */
	[[nodiscard]] int32 GetCompressedSampleRate() const
	{
		return CompressedSampleRate;
	}

	/**
	 * Replaces the ADPCM: whole blocks at InSampleRate, looping from InLoopStartFrame (a block's first frame) or
	 * INDEX_NONE for a one-shot. False, and nothing changed, for anything else.
	 */
	bool SetCompressedData(const uint8* Blocks, int32 NumBytes, int32 InSampleRate, int32 InLoopStartFrame);

	/**
	 * The engine audio device's buffer of the sound (UE: InitAudioResource / ResourceID): uploaded the first time (to
	 * SPU2 RAM on the PS2; on uncooked content once its ADPCM is made) and kept referenced while the sound lives.
	 * INDEX_NONE when the device is silent or the sound does not fit. PostLoad calls it: a sound is uploaded when it
	 * loads, not when it first plays.
	 */
	int32 GetSoundBuffer();

	/** The tagged properties, then the source samples (uncooked) or the ADPCM (cooked, PKG_FilterEditorOnly). */
	void Serialize(FArchive& Ar) override;
	void PostLoad() override;
	void BeginDestroy() override;

private:
#if WITH_EDITORONLY_DATA
	FByteBulkData RawPCMData;
#endif
	/** The ADPCM blocks (FSpuAdpcm::BytesPerBlock each). */
	FByteBulkData CompressedData;
	int32 CompressedSampleRate = 0;
	int32 CompressedLoopStartFrame = INDEX_NONE;
	/** The engine audio device's buffer and the device serial it belongs to (FAudioDevice::GetSerial). */
	int32 SoundBuffer = INDEX_NONE;
	uint32 SoundBufferSerial = 0;
};
