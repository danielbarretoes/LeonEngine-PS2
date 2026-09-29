#pragma once

#include "CoreMinimal.h"
#include "SkeletalAnimation.h"

/**
 * The error bounds of the key reduction (Leon; UE: the compression settings of an animation's codec), read from
 * `[/Script/Engine.AnimationSettings]` of the engine config (UE: UAnimationSettings). A key is dropped when the linear
 * interpolation of the keys kept around it stays within them at every frame of the source.
 */
struct ANIMATIONCORE_API FAnimCompressionSettings
{
	/** Degrees a bone's rotation may turn away from the source at any frame. */
	float RotationErrorToleranceDegrees = 0.1f;
	/** Centimetres a bone's translation may move away from the source at any frame. */
	float TranslationErrorTolerance = 0.05f;
	/** Largest difference of any scale component from the source's at any frame (unitless). */
	float ScaleErrorTolerance = 0.001f;

	/** The engine config's values over the defaults above (the defaults when the config cannot be read). */
	[[nodiscard]] static FAnimCompressionSettings Load();
};

/**
 * A unit quaternion in 48 bits, "smallest three": the largest component is dropped (made positive by negating the
 * whole quaternion, which is the same rotation) and the other three, each within +-1/sqrt(2), are stored as 15-bit
 * values (0 to 32766, 0 exact at 16383). Each word keeps a component in bits 0 to 14; bit 15 of words 0 and 1 is the
 * index (0..3 for X, Y, Z, W) of the dropped component, high bit in word 1. A component is off by at most 2.2e-5, a
 * rotation by less than 0.005 degrees.
 */
struct ANIMATIONCORE_API FQuantizedQuat48
{
	uint16 Words[3] = {0, 0, 0};

	/** Packs a rotation (normalized first). */
	[[nodiscard]] static FQuantizedQuat48 Quantize(const FQuat& Rotation);
	/** The unit quaternion the words hold (W not negative when it was the dropped component). */
	[[nodiscard]] FQuat Dequantize() const;
};

/**
 * Where one bone's keys are in an FCompressedAnimSequence: each channel is a run of keys (their frame numbers and their
 * values) in the sequence's arrays. A rotation or a translation channel has at least one key; a scale channel with no
 * keys is a unit scale.
 */
struct ANIMATIONCORE_API FCompressedBoneTrack
{
	int32 FirstRotationKey = 0;
	int32 NumRotationKeys = 0;
	int32 FirstTranslationKey = 0;
	int32 NumTranslationKeys = 0;
	/** The first key's value: 3 entries of Translations or FloatTranslations a key. */
	int32 FirstTranslationValue = 0;
	int32 FirstScaleKey = 0;
	int32 NumScaleKeys = 0;
	/** The translations are 3 floats a key (FloatTranslations) instead of 3 int16 (Translations). */
	bool bFloatTranslation = false;
	/** An int16 translation is Quantized * TranslationScale + TranslationBias on each axis (centimetres). */
	FVector TranslationScale = FVector::ZeroVector;
	FVector TranslationBias = FVector::ZeroVector;

	friend ANIMATIONCORE_API FArchive& operator<<(FArchive& Ar, FCompressedBoneTrack& Track);
};

/**
 * A clip's compressed keys (UE: FCompressedAnimSequence), what UAnimSequence keeps and the EE samples. Each bone of
 * the skeleton has local-space channels of keys at 30 Hz frames, reduced to the keys the others cannot interpolate
 * within FAnimCompressionSettings:
 *
 * - rotation: FQuantizedQuat48, 6 bytes a key, interpolated with a normalized lerp along the shortest arc;
 * - translation: int16 x 3 with the track's scale and bias, 6 bytes a key, when that step keeps the translation within
 *   a quarter of the tolerance (a range of 16 m at 0.05 cm); else float x 3, 12 bytes;
 * - scale: float x 3, 12 bytes a key, only for a bone whose scale is not 1;
 * - every key also has its frame number, a uint16 (2 bytes).
 *
 * Sampling decompresses to local transforms (UE: DecompressPose); the pose code blends them and builds the
 * component-space matrices once (FAnimationRuntime).
 */
struct ANIMATIONCORE_API FCompressedAnimSequence
{
	/** The frames of the source, (NumFrames - 1) / FrameRate seconds. */
	int32 NumFrames = 0;
	float FrameRate = 30.0f;
	/** One per bone of the skeleton, in its order. */
	TArray<FCompressedBoneTrack> Tracks;

	TArray<uint16> RotationFrames;
	/** 3 words a key (FQuantizedQuat48). */
	TArray<uint16> Rotations;
	TArray<uint16> TranslationFrames;
	/** 3 a key, for the tracks without bFloatTranslation. */
	TArray<int16> Translations;
	/** 3 a key, for the tracks with bFloatTranslation. */
	TArray<float> FloatTranslations;
	TArray<uint16> ScaleFrames;
	/** 3 a key. */
	TArray<float> Scales;

	[[nodiscard]] int32 GetNumTracks() const
	{
		return Tracks.Num();
	}
	[[nodiscard]] bool IsEmpty() const
	{
		return Tracks.Num() == 0 || NumFrames <= 0;
	}

	/** The keys of every channel (the payload of a UAnimSequence's bulk data). */
	[[nodiscard]] int32 GetNumKeys() const;
	/** The bytes the keys take (values and frame numbers). */
	[[nodiscard]] int32 GetKeyBytes() const;

	/**
	 * A bone's local transform at Frame (0 to NumFrames - 1, fractions between two frames): the two keys around it
	 * interpolated (the rotation with a normalized lerp).
	 */
	[[nodiscard]] FTransform SampleTrack(int32 TrackIndex, float Frame) const;

	/** Every bone's local transform at Frame into OutPose (one per track; resized, reusing its memory). */
	void GetBonePose(float Frame, TArray<FTransform>& OutPose) const;
	/** Every bone's local transform at Frame into OutPose, one transform per track (a pose on the frame's stack). */
	void GetBonePose(float Frame, TArrayView<FTransform> OutPose) const;

	/** True when the arrays are consistent: every run inside its arrays, frame numbers rising (Leon). */
	[[nodiscard]] bool IsValid() const;

	friend ANIMATIONCORE_API FArchive& operator<<(FArchive& Ar, FCompressedAnimSequence& Sequence);
};

/** Compresses sampled clips (Leon; UE: the animation compression codecs, run when an animation is imported). */
struct ANIMATIONCORE_API FAnimCompression
{
	/**
	 * Compresses Raw into Out: each channel quantized (rotations to 48 bits, translations to int16 with the track's
	 * scale and bias when the tolerance allows), then its keys reduced greedily from the first: a key is kept only when
	 * the interpolation of the kept keys (the runtime's, from their quantized values) would leave some frame between
	 * them farther from the source than Settings allows. A channel whose frames all stay within the tolerance of its
	 * first key keeps that key alone; a scale channel within the tolerance of 1 has no keys. Deterministic: the same
	 * clip gives the same arrays. False, with Out empty, for a clip without tracks or frames, or with more than 65 536
	 * frames.
	 */
	[[nodiscard]] static bool Compress(
		const FRawAnimSequence& Raw, const FAnimCompressionSettings& Settings, FCompressedAnimSequence& Out);

	/** The angle between two rotations in degrees (0 to 180; the shorter way). */
	[[nodiscard]] static float GetRotationErrorDegrees(const FQuat& A, const FQuat& B);

	/** The interpolation the runtime uses between two rotation keys: a normalized lerp along the shortest arc. */
	[[nodiscard]] static FQuat InterpolateRotation(const FQuat& A, const FQuat& B, float Alpha);
};
