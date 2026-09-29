#pragma once

#include "CoreMinimal.h"

/**
 * The desktop's stand-in for the SPU2's voices (Leon; UE's FMixerDevice mixes its sources the same way before the
 * platform's output): 24 voices over mono PCM16 samples (the decoded ADPCM buffers, not copied), each stepping through
 * its samples at the SPU2's pitch (4096: one source frame per 48 kHz frame; linear between frames where the SPU2
 * interpolates with its Gaussian table) at the left and right gains FAudioDevice sent (audsrv's levels), summed into
 * interleaved stereo 16-bit frames. A looping voice returns to its loop start, a one-shot stops at its end.
 */
class FSoftwareAudioMixer
{
public:
	static constexpr int32 MaxVoices = 24;

	explicit FSoftwareAudioMixer(int32 InOutputRate);

	[[nodiscard]] int32 GetOutputRate() const
	{
		return OutputRate;
	}

	/**
	 * Starts Voice over NumFrames Samples (they must outlive the voice), from their start, looping from LoopStartFrame
	 * (INDEX_NONE: a one-shot), Pitch / 4096 source frames per 48 kHz frame (FSpuAdpcm::GetPitch).
	 */
	void Play(int32 Voice, const int16* Samples, int32 NumFrames, int32 LoopStartFrame, int32 Pitch, float GainLeft,
		float GainRight);
	void SetGains(int32 Voice, float GainLeft, float GainRight);
	void Stop(int32 Voice);
	[[nodiscard]] bool IsPlaying(int32 Voice) const;
	[[nodiscard]] int32 GetNumPlaying() const;

	/** Mixes NumFrames stereo frames into Out (left, right, ...), advancing the voices. */
	void Mix(int16* Out, int32 NumFrames);

private:
	/**
	 * The play position and the step are fixed point, in 1 / 2^FractionBits of a source frame: the step is the pitch's
	 * 4.12 exactly (at 48 kHz, Pitch << 4), so a voice keeps the SPU2's time to the frame.
	 */
	static constexpr int32 FractionBits = 16;
	static constexpr uint64 FractionMask = (uint64(1) << FractionBits) - 1;

	struct FVoice
	{
		const int16* Samples = nullptr;
		int32 NumFrames = 0;
		int32 LoopStartFrame = INDEX_NONE;
		uint64 Step = uint64(1) << FractionBits;
		uint64 Position = 0;
		float GainLeft = 1.0f;
		float GainRight = 1.0f;
		bool bPlaying = false;
	};

	int32 OutputRate = 48000;
	FVoice Voices[MaxVoices];
	TArray<float> MixBuffer;
};
