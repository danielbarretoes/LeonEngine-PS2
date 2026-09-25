#pragma once

#include "CoreMinimal.h"

/** Built-in UI / feedback cues: the sound wave the engine config gives each, else a procedural tone. */
enum class EUISound : uint8
{
	Click = 0,
	Confirm = 1,
	Back = 2,
	Error = 3,
};

/** Number of EUISound cues. */
inline constexpr int32 NumUISounds = 4;

/**
 * Interleaved 16-bit PCM samples: what a sound wave asset holds (Engine's USoundWave; AudioMixer sits below Engine and
 * never sees assets). The device copies the samples when it starts a sound, so they only need to live for the call.
 */
struct FSoundWavePCM
{
	const int16* Samples = nullptr;
	/** Samples per channel. */
	int32 NumFrames = 0;
	int32 NumChannels = 0;
	int32 SampleRate = 0;

	[[nodiscard]] bool IsValid() const
	{
		return Samples != nullptr && NumFrames > 0 && NumChannels > 0 && SampleRate > 0;
	}
};

/**
 * UE-like audio subsystem (FAudioDevice / UGameplayStatics PlaySound lite), backed by miniaudio.
 * Safe no-op when Initialize fails or in headless silent mode. It plays PCM16 samples from memory (FSoundWavePCM, what
 * Engine's USoundWave assets hold: UGameplayStatics::PlaySound2D / PlaySoundAtLocation), and the UI cues.
 */
class AUDIOMIXER_API FAudioDevice
{
public:
	FAudioDevice();
	~FAudioDevice();

	FAudioDevice(const FAudioDevice&) = delete;
	FAudioDevice& operator=(const FAudioDevice&) = delete;

	/**
	 * bInSilent skips opening the device (dedicated / CI). Returns false only on a hard failure when not silent (the
	 * engine still runs; the Play* calls become no-ops).
	 */
	bool Initialize(bool bInSilent = false);
	void Shutdown();
	/** Reaps finished one-shots (called once per frame by the engine). */
	void Tick();
	[[nodiscard]] bool IsInitialized() const
	{
		return bInitialized;
	}
	[[nodiscard]] bool IsSilent() const
	{
		return bSilent;
	}

	void SetMasterVolume(float Volume01);
	[[nodiscard]] float GetMasterVolume() const
	{
		return MasterVolume;
	}

	/** Listener for 3D sounds (UE SetListener); called by the engine after the camera update. */
	void SetListener(const FVector& Location, const FVector& Forward, const FVector& Up);

	/** UE PlaySound2D: a fire-and-forget, non-spatialized one-shot of the samples (copied). */
	void PlaySound2D(const FSoundWavePCM& Sound, float VolumeMultiplier = 1.0f);

	/** UE PlaySoundAtLocation: a spatialized one-shot of the samples (copied). */
	void PlaySoundAtLocation(const FSoundWavePCM& Sound, const FVector& Location, float VolumeMultiplier = 1.0f);

	/**
	 * The samples a UI cue plays (copied), from the engine config (UEngine's UI*SoundName); invalid samples bring the
	 * procedural tone back.
	 */
	void SetUiSound(EUISound InSound, const FSoundWavePCM& Sound);

	/** True when the cue has samples of its own (SetUiSound), false when it plays its procedural tone. */
	[[nodiscard]] bool HasUiSound(EUISound InSound) const;

	/** UI cue: the samples SetUiSound gave it, else a procedural tone. */
	void PlayUiSound(EUISound InSound, float VolumeMultiplier = 1.0f);

	/** Looping 2D music bed (dedicated slot, not the one-shot voice pool). Replaces any prior bed. */
	void PlayMusic(const FSoundWavePCM& Sound, float VolumeMultiplier = 0.35f);
	void StopMusic();
	[[nodiscard]] bool IsMusicPlaying() const;

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
	bool bInitialized = false;
	bool bSilent = true;
	float MasterVolume = 1.0f;
};
