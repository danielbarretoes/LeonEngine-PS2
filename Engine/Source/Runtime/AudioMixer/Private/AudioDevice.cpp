#include "AudioDevice.h"

#include "AudioHardware.h"
#include "HAL/PlatformTime.h"
#include "SpuVoiceVolume.h"
#include "Stats/Stats.h"

DEFINE_LOG_CATEGORY_STATIC(LogAudioMixer, Log, All);
DECLARE_CYCLE_STAT(TEXT("Audio Voices"), STAT_AudioVoices, STATGROUP_Audio);

// FAudioDevice on every platform (Docs/PLANS/ps2-shipping.md N19): the SPU2's model, decided here and carried out by
// the platform's FAudioHardware (audsrv's voices on the PS2, a CPU mix on the desktop), so both platforms keep the same
// buffers, give the same voices and send the same volumes.

namespace
{

	FAudioHardwareFactory GAudioHardwareFactory = nullptr;
	/** The next device serial (0 is never one). */
	uint32 GNextSerial = 1;

	/** Centimetres to the metres the attenuation is tuned for. */
	constexpr float MetresPerUnit = 0.01f;
	/** A voice stays taken for its sound's length and this much more: the hardware starts it a little after the tick.
	 */
	constexpr uint64 VoiceEndMarginMicroseconds = 4000;
	/** A voice the hardware refused (the SPU2 has not reached its end) is left alone this long. */
	constexpr uint64 RefusedVoiceMicroseconds = 10000;
	/** The voices a play tries before it is dropped. */
	constexpr int32 MaxPlayAttempts = 3;
	/** The UI cues' priority: above the default, so the reserved voices take them. */
	constexpr float UiPriority = 2.0f;
	constexpr int32 NumEffectVoices = FAudioDevice::NumVoices - FAudioDevice::NumMusicVoices;

} // namespace

void SetAudioHardwareFactory(FAudioHardwareFactory Factory)
{
	GAudioHardwareFactory = Factory;
}

struct FAudioDevice::FImpl
{
	/** A buffer slot: a sound resident in the hardware (SPU2 RAM), shared by its key. */
	struct FBuffer
	{
		FName Key;
		int32 RefCount = 0;
		int32 Bytes = 0;
		int32 NumFrames = 0;
		int32 LoopStartFrame = INDEX_NONE;
		int32 Pitch = 0;
		bool bResident = false;
	};

	/** What the device knows of a hardware voice. */
	struct FVoice
	{
		int32 Buffer = INDEX_NONE;
		/** The device clock's time the voice is free again (MAX_uint64 for a loop). */
		uint64 EndMicroseconds = 0;
		float Volume = 1.0f;
		bool bSpatialized = false;
		FVector Location = FVector::ZeroVector;
		/** Stopped before its end (StopMusic): silent, and busy until its end. */
		bool bMuted = false;
		/** The volume the hardware has. */
		FSpuVoiceVolume Sent;
	};

	/** A play queued for the next Tick. */
	struct FPlay
	{
		int32 Buffer = INDEX_NONE;
		float Volume = 1.0f;
		float Priority = DefaultPriority;
		FVector Location = FVector::ZeroVector;
		bool bSpatialized = false;
		bool bMusic = false;
	};

	/** Null when silent. */
	TUniquePtr<FAudioHardware> Hardware;
	FBuffer Buffers[MaxSoundBuffers];
	TMap<FName, int32> BufferByKey;
	/** The resident buffers in upload order: the SPU2 RAM is a stack (audsrv places each sample after the last). */
	TArray<int32> Stack;
	int32 RamUsed = 0;
	FVoice Voices[NumVoices];
	FPlay Pending[MaxPendingPlays];
	int32 NumPending = 0;
	int32 UiBuffers[NumUISounds] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
	int32 MusicVoice = INDEX_NONE;
	float MasterVolume = 1.0f;
	/** The master volume changed: every voice gets its volume again. */
	bool bVolumesDirty = false;
	FVector ListenerLocation = FVector::ZeroVector;
	FVector ListenerRight = FVector(0.0f, 1.0f, 0.0f);
	/** The device clock: microseconds since Initialize, as of the last Tick (integers: the EE has no fast doubles). */
	uint64 StartCycles = 0;
	uint64 Now = 0;

	[[nodiscard]] bool IsResident(int32 Buffer) const
	{
		return Buffer >= 0 && Buffer < MaxSoundBuffers && Buffers[Buffer].bResident;
	}

	[[nodiscard]] bool IsBusy(int32 Voice) const
	{
		return Voices[Voice].EndMicroseconds > Now;
	}

	/** Whether a busy voice or a queued play still reads the buffer. */
	[[nodiscard]] bool IsBufferInUse(int32 Buffer) const
	{
		for (int32 Voice = 0; Voice < NumVoices; ++Voice)
		{
			if (Voices[Voice].Buffer == Buffer && IsBusy(Voice))
			{
				return true;
			}
		}
		for (int32 Index = 0; Index < NumPending; ++Index)
		{
			if (Pending[Index].Buffer == Buffer)
			{
				return true;
			}
		}
		return false;
	}

	/** Frees the last uploaded buffer when nothing references or plays it; false when it cannot go. */
	bool FreeUnusedTop()
	{
		if (Stack.Num() == 0)
		{
			return false;
		}
		const int32 Top = Stack.Last();
		FBuffer& Buffer = Buffers[Top];
		if (Buffer.RefCount > 0 || IsBufferInUse(Top))
		{
			return false;
		}
		Hardware->FreeSound(Top);
		for (FVoice& Voice : Voices)
		{
			if (Voice.Buffer == Top)
			{
				Voice.Buffer = INDEX_NONE;
			}
		}
		BufferByKey.Remove(Buffer.Key);
		RamUsed -= Buffer.Bytes;
		Buffer = FBuffer();
		Stack.Pop();
		return true;
	}

	/** A spatialized sound's left and right gains from the listener (1 / distance past 1 m; panned across its right).
	 */
	void SpatialGains(const FVector& Location, float& OutLeft, float& OutRight) const
	{
		const FVector ToSound = Location - ListenerLocation;
		const float Distance = ToSound.Size() * MetresPerUnit;
		const float Attenuation = 1.0f / FMath::Max(Distance, 1.0f);
		// -1 on the left, +1 on the right: the near side stays full, the far side fades.
		const float Pan =
			Distance > 1.0e-4f ? FMath::Clamp(ToSound.GetSafeNormal() | ListenerRight, -1.0f, 1.0f) : 0.0f;
		OutLeft = Attenuation * FMath::Min(1.0f, 1.0f - Pan);
		OutRight = Attenuation * FMath::Min(1.0f, 1.0f + Pan);
	}

	[[nodiscard]] FSpuVoiceVolume ComputeVolume(float Volume, bool bSpatialized, const FVector& Location) const
	{
		const float Gain = FMath::Clamp(Volume, 0.0f, 1.0f) * MasterVolume;
		float Left = Gain;
		float Right = Gain;
		if (bSpatialized)
		{
			float SpatialLeft = 1.0f;
			float SpatialRight = 1.0f;
			SpatialGains(Location, SpatialLeft, SpatialRight);
			Left *= SpatialLeft;
			Right *= SpatialRight;
		}
		return FSpuVoiceVolume::FromGains(Left, Right);
	}

	/**
	 * A free effect voice for a sound of Priority: the one free the longest (so a voice that just ended, which the SPU2
	 * may not have reached the end of yet, is taken last). A default priority sound leaves the last NumReservedVoices
	 * free ones to higher ones. INDEX_NONE when there is none.
	 */
	[[nodiscard]] int32 AllocateVoice(float Priority) const
	{
		int32 NumFree = 0;
		int32 Best = INDEX_NONE;
		for (int32 Voice = 0; Voice < NumEffectVoices; ++Voice)
		{
			if (!IsBusy(Voice))
			{
				++NumFree;
				if (Best == INDEX_NONE || Voices[Voice].EndMicroseconds < Voices[Best].EndMicroseconds)
				{
					Best = Voice;
				}
			}
		}
		if (Best != INDEX_NONE && Priority <= DefaultPriority && NumFree <= NumReservedVoices)
		{
			return INDEX_NONE;
		}
		if (Best != INDEX_NONE && Priority < DefaultPriority && NumFree <= NumLowPriorityVoices)
		{
			return INDEX_NONE;
		}
		return Best;
	}

	[[nodiscard]] int32 AllocateMusicVoice() const
	{
		int32 Best = INDEX_NONE;
		for (int32 Voice = NumEffectVoices; Voice < NumVoices; ++Voice)
		{
			if (!IsBusy(Voice) && (Best == INDEX_NONE || Voices[Voice].EndMicroseconds < Voices[Best].EndMicroseconds))
			{
				Best = Voice;
			}
		}
		return Best;
	}

	void StartPlay(const FPlay& Play)
	{
		if (!IsResident(Play.Buffer))
		{
			return;
		}
		const FBuffer& Buffer = Buffers[Play.Buffer];
		const FSpuVoiceVolume Volume = ComputeVolume(Play.Volume, Play.bSpatialized, Play.Location);
		if (!Play.bMusic && Play.bSpatialized && Volume.GetLeftLevel() == 0 && Volume.GetRightLevel() == 0)
		{
			// Too far to be heard: no voice for it.
			return;
		}
		for (int32 Attempt = 0; Attempt < MaxPlayAttempts; ++Attempt)
		{
			const int32 VoiceIndex = Play.bMusic ? AllocateMusicVoice() : AllocateVoice(Play.Priority);
			if (VoiceIndex == INDEX_NONE)
			{
				UE_LOG(LogAudioMixer, Verbose, "Audio: no free voice for %s", *Buffer.Key.ToString());
				return;
			}
			FVoice& Voice = Voices[VoiceIndex];
			if (!Hardware->PlayVoice(VoiceIndex, Play.Buffer, Volume))
			{
				Voice.EndMicroseconds = Now + RefusedVoiceMicroseconds;
				continue;
			}
			Voice.Buffer = Play.Buffer;
			Voice.EndMicroseconds = Buffer.LoopStartFrame != INDEX_NONE
				? MAX_uint64
				: Now + FSpuAdpcm::GetPlayMicroseconds(Buffer.NumFrames, Buffer.Pitch) + VoiceEndMarginMicroseconds;
			Voice.Volume = Play.Volume;
			Voice.bSpatialized = Play.bSpatialized;
			Voice.Location = Play.Location;
			Voice.bMuted = false;
			Voice.Sent = Volume;
			if (Play.bMusic)
			{
				MusicVoice = VoiceIndex;
			}
			return;
		}
	}

	/** Sends the playing voices' volumes that changed (spatialized ones follow the listener). */
	void UpdateVolumes()
	{
		for (int32 VoiceIndex = 0; VoiceIndex < NumVoices; ++VoiceIndex)
		{
			FVoice& Voice = Voices[VoiceIndex];
			if (Voice.Buffer == INDEX_NONE || Voice.bMuted || !IsBusy(VoiceIndex) ||
				(!Voice.bSpatialized && !bVolumesDirty))
			{
				continue;
			}
			const FSpuVoiceVolume Volume = ComputeVolume(Voice.Volume, Voice.bSpatialized, Voice.Location);
			if (Volume != Voice.Sent)
			{
				Hardware->SetVoiceVolume(VoiceIndex, Volume);
				Voice.Sent = Volume;
			}
		}
		bVolumesDirty = false;
	}

	void Queue(const FPlay& Play)
	{
		if (NumPending >= MaxPendingPlays)
		{
			UE_LOG(LogAudioMixer, Verbose, "Audio: %d plays queued this frame; %s dropped", MaxPendingPlays,
				*Buffers[Play.Buffer].Key.ToString());
			return;
		}
		Pending[NumPending++] = Play;
	}
};

FAudioDevice::FAudioDevice() = default;

FAudioDevice::~FAudioDevice()
{
	Shutdown();
}

bool FAudioDevice::Initialize(bool bInSilent)
{
	Shutdown();
	Impl = MakeUnique<FImpl>();
	MasterVolume = 1.0f;
	Serial = GNextSerial++;
	bInitialized = true;
	bSilent = true;
	if (bInSilent)
	{
		return true;
	}
	TUniquePtr<FAudioHardware> Hardware =
		GAudioHardwareFactory != nullptr ? GAudioHardwareFactory() : CreatePlatformAudioHardware();
	if (!Hardware || !Hardware->Start())
	{
		UE_LOG(LogAudioMixer, Warning, "Audio: no sound hardware; silent");
		return false;
	}
	Impl->Hardware = MoveTemp(Hardware);
	Impl->StartCycles = FPlatformTime::Cycles64();
	bSilent = false;
	UE_LOG(LogAudioMixer, Log, "Audio: %d voices (%d for music), %d KB of SPU2 RAM for sounds", NumVoices,
		NumMusicVoices, (FSpuAdpcm::SoundRamBytes + 1023) / 1024);
	return true;
}

void FAudioDevice::Shutdown()
{
	Impl.Reset();
	if (bInitialized)
	{
		Serial = GNextSerial++;
	}
	bSilent = true;
	bInitialized = false;
}

void FAudioDevice::Tick()
{
	if (!Impl || !Impl->Hardware)
	{
		return;
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_AudioVoices);
		Impl->Now = FPlatformTime::CyclesToMicroseconds(FPlatformTime::Cycles64() - Impl->StartCycles);
		for (int32 Index = 0; Index < Impl->NumPending; ++Index)
		{
			Impl->StartPlay(Impl->Pending[Index]);
		}
		Impl->NumPending = 0;
		Impl->UpdateVolumes();
	}
	Impl->Hardware->Tick();
}

void FAudioDevice::SetMasterVolume(float Volume01)
{
	MasterVolume = FMath::Clamp(Volume01, 0.0f, 1.0f);
	if (Impl)
	{
		Impl->MasterVolume = MasterVolume;
		Impl->bVolumesDirty = true;
	}
}

void FAudioDevice::SetListener(const FVector& Location, const FVector& Forward, const FVector& Up)
{
	if (!Impl)
	{
		return;
	}
	Impl->ListenerLocation = Location;
	// The world is left-handed, Z up: the right side is Up ^ Forward.
	const FVector Right = Up ^ Forward;
	if (Right.SizeSquared() > 1.0e-8f)
	{
		Impl->ListenerRight = Right.GetSafeNormal();
	}
}

int32 FAudioDevice::AcquireSoundBuffer(FName Key, const FSpuAdpcmSound& Sound)
{
	if (!Impl || !Impl->Hardware || !Sound.IsValid())
	{
		return INDEX_NONE;
	}
	if (const int32* Found = Impl->BufferByKey.Find(Key))
	{
		++Impl->Buffers[*Found].RefCount;
		return *Found;
	}
	const int32 Bytes = Sound.GetNumBytes();
	// Room: the unreferenced buffers on top of the stack go first (audsrv frees only from the end).
	while (Impl->RamUsed + Bytes > FSpuAdpcm::SoundRamBytes && Impl->FreeUnusedTop())
	{
	}
	int32 Slot = INDEX_NONE;
	while (Slot == INDEX_NONE)
	{
		for (int32 Index = 0; Index < MaxSoundBuffers && Slot == INDEX_NONE; ++Index)
		{
			Slot = Impl->Buffers[Index].bResident ? INDEX_NONE : Index;
		}
		if (Slot == INDEX_NONE && !Impl->FreeUnusedTop())
		{
			break;
		}
	}
	if (Impl->RamUsed + Bytes > FSpuAdpcm::SoundRamBytes || Slot == INDEX_NONE)
	{
		UE_LOG(LogAudioMixer, Error,
			"Audio: %s needs %d bytes of SPU2 RAM; %d of %d are taken by %d sound(s) still in use (Budgets.md)",
			*Key.ToString(), Bytes, Impl->RamUsed, FSpuAdpcm::SoundRamBytes, Impl->Stack.Num());
		return INDEX_NONE;
	}
	const int32 Pitch = FSpuAdpcm::GetPitch(Sound.SampleRate);
	if (!Impl->Hardware->UploadSound(Slot, Sound, Pitch))
	{
		return INDEX_NONE;
	}
	FImpl::FBuffer& Buffer = Impl->Buffers[Slot];
	Buffer.Key = Key;
	Buffer.RefCount = 1;
	Buffer.Bytes = Bytes;
	Buffer.NumFrames = Sound.GetNumFrames();
	Buffer.LoopStartFrame = Sound.LoopStartFrame;
	Buffer.Pitch = Pitch;
	Buffer.bResident = true;
	Impl->BufferByKey.Add(Key, Slot);
	Impl->Stack.Add(Slot);
	Impl->RamUsed += Bytes;
	UE_LOG(LogAudioMixer, Log, "Audio: %s in SPU2 RAM, %d bytes at %d Hz%s (%d of %d KB)", *Key.ToString(), Bytes,
		Sound.SampleRate, Sound.IsLooping() ? TEXT(", looping") : TEXT(""), (Impl->RamUsed + 1023) / 1024,
		(FSpuAdpcm::SoundRamBytes + 1023) / 1024);
	return Slot;
}

void FAudioDevice::ReleaseSoundBuffer(int32 Buffer)
{
	if (Impl && Impl->IsResident(Buffer) && Impl->Buffers[Buffer].RefCount > 0)
	{
		--Impl->Buffers[Buffer].RefCount;
	}
}

int32 FAudioDevice::GetSoundRamUsed() const
{
	return Impl ? Impl->RamUsed : 0;
}

int32 FAudioDevice::GetNumSoundBuffers() const
{
	return Impl ? Impl->Stack.Num() : 0;
}

void FAudioDevice::PlaySound2D(int32 Buffer, float VolumeMultiplier, float Priority)
{
	if (Impl && Impl->Hardware && Impl->IsResident(Buffer))
	{
		FImpl::FPlay Play;
		Play.Buffer = Buffer;
		Play.Volume = VolumeMultiplier;
		Play.Priority = Priority;
		Impl->Queue(Play);
	}
}

void FAudioDevice::PlaySoundAtLocation(int32 Buffer, const FVector& Location, float VolumeMultiplier, float Priority)
{
	if (Impl && Impl->Hardware && Impl->IsResident(Buffer))
	{
		FImpl::FPlay Play;
		Play.Buffer = Buffer;
		Play.Volume = VolumeMultiplier;
		Play.Priority = Priority;
		Play.Location = Location;
		Play.bSpatialized = true;
		Impl->Queue(Play);
	}
}

void FAudioDevice::SetUiSound(EUISound InSound, int32 Buffer)
{
	if (!Impl)
	{
		return;
	}
	int32& Cue = Impl->UiBuffers[int32(InSound)];
	ReleaseSoundBuffer(Cue);
	Cue = Impl->IsResident(Buffer) ? Buffer : INDEX_NONE;
	if (Cue != INDEX_NONE)
	{
		++Impl->Buffers[Cue].RefCount;
	}
}

bool FAudioDevice::HasUiSound(EUISound InSound) const
{
	return Impl && Impl->UiBuffers[int32(InSound)] != INDEX_NONE;
}

void FAudioDevice::PlayUiSound(EUISound InSound, float VolumeMultiplier)
{
	if (Impl)
	{
		PlaySound2D(Impl->UiBuffers[int32(InSound)], VolumeMultiplier, UiPriority);
	}
}

void FAudioDevice::PlayMusic(int32 Buffer, float VolumeMultiplier)
{
	if (Impl && Impl->Hardware && Impl->IsResident(Buffer))
	{
		StopMusic();
		FImpl::FPlay Play;
		Play.Buffer = Buffer;
		Play.Volume = VolumeMultiplier;
		Play.bMusic = true;
		Impl->Queue(Play);
	}
}

void FAudioDevice::StopMusic()
{
	if (!Impl || !Impl->Hardware)
	{
		return;
	}
	// A queued bed never starts.
	int32 Kept = 0;
	for (int32 Index = 0; Index < Impl->NumPending; ++Index)
	{
		if (!Impl->Pending[Index].bMusic)
		{
			Impl->Pending[Kept++] = Impl->Pending[Index];
		}
	}
	Impl->NumPending = Kept;
	const int32 VoiceIndex = Impl->MusicVoice;
	Impl->MusicVoice = INDEX_NONE;
	if (VoiceIndex == INDEX_NONE || !Impl->IsBusy(VoiceIndex))
	{
		return;
	}
	Impl->Hardware->StopVoice(VoiceIndex);
	// The PS2's voice plays on muted until it passes its sound's end: busy until then.
	FImpl::FVoice& Voice = Impl->Voices[VoiceIndex];
	const FImpl::FBuffer& Buffer = Impl->Buffers[Voice.Buffer];
	Voice.bMuted = true;
	Voice.EndMicroseconds = FMath::Min(Voice.EndMicroseconds,
		Impl->Now + FSpuAdpcm::GetPlayMicroseconds(Buffer.NumFrames, Buffer.Pitch) + VoiceEndMarginMicroseconds);
}

bool FAudioDevice::IsMusicPlaying() const
{
	if (!Impl)
	{
		return false;
	}
	for (int32 Index = 0; Index < Impl->NumPending; ++Index)
	{
		if (Impl->Pending[Index].bMusic)
		{
			return true;
		}
	}
	return Impl->MusicVoice != INDEX_NONE && Impl->IsBusy(Impl->MusicVoice);
}

int32 FAudioDevice::GetNumPlayingVoices() const
{
	int32 Count = 0;
	for (int32 Voice = 0; Impl && Voice < NumVoices; ++Voice)
	{
		Count += Impl->IsBusy(Voice) ? 1 : 0;
	}
	return Count;
}
