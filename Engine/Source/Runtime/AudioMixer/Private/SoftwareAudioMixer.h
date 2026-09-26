#pragma once

#include "AudioDevice.h"
#include "CoreMinimal.h"

/**
 * A mixer on the CPU (Leon; UE's FMixerDevice mixes its sources the same way before the platform's output): voices of
 * copied PCM16 samples, resampled linearly to the output rate and summed into interleaved stereo 16-bit frames. The
 * PS2's audio device mixes with it and streams the result to the SPU2 (audsrv); it builds on every platform, so its
 * tests run on the host.
 *
 * A 2D voice plays its channels at its volume (a mono one on both sides). A spatialized one is heard as the desktop's
 * miniaudio hears it with its defaults: the gain falls as 1 / distance in metres past 1 m, and it pans across the
 * listener's right, full in the ear it is on and fading in the other.
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

	/** The listener of the spatialized voices (world space, centimetres). */
	void SetListener(const FVector& Location, const FVector& Forward, const FVector& Up);
	void SetMasterVolume(float Volume01);

	/**
	 * Starts a voice over a copy of Sound's samples; a spatialized one plays at Location. Returns its index, or
	 * INDEX_NONE when the sound is invalid or every voice is busy.
	 */
	int32 Play(const FSoundWavePCM& Sound, float Volume, bool bLooping, bool bSpatialized = false,
		const FVector& Location = FVector::ZeroVector);
	void Stop(int32 Voice);
	[[nodiscard]] bool IsPlaying(int32 Voice) const;
	[[nodiscard]] int32 GetNumPlaying() const;

	/** Mixes NumFrames stereo frames into Out (left, right, ...), advancing the voices; a one-shot that ends stops. */
	void Mix(int16* Out, int32 NumFrames);

private:
	struct FVoice
	{
		TArray<int16> Samples;
		int32 NumFrames = 0;
		int32 NumChannels = 0;
		/** Source frames per output frame. */
		double Step = 1.0;
		double Position = 0.0;
		float Volume = 1.0f;
		bool bLooping = false;
		bool bSpatialized = false;
		FVector Location = FVector::ZeroVector;
		bool bPlaying = false;
	};

	/** A spatialized voice's left and right gains from the listener. */
	void SpatialGains(const FVector& Location, float& OutLeft, float& OutRight) const;

	int32 OutputRate = 48000;
	float MasterVolume = 1.0f;
	FVector ListenerLocation = FVector::ZeroVector;
	FVector ListenerRight = FVector(0.0f, 1.0f, 0.0f);
	FVoice Voices[MaxVoices];
	TArray<float> MixBuffer;
};
