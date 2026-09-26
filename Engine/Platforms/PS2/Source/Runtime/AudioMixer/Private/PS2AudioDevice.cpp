#include "AudioDevice.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "SoftwareAudioMixer.h"
#include "UiTone.h"

#include <audsrv.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <sifrpc.h>

DEFINE_LOG_CATEGORY_STATIC(LogAudioMixer, Log, All);

// The PS2's FAudioDevice (Docs/PLANS/ps2-engine.md E5): the sounds mix on the EE (FSoftwareAudioMixer, 48 kHz stereo,
// the SPU2's rate) and stream to the SPU2 through audsrv, the IOP module the build copies beside the ELF
// (RUNTIME_DEPENDENCIES: $PS2SDK/iop/irx/audsrv.irx) over the ROM's LIBSD.

namespace
{

	constexpr int32 OutputRate = 48000;
	/** The audio queued ahead of the SPU2 when the device starts: two frames at 30 fps. */
	constexpr double LeadSeconds = 2.0 / 30.0;
	/** The most a frame mixes: a hitch longer than this skips audio rather than queueing it. */
	constexpr double MaxTickSeconds = 0.1;

	/** Loads LIBSD from the ROM and audsrv.irx from the ELF's folder into the IOP, then starts audsrv. */
	bool StartAudsrv()
	{
		SifInitRpc(0);
		// The ROM's LOADFILE cannot load a module from EE memory without this patch (older consoles).
		sbv_patch_enable_lmb();
		if (SifLoadModule("rom0:LIBSD", 0, nullptr) < 0)
		{
			UE_LOG(LogAudioMixer, Warning, "PS2 audio: rom0:LIBSD did not load");
			return false;
		}
		const FString IrxPath = FString(FPlatformProcess::BaseDir()) + "audsrv.irx";
		TArray<uint8> Irx;
		if (!FFileHelper::LoadFileToArray(Irx, *IrxPath))
		{
			UE_LOG(LogAudioMixer, Warning, "PS2 audio: no '%s' (the build copies it from $PS2SDK/iop/irx)", *IrxPath);
			return false;
		}
		// The SIF DMA reads the module from EE memory: 64-byte aligned.
		void* Aligned = FMemory::Malloc(SIZE_T(Irx.Num()), 64);
		FMemory::Memcpy(Aligned, Irx.GetData(), SIZE_T(Irx.Num()));
		int ModuleResult = 0;
		const int Module = SifExecModuleBuffer(Aligned, u32(Irx.Num()), 0, nullptr, &ModuleResult);
		FMemory::Free(Aligned);
		if (Module < 0 || ModuleResult < 0)
		{
			UE_LOG(LogAudioMixer, Warning, "PS2 audio: audsrv.irx did not start (%d, %d)", Module, ModuleResult);
			return false;
		}
		if (audsrv_init() != 0)
		{
			UE_LOG(LogAudioMixer, Warning, "PS2 audio: audsrv_init failed (%d)", audsrv_get_error());
			return false;
		}
		audsrv_fmt_t Format;
		Format.freq = OutputRate;
		Format.bits = 16;
		Format.channels = 2;
		if (audsrv_set_format(&Format) != 0)
		{
			UE_LOG(LogAudioMixer, Warning, "PS2 audio: audsrv_set_format failed (%d)", audsrv_get_error());
			audsrv_quit();
			return false;
		}
		audsrv_set_volume(MAX_VOLUME);
		return true;
	}

} // namespace

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
	TArray<int16> Chunk;
	FUiSound UiSounds[NumUISounds];
	int32 MusicVoice = INDEX_NONE;
	double LastTickSeconds = 0.0;
	/** Output frames owed to the SPU2 but not yet mixed (the fraction a tick leaves). */
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

	/** Mixes Frames stereo frames and queues them on the SPU2. */
	void Stream(int32 Frames)
	{
		if (Frames <= 0)
		{
			return;
		}
		Chunk.SetNumUninitialized(Frames * 2);
		Mixer.Mix(Chunk.GetData(), Frames);
		// audsrv queues what its ring holds and drops the rest (a clock that drifts ahead loses a little audio).
		(void)audsrv_play_audio(reinterpret_cast<const char*>(Chunk.GetData()), Frames * 2 * int32(sizeof(int16)));
	}
};

FAudioDevice::FAudioDevice() = default;

FAudioDevice::~FAudioDevice()
{
	Shutdown();
}

bool FAudioDevice::Initialize(bool bInSilent)
{
	bInitialized = true;
	bSilent = true;
	if (bInSilent)
	{
		return true;
	}
	if (!StartAudsrv())
	{
		UE_LOG(LogAudioMixer, Warning, "PS2 audio: silent");
		return false;
	}
	Impl = MakeUnique<FImpl>();
	for (int32 Index = 0; Index < NumUISounds; ++Index)
	{
		FImpl::MakeTone(EUISound(Index), Impl->UiSounds[Index]);
	}
	Impl->Mixer.SetMasterVolume(MasterVolume);
	Impl->Stream(int32(LeadSeconds * OutputRate));
	Impl->LastTickSeconds = FPlatformTime::Seconds();
	bSilent = false;
	UE_LOG(LogAudioMixer, Log, "PS2 audio: audsrv, %d Hz stereo mixed on the EE", OutputRate);
	return true;
}

void FAudioDevice::Shutdown()
{
	if (Impl)
	{
		audsrv_stop_audio();
		audsrv_quit();
		Impl.Reset();
	}
	bSilent = true;
	bInitialized = false;
}

void FAudioDevice::Tick()
{
	if (!Impl)
	{
		return;
	}
	// The SPU2 plays at the output rate: a frame queues what it played since the last one.
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
	if (Impl)
	{
		(void)Impl->Mixer.Play(Sound, VolumeMultiplier, false);
	}
}

void FAudioDevice::PlaySoundAtLocation(const FSoundWavePCM& Sound, const FVector& Location, float VolumeMultiplier)
{
	if (Impl)
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
	if (Impl)
	{
		(void)Impl->Mixer.Play(FImpl::View(Impl->UiSounds[int32(InSound)]), VolumeMultiplier, false);
	}
}

void FAudioDevice::PlayMusic(const FSoundWavePCM& Sound, float VolumeMultiplier)
{
	if (Impl)
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
