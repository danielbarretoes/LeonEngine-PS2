#pragma once

#include "AudioDevice.h"
#include "CoreMinimal.h"
#include "Serialization/BulkData.h"
#include "Sound/SoundBase.h"
#include "SoundWave.generated.h"

class UAssetImportData;

/**
 * A sound asset holding its samples (UE: USoundWave): 16-bit signed PCM, the channels interleaved, in bulk data. UE
 * keeps the imported `.wav` file in RawData and the cooked, compressed audio for the platform; Leon keeps the PCM
 * (RawPCMData) and compresses nothing.
 *
 * The audio device plays the samples from memory (UGameplayStatics::PlaySound2D / PlaySoundAtLocation, the UI cues
 * of the engine config); LeonEd's USoundFactory imports `.wav` files into it.
 */
UCLASS()
class ENGINE_API USoundWave : public USoundBase
{
	GENERATED_BODY()

public:
	USoundWave(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Interleaved channels: 1 mono, 2 stereo (UE: NumChannels). */
	UPROPERTY()
	int32 NumChannels = 0;

	/** Frames per second (UE: SampleRate). */
	UPROPERTY()
	int32 SampleRate = 0;

#if WITH_EDITORONLY_DATA
	/** Where the sound was imported from: made by the factory that imported it, dropped by the cook (UE:
	 * AssetImportData). */
	UPROPERTY(Instanced)
	UAssetImportData* AssetImportData = nullptr;
#endif

	/**
	 * Replaces the samples with NumFrames frames of InNumChannels interleaved PCM16 samples and sets the channels, the
	 * rate and the duration (Leon; UE's importer fills RawData). False, and nothing changed, for no channels or no
	 * rate.
	 */
	bool SetPCMData(const int16* Samples, int32 NumFrames, int32 InNumChannels, int32 InSampleRate);

	/** Number of frames: samples per channel. */
	[[nodiscard]] int32 GetNumFrames() const;

	/** Copies the interleaved samples out. */
	void GetPCMData(TArray<int16>& OutSamples) const;

	/** Copies the samples into OutSamples and describes them for the audio device (Leon). */
	[[nodiscard]] FSoundWavePCM GetPCMView(TArray<int16>& OutSamples) const;

	/** The samples as bulk data (bytes, little-endian PCM16). */
	[[nodiscard]] const FByteBulkData& GetRawPCMData() const
	{
		return RawPCMData;
	}

	/** The tagged properties, then the samples as bulk data. */
	void Serialize(FArchive& Ar) override;

private:
	FByteBulkData RawPCMData;
};
