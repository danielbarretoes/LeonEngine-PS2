#pragma once

#include "AnimCompression.h"
#include "Animation/AnimSequenceBase.h"
#include "CoreMinimal.h"
#include "Serialization/BulkData.h"
#include "SkeletalAnimation.h"
#include "AnimSequence.generated.h"

class UAssetImportData;

/**
 * An animation clip asset (UE: UAnimSequence): one track per bone of its skeleton, compressed local-space keys at
 * FrameRate (FCompressedAnimSequence: 48-bit rotations, int16 translations with a scale and bias, scales only where
 * they are not 1, keys the interpolation can rebuild within FAnimCompressionSettings dropped). Sampling gives local
 * transforms, which the anim instances blend before the component builds the pose's matrices once. In a package the
 * keys are bulk data after the tagged properties.
 */
UCLASS()
class ENGINE_API UAnimSequence : public UAnimSequenceBase
{
	GENERATED_BODY()

public:
	UAnimSequence(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Frames of the source, at FrameRate (UE: NumFrames). */
	UPROPERTY()
	int32 NumFrames = 0;

	/** Frames per second: 30 for imported clips (Leon; UE 4.27 derives the rate from NumFrames and SequenceLength). */
	UPROPERTY()
	float FrameRate = 30.0f;

#if WITH_EDITORONLY_DATA
	/** Where the clip was imported from: made by the factory that imported it, dropped by the cook (UE:
	 * AssetImportData). */
	UPROPERTY(Instanced)
	UAssetImportData* AssetImportData = nullptr;
#endif

	/**
	 * Takes a clip the importer sampled: its length, rate and looping, its notifies (named, FRawAnimNotify), and its
	 * tracks compressed with Settings (the engine config's by default). False, leaving the clip without keys, when it
	 * has no tracks or frames.
	 */
	bool SetFromRawAnimSequence(const FRawAnimSequence& Raw);
	bool SetFromRawAnimSequence(const FRawAnimSequence& Raw, const FAnimCompressionSettings& Settings);

	/** The compressed keys (UE: CompressedData). */
	[[nodiscard]] const FCompressedAnimSequence& GetCompressedData() const
	{
		return CompressedData;
	}

	/** Number of bone tracks. */
	[[nodiscard]] int32 GetNumberOfTracks() const
	{
		return CompressedData.GetNumTracks();
	}

	int32 GetNumberOfFrames() const override
	{
		return NumFrames;
	}

	/** True once a one-shot clip reached its end at TimeSeconds; a looping clip never finishes. */
	[[nodiscard]] bool IsFinished(float TimeSeconds) const;

	/** The frame of the clip at TimeSeconds: wrapped when looping, else clamped to the last frame. */
	[[nodiscard]] float GetFrameAtTime(float TimeSeconds) const;

	/**
	 * The bones' local transforms at TimeSeconds (UE: GetBonePose): the keys around it interpolated. One transform per
	 * track into OutPose (resized, reusing its memory); none without frames.
	 */
	void GetBonePose(float TimeSeconds, TArray<FTransform>& OutPose) const;
	/** The same into a pose with one transform per track (a pose on the frame's stack); untouched without frames. */
	void GetBonePose(float TimeSeconds, TArrayView<FTransform> OutPose) const;

	/** The tagged properties, then the keys (bulk data). */
	void Serialize(FArchive& Ar) override;

private:
	FCompressedAnimSequence CompressedData;
	/** The keys in a package: filled while saving, read back and emptied while loading. */
	FByteBulkData TrackBulkData;
};
