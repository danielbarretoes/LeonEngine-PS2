#include "AudioDevice.h"

#include "AudioOutput.h"
#include "HAL/PlatformTime.h"
#include "SoftwareAudioMixer.h"
#include "UiTone.h"

DEFINE_LOG_CATEGORY_STATIC(LogAudioMixer, Log, All);

// FAudioDevice on every platform (Docs/PLANS/ps2-preview.md V1): the sounds mix on the game thread
// (FSoftwareAudioMixer, 48 kHz stereo, the SPU2's rate) and each tick queues what the platform's output (FAudioOutput:
// the SPU2 through audsrv, or a desktop device) played since the last one, so the desktop hears what the PS2 plays.

namespace
{

	constexpr int32 OutputRate = 48000;
	/** The audio queued ahead of the output when the device starts: two frames at 30 fps. */
	constexpr double LeadSeconds = 2.0 / 30.0;
	/** The most a tick mixes: a hitch longer than this skips audio rather than queueing it. */
	constexpr double MaxTickSeconds = 0.1;

	FAudioOutputFactory GAudioOutputFactory = nullptr;

} // namespace

void SetAudioOutputFactory(FAudioOutputFactory Factory)
{
	GAudioOutputFactory = Factory;
}

struct FAudioDevice::FImpl
{
	/** A UI cue's samples (SetUiSound), or its procedural tone. */
	struct FUiSound
	{
		TArray<int16> Samples;
		int32 NumChannels = 1;
		int32 SampleRate = 0;
		/** The samples came from SetUiSound (HasUiSound). */
		bool bOwnSamples = false;
	};

	FSoftwareAudioMixer Mixer{OutputRate};
	/** Null when silent. */
	TUniquePtr<FAudioOutput> Output;
	TArray<int16> Chunk;
	FUiSound UiSounds[NumUISounds];
	int32 MusicVoice = INDEX_NONE;
	double LastTickSeconds = 0.0;
	/** Output frames owed but not yet mixed (the fraction a tick leaves). */
	double PendingFrames = 0.0;

	[[nodiscard]] static FSoundWavePCM View(const FUiSound& Sound)
	{
		FSoundWavePCM View;
		View.Samples = Sound.Samples.GetData();
		View.NumChannels = Sound.NumChannels;
		View.NumFrames = Sound.NumChannels > 0 ? Sound.Samples.Num() / Sound.NumChannels : 0;
		View.SampleRate = Sound.SampleRate;
		return View;
	}

	/** The procedural tone of a cue, as 16-bit samples. */
	static void MakeTone(EUISound InSound, FUiSound& Out)
	{
		TArray<float> Tone;
		int32 SampleRate = 0;
		BuildUiTone(InSound, Tone, SampleRate);
		Out.Samples.SetNumUninitialized(Tone.Num());
		for (int32 Index = 0; Index < Tone.Num(); ++Index)
		{
			Out.Samples[Index] = int16(FMath::Clamp(FMath::RoundToInt(Tone[Index] * 32767.0f), -32768, 32767));
		}
		Out.NumChannels = 1;
		Out.SampleRate = SampleRate;
		Out.bOwnSamples = false;
	}

	/** Mixes Frames stereo frames and queues them on the output. */
	void Stream(int32 Frames)
	{
		if (Frames <= 0 || !Output)
		{
			return;
		}
		Chunk.SetNumUninitialized(Frames * 2);
		Mixer.Mix(Chunk.GetData(), Frames);
		Output->Queue(Chunk.GetData(), Frames);
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
	for (int32 Index = 0; Index < NumUISounds; ++Index)
	{
		FImpl::MakeTone(EUISound(Index), Impl->UiSounds[Index]);
	}
	MasterVolume = 1.0f;
	Impl->Mixer.SetMasterVolume(MasterVolume);
	bInitialized = true;
	bSilent = true;
	if (bInSilent)
	{
		return true;
	}
	TUniquePtr<FAudioOutput> Output =
		GAudioOutputFactory != nullptr ? GAudioOutputFactory() : CreatePlatformAudioOutput();
	if (!Output || !Output->Start(OutputRate))
	{
		UE_LOG(LogAudioMixer, Warning, "Audio: no output; silent");
		return false;
	}
	Impl->Output = MoveTemp(Output);
	Impl->Stream(int32(LeadSeconds * OutputRate));
	Impl->LastTickSeconds = FPlatformTime::Seconds();
	bSilent = false;
	UE_LOG(LogAudioMixer, Log, "Audio: %d Hz stereo, mixed on the game thread", OutputRate);
	return true;
}

void FAudioDevice::Shutdown()
{
	Impl.Reset();
	bSilent = true;
	bInitialized = false;
}

void FAudioDevice::Tick()
{
	if (!Impl || !Impl->Output)
	{
		return;
	}
	// The output plays at the output rate: a tick queues what it played since the last one.
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = FMath::Min(Now - Impl->LastTickSeconds, MaxTickSeconds);
	Impl->LastTickSeconds = Now;
	Impl->PendingFrames += Elapsed * double(OutputRate);
	const int32 Frames = int32(Impl->PendingFrames);
	Impl->PendingFrames -= double(Frames);
	Impl->Stream(Frames);
}

void FAudioDevice::SetMasterVolume(float Volume01)
{
	MasterVolume = FMath::Clamp(Volume01, 0.0f, 1.0f);
	if (Impl)
	{
		Impl->Mixer.SetMasterVolume(MasterVolume);
	}
}

void FAudioDevice::SetListener(const FVector& Location, const FVector& Forward, const FVector& Up)
{
	if (Impl)
	{
		Impl->Mixer.SetListener(Location, Forward, Up);
	}
}

void FAudioDevice::PlaySound2D(const FSoundWavePCM& Sound, float VolumeMultiplier)
{
	if (Impl && Impl->Output)
	{
		(void)Impl->Mixer.Play(Sound, VolumeMultiplier, false);
	}
}

void FAudioDevice::PlaySoundAtLocation(const FSoundWavePCM& Sound, const FVector& Location, float VolumeMultiplier)
{
	if (Impl && Impl->Output)
	{
		(void)Impl->Mixer.Play(Sound, VolumeMultiplier, false, true, Location);
	}
}

void FAudioDevice::SetUiSound(EUISound InSound, const FSoundWavePCM& Sound)
{
	if (!Impl)
	{
		return;
	}
	FImpl::FUiSound& Cue = Impl->UiSounds[int32(InSound)];
	if (!Sound.IsValid())
	{
		FImpl::MakeTone(InSound, Cue);
		return;
	}
	Cue.Samples.SetNumUninitialized(Sound.NumFrames * Sound.NumChannels);
	FMemory::Memcpy(Cue.Samples.GetData(), Sound.Samples, SIZE_T(Cue.Samples.Num()) * sizeof(int16));
	Cue.NumChannels = Sound.NumChannels;
	Cue.SampleRate = Sound.SampleRate;
	Cue.bOwnSamples = true;
}

bool FAudioDevice::HasUiSound(EUISound InSound) const
{
	return Impl && Impl->UiSounds[int32(InSound)].bOwnSamples;
}

void FAudioDevice::PlayUiSound(EUISound InSound, float VolumeMultiplier)
{
	if (Impl && Impl->Output)
	{
		(void)Impl->Mixer.Play(FImpl::View(Impl->UiSounds[int32(InSound)]), VolumeMultiplier, false);
	}
}

void FAudioDevice::PlayMusic(const FSoundWavePCM& Sound, float VolumeMultiplier)
{
	if (Impl && Impl->Output)
	{
		StopMusic();
		Impl->MusicVoice = Impl->Mixer.Play(Sound, VolumeMultiplier, true);
	}
}

void FAudioDevice::StopMusic()
{
	if (Impl && Impl->MusicVoice != INDEX_NONE)
	{
		Impl->Mixer.Stop(Impl->MusicVoice);
		Impl->MusicVoice = INDEX_NONE;
	}
}

bool FAudioDevice::IsMusicPlaying() const
{
	return Impl && Impl->Mixer.IsPlaying(Impl->MusicVoice);
}
