#pragma once

#include "CoreMinimal.h"
#include "SpuAdpcm.h"

/** Built-in UI / feedback cues: the sound wave the engine config gives each (silent without one). */
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
 * UE-like audio subsystem (FAudioDevice / UGameplayStatics PlaySound lite) over the SPU2's model on every platform
 * (Docs/PLANS/ps2-shipping.md N19): sounds are SPU2 ADPCM buffers (FSpuAdpcmSound, what Engine's USoundWave holds;
 * AudioMixer sits below Engine and never sees assets), resident in the SPU2's 2 MB, played on 24 hardware voices whose
 * volume and pan the device computes from the listener each frame. The PS2's hardware is the SPU2 through audsrv; the
 * desktop's decodes the same buffers and mixes the voices on the CPU with the SPU2's pitch and audsrv's volume steps,
 * so both platforms play the same sounds, keep the same buffers and give the same voices.
 *
 * Buffers are shared by name and counted (AcquireSoundBuffer / ReleaseSoundBuffer); an unreferenced buffer stays
 * resident until a new one needs its room (the SPU2 RAM is a stack, as audsrv allocates it: only the last buffers can
 * be freed). Plays are queued and started by Tick. Two voices are kept for the music; the last NumReservedVoices of the
 * others go only to sounds above the default priority, and a sound below it (steps, impacts) leaves the last
 * NumLowPriorityVoices free for the rest. audsrv cannot stop a voice, so a sound that finds no free voice is dropped
 * rather than stealing one, on every platform; a spatialized sound too far to be heard (both sides at audsrv's level 0)
 * takes no voice (UE: a sound beyond its attenuation is not played).
 *
 * Safe no-op when Initialize fails or in headless silent mode: no buffer is made and the Play* calls do nothing.
 */
class AUDIOMIXER_API FAudioDevice
{
public:
	/** The buffers the device can keep at once (the SPU2 RAM decides how many fit). */
	static constexpr int32 MaxSoundBuffers = 256;
	/** The SPU2's hardware voices (core 1, audsrv's). */
	static constexpr int32 NumVoices = 24;
	/** The voices kept for the music (the last ones). */
	static constexpr int32 NumMusicVoices = 2;
	/** The last free effect voices, kept for sounds above DefaultPriority. */
	static constexpr int32 NumReservedVoices = 4;
	/** The last free effect voices a sound below DefaultPriority leaves (the reserved ones included). */
	static constexpr int32 NumLowPriorityVoices = 10;
	/** The priority of a sound that sets none (UE: USoundBase::Priority's default). */
	static constexpr float DefaultPriority = 1.0f;
	/** The plays a frame may queue; more are dropped. */
	static constexpr int32 MaxPendingPlays = 32;

	FAudioDevice();
	~FAudioDevice();

	FAudioDevice(const FAudioDevice&) = delete;
	FAudioDevice& operator=(const FAudioDevice&) = delete;

	/**
	 * bInSilent skips opening the hardware (dedicated / CI). Returns false only on a hard failure when not silent (the
	 * engine still runs; the Play* calls become no-ops).
	 */
	bool Initialize(bool bInSilent = false);
	void Shutdown();

	/**
	 * Once per frame, after the world ticked (UE: FAudioDevice::Update): frees the voices whose sound ended, starts the
	 * queued plays, sends the playing voices' new volumes, and lets the hardware run (the desktop mixes).
	 */
	void Tick();

	[[nodiscard]] bool IsInitialized() const
	{
		return bInitialized;
	}
	[[nodiscard]] bool IsSilent() const
	{
		return bSilent;
	}

	/** Changes with every Initialize and Shutdown: a buffer index is valid for the serial it was acquired under. */
	[[nodiscard]] uint32 GetSerial() const
	{
		return Serial;
	}

	void SetMasterVolume(float Volume01);
	[[nodiscard]] float GetMasterVolume() const
	{
		return MasterVolume;
	}

	/** Listener for 3D sounds (UE SetListener); called by the engine after the camera update. */
	void SetListener(const FVector& Location, const FVector& Forward, const FVector& Up);

	/**
	 * The buffer of the sound Key names (its object path), made from Sound when it is not resident (UE: the device's
	 * wave buffers, FAudioDevice::Precache): uploaded to SPU2 RAM on the PS2, decoded on the desktop. Adds a reference.
	 * INDEX_NONE when silent, for an invalid sound, or when the sound does not fit the SPU2 RAM left (an error that
	 * names the sound and the RAM used).
	 */
	int32 AcquireSoundBuffer(FName Key, const FSpuAdpcmSound& Sound);

	/** Drops a reference AcquireSoundBuffer gave; the buffer stays resident until its room is needed. */
	void ReleaseSoundBuffer(int32 Buffer);

	/** The SPU2 RAM the resident buffers take, in bytes (of FSpuAdpcm::SoundRamBytes). */
	[[nodiscard]] int32 GetSoundRamUsed() const;

	/** The resident buffers, referenced or not. */
	[[nodiscard]] int32 GetNumSoundBuffers() const;

	/** UE PlaySound2D: a fire-and-forget, non-spatialized play of a buffer (started by the next Tick). */
	void PlaySound2D(int32 Buffer, float VolumeMultiplier = 1.0f, float Priority = DefaultPriority);

	/** UE PlaySoundAtLocation: a spatialized play of a buffer at Location (started by the next Tick). */
	void PlaySoundAtLocation(
		int32 Buffer, const FVector& Location, float VolumeMultiplier = 1.0f, float Priority = DefaultPriority);

	/** The buffer a UI cue plays (UEngine's UI*SoundName); INDEX_NONE silences it. The device keeps a reference. */
	void SetUiSound(EUISound InSound, int32 Buffer);

	/** True when the cue has a buffer (SetUiSound). */
	[[nodiscard]] bool HasUiSound(EUISound InSound) const;

	/** UI cue: the buffer SetUiSound gave it, 2D, above the default priority; nothing without one. */
	void PlayUiSound(EUISound InSound, float VolumeMultiplier = 1.0f);

	/**
	 * A 2D music bed on the music voices (a looping buffer loops; a one-shot plays once). Replaces any prior bed. Music
	 * is a resident buffer like the rest; streaming a long track needs an IOP module of its own (Docs/ARCHITECTURE.md,
	 * Audio).
	 */
	void PlayMusic(int32 Buffer, float VolumeMultiplier = 0.35f);
	void StopMusic();
	[[nodiscard]] bool IsMusicPlaying() const;

	/** The voices busy with a sound, music included. */
	[[nodiscard]] int32 GetNumPlayingVoices() const;

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
	bool bInitialized = false;
	bool bSilent = true;
	float MasterVolume = 1.0f;
	uint32 Serial = 0;
};
