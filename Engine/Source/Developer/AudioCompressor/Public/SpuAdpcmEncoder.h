#pragma once

#include "CoreMinimal.h"
#include "SpuAdpcm.h"

/** How a sound becomes SPU2 ADPCM (USoundWave's compression settings). */
struct FSpuAdpcmSettings
{
	/** The rate to make it at: the source's when lower, or when 0 (FSpuAdpcmEncoder::GetTargetSampleRate). */
	int32 SampleRate = 22050;
	bool bLooping = false;
	/** The source frame the loop returns to. */
	int32 LoopStartFrame = 0;
};

/** A sound made SPU2 ADPCM (what a cooked USoundWave keeps). */
struct FSpuAdpcmCompressed
{
	/** The blocks, FSpuAdpcm::BytesPerBlock bytes each, their loop flags set. */
	TArray<uint8> Blocks;
	int32 SampleRate = 0;
	/** The loop's first frame (a block's first), or INDEX_NONE for a one-shot. */
	int32 LoopStartFrame = INDEX_NONE;

	/** The blocks described for the decoder and the audio device (valid while this lives). */
	[[nodiscard]] FSpuAdpcmSound GetSound() const;
};

/**
 * The SPU2 ADPCM encoder (Leon; UE's AudioFormatADPCM encodes Microsoft's ADPCM): PCM16 to the console's 16-byte blocks
 * of 28 samples (FSpuAdpcm), byte for byte the same on every run.
 *
 * - Each block tries the five predictor filters and the 13 shifts, each sample closed-loop against the decoder's own
 *   arithmetic (the nearest nibble to what the prediction from the decoded samples misses), and keeps the pair with the
 *   least squared error (the first on a tie).
 * - The channels are averaged: an SPU2 voice is mono, and a spatialized sound is a point.
 * - The rate goes down to the sound's (22 050 Hz for effects) through a windowed sinc (Blackman, 16 zero crossings,
 *   cut at the lower Nyquist), never up.
 * - A loop starts on a block (silence ahead of the sound moves it there), and its length is resampled to whole blocks,
 *   so the voice's jump back is seamless: its first block uses no prediction (filter 0), since the samples before it
 *   differ the first time and on each return. A one-shot ends with the end flag, which silences the voice.
 */
class AUDIOCOMPRESSOR_API FSpuAdpcmEncoder
{
public:
	/** The rate a sound is made at: Requested when the source is higher (the source's for 0), at most 48 000. */
	[[nodiscard]] static int32 GetTargetSampleRate(int32 SourceRate, int32 RequestedRate);

	/** The channels of NumFrames interleaved frames averaged into one (rounded to the nearest). */
	static void DownmixToMono(const int16* Interleaved, int32 NumFrames, int32 NumChannels, TArray<int16>& OutMono);

	/**
	 * SegmentFrames source frames from SegmentStart resampled to OutFrames (a windowed sinc; a copy when they are as
	 * many). The kernel reads Source around the segment (silence outside it), or, with bWrap, the segment as a loop.
	 */
	static void ResampleSegment(const int16* Source, int32 SourceFrames, int32 SegmentStart, int32 SegmentFrames,
		int32 OutFrames, bool bWrap, TArray<int16>& OutSamples);

	/**
	 * Encodes mono samples at their rate into blocks: the last padded with silence, the flags of a one-shot, or of a
	 * loop from LoopStartFrame (INDEX_NONE for none; a multiple of 28 otherwise).
	 */
	static void EncodeBlocks(const int16* Samples, int32 NumFrames, int32 LoopStartFrame, TArray<uint8>& OutBlocks);

	/**
	 * The whole conversion: NumFrames interleaved frames of NumChannels at SampleRate, made mono, resampled, the loop
	 * aligned, encoded. False for no samples.
	 */
	static bool Compress(const int16* Interleaved, int32 NumFrames, int32 NumChannels, int32 SampleRate,
		const FSpuAdpcmSettings& Settings, FSpuAdpcmCompressed& Out);
};
