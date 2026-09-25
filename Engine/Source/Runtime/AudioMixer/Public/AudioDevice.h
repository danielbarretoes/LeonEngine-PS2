#pragma once

#include "CoreMinimal.h"

/** Built-in UI / feedback cues. Prefers Content WAVs when present; procedural fallback. */
enum class EUISound : uint8
{
	Click = 0,
	Confirm = 1,
	Back = 2,
	Error = 3,
};

/**
 * UE-like audio subsystem (FAudioDevice / UGameplayStatics PlaySound lite), backed by miniaudio.
 * Safe no-op when Initialize fails or in headless silent mode. Sound paths are legacy content names resolved with
 * FPaths::ResolveLegacyContentPath: the device sits below Engine and plays files, not Engine's USoundWave assets
 * (FLegacyAssetLoader::LoadSoundWave reads a `.wav` into one; playing them comes with the gameplay sounds).
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

	/** UE PlaySound2D: fire-and-forget WAV / FLAC / MP3 / OGG. */
	void PlaySound2D(const TCHAR* AssetRelativePath, float VolumeMultiplier = 1.0f);

	/** UE PlaySoundAtLocation: spatialized one-shot. */
	void PlaySoundAtLocation(const TCHAR* AssetRelativePath, const FVector& Location, float VolumeMultiplier = 1.0f);

	/** UI cue: tries Content assets/Audio/UI/UI_*.wav, else a procedural tone. */
	void PlayUiSound(EUISound InSound, float VolumeMultiplier = 1.0f);

	/** Looping 2D music bed (dedicated slot, not the one-shot voice pool). Replaces any prior bed. */
	void PlayMusic(const TCHAR* AssetRelativePath, float VolumeMultiplier = 0.35f);
	void StopMusic();
	[[nodiscard]] bool IsMusicPlaying() const;

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
	bool bInitialized = false;
	bool bSilent = true;
	float MasterVolume = 1.0f;
};
