#pragma once

#include "Animation/AnimSequenceBase.h"
#include "CoreMinimal.h"
#include "Serialization/BulkData.h"
#include "SkeletalAnimation.h"
#include "AnimSequence.generated.h"

class UAssetImportData;

/**
 * An animation clip asset (UE: UAnimSequence): one track of keys per bone of its skeleton, sampled at FrameRate. Leon's
 * keys are model-space bone matrices (FRawAnimSequenceTrack), blended linearly between frames; UE compresses local
 * position, rotation and scale keys. In a package the tracks are bulk data after the tagged properties.
 */
UCLASS()
class ENGINE_API UAnimSequence : public UAnimSequenceBase
{
	GENERATED_BODY()

public:
	UAnimSequence(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Keys per track (UE: NumFrames). */
	UPROPERTY()
	int32 NumFrames = 0;

	/** Keys per second (Leon; UE 4.27 derives the rate from NumFrames and SequenceLength). */
	UPROPERTY()
	float FrameRate = 30.0f;

#if WITH_EDITORONLY_DATA
	/** Where the clip was imported from: made by the factory that imported it, dropped by the cook (UE:
	 * AssetImportData). */
	UPROPERTY(Instanced)
	UAssetImportData* AssetImportData = nullptr;
#endif

	/** Replaces the tracks, one per bone, and sets NumFrames from the first (Leon; UE's factories fill them). */
	void SetRawAnimationData(TArray<FRawAnimSequenceTrack> InTracks);

	/** Takes a clip the importer baked: its length, rate, looping and tracks (Leon). */
	void SetFromRawAnimSequence(const FRawAnimSequence& Raw);

	/** One track per bone (UE: GetRawAnimationData). */
	[[nodiscard]] const TArray<FRawAnimSequenceTrack>& GetRawAnimationData() const
	{
		return RawAnimationData;
	}

	/** Number of bone tracks. */
	[[nodiscard]] int32 GetNumberOfTracks() const
	{
		return RawAnimationData.Num();
	}

	int32 GetNumberOfFrames() const override
	{
		return NumFrames;
	}

	/** True once a one-shot clip reached its end at TimeSeconds; a looping clip never finishes. */
	[[nodiscard]] bool IsFinished(float TimeSeconds) const;

	/**
	 * The bones' model-space matrices at TimeSeconds (wrapped when looping, else clamped), blended between the two
	 * nearest frames (UE: GetBonePose, local transforms there). One matrix per track; none without frames.
	 */
	void GetBonePose(float TimeSeconds, TArray<FMatrix>& OutBoneWorld) const;

	/** The tagged properties, then the tracks (bulk data). */
	void Serialize(FArchive& Ar) override;

private:
	TArray<FRawAnimSequenceTrack> RawAnimationData;
	/** The tracks in a package: filled while saving, read back and emptied while loading. */
	FByteBulkData TrackBulkData;
};
