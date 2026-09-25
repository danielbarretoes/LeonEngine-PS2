#include "AudioDevice.h"

#include <miniaudio.h>

DEFINE_LOG_CATEGORY_STATIC(LogAudioMixer, Log, All);

namespace
{

	[[nodiscard]] float Clamp01(float V)
	{
		return FMath::Clamp(V, 0.0f, 1.0f);
	}

	/**
	 * miniaudio works in a right-handed, Y-up space and its distance attenuation is tuned for metres; the engine world
	 * is left-handed, Z up, in centimetres. At this boundary Y and Z swap (which keeps the physical scene, so left and
	 * right stay where they are) and locations scale by 0.01 (directions only swap).
	 */
	constexpr float AudioMetresPerUnit = 0.01f;

	[[nodiscard]] FVector ToAudioDirection(const FVector& WorldDirection)
	{
		return FVector(WorldDirection.X, WorldDirection.Z, WorldDirection.Y);
	}

	[[nodiscard]] FVector ToAudioMetres(const FVector& WorldLocation)
	{
		return ToAudioDirection(WorldLocation) * AudioMetresPerUnit;
	}

	void BuildUiTone(EUISound InSound, TArray<float>& OutSamples, int32& OutSampleRate)
	{
		OutSampleRate = 44100;
		float Freq = 660.0f;
		float Duration = 0.045f;
		float Amp = 0.22f;
		switch (InSound)
		{
			case EUISound::Click:
				Freq = 880.0f;
				Duration = 0.035f;
				Amp = 0.18f;
				break;
			case EUISound::Confirm:
				Freq = 520.0f;
				Duration = 0.08f;
				Amp = 0.22f;
				break;
			case EUISound::Back:
				Freq = 320.0f;
				Duration = 0.05f;
				Amp = 0.16f;
				break;
			case EUISound::Error:
				Freq = 180.0f;
				Duration = 0.12f;
				Amp = 0.2f;
				break;
		}
		const int32 N = FMath::Max(1, static_cast<int32>(Duration * static_cast<float>(OutSampleRate)));
		OutSamples.SetNum(N);
		constexpr float Pi = 3.14159265f;
		for (int32 I = 0; I < N; ++I)
		{
			const float T = static_cast<float>(I) / static_cast<float>(OutSampleRate);
			const float Env = 1.0f - (static_cast<float>(I) / static_cast<float>(N));
			float Sample = FMath::Sin(2.0f * Pi * Freq * T) * Amp * Env;
			if (InSound == EUISound::Confirm && T > 0.04f)
			{
				Sample += FMath::Sin(2.0f * Pi * 780.0f * T) * Amp * 0.55f * Env;
			}
			if (InSound == EUISound::Error)
			{
				Sample =
					(FMath::Sin(2.0f * Pi * Freq * T) + 0.5f * FMath::Sin(2.0f * Pi * (Freq * 1.5f) * T)) * Amp * Env;
			}
			OutSamples[I] = Sample;
		}
	}

} // namespace

struct FAudioDevice::FImpl
{
	static constexpr int32 MaxVoices = 24;

	struct FVoice
	{
		ma_sound Sound{};
		ma_audio_buffer Buffer{};
		TArray<float> Pcm; // keeps the buffer memory alive for UI tones
		TArray<int16> Pcm16; // keeps the buffer memory alive for sound waves
		bool bInUse = false;
		bool bOwnsBuffer = false;
	};

	/** The samples of a UI cue (SetUiSound); empty: its procedural tone. */
	struct FUiSound
	{
		TArray<int16> Samples;
		int32 NumChannels = 0;
		int32 SampleRate = 0;
	};

	ma_engine Engine{};
	bool bEngineOk = false;
	FVoice Voices[MaxVoices]{};
	ma_sound Music{};
	ma_audio_buffer MusicBuffer{};
	TArray<int16> MusicPcm;
	bool bMusicInUse = false;
	FUiSound UiSounds[NumUISounds];

	[[nodiscard]] FVoice* AcquireVoice()
	{
		for (FVoice& Voice : Voices)
		{
			if (!Voice.bInUse)
			{
				return &Voice;
			}
		}
		// Steal oldest finished or first slot.
		for (FVoice& Voice : Voices)
		{
			if (Voice.bInUse && !ma_sound_is_playing(&Voice.Sound))
			{
				ReleaseVoice(Voice);
				return &Voice;
			}
		}
		ReleaseVoice(Voices[0]);
		return &Voices[0];
	}

	void ReleaseVoice(FVoice& Voice)
	{
		if (!Voice.bInUse)
		{
			return;
		}
		ma_sound_uninit(&Voice.Sound);
		if (Voice.bOwnsBuffer)
		{
			ma_audio_buffer_uninit(&Voice.Buffer);
			Voice.bOwnsBuffer = false;
		}
		Voice.Pcm.Empty();
		Voice.Pcm16.Empty();
		Voice.bInUse = false;
		FMemory::Memzero(&Voice.Sound, sizeof(Voice.Sound));
		FMemory::Memzero(&Voice.Buffer, sizeof(Voice.Buffer));
	}

	void ReleaseMusic()
	{
		if (!bMusicInUse)
		{
			return;
		}
		ma_sound_uninit(&Music);
		ma_audio_buffer_uninit(&MusicBuffer);
		FMemory::Memzero(&Music, sizeof(Music));
		FMemory::Memzero(&MusicBuffer, sizeof(MusicBuffer));
		MusicPcm.Empty();
		bMusicInUse = false;
	}

	void ReapFinished()
	{
		for (FVoice& Voice : Voices)
		{
			if (Voice.bInUse && !ma_sound_is_playing(&Voice.Sound))
			{
				ReleaseVoice(Voice);
			}
		}
	}

	void ReleaseAll()
	{
		ReleaseMusic();
		for (FVoice& Voice : Voices)
		{
			ReleaseVoice(Voice);
		}
	}

	/**
	 * Starts a one-shot voice over PCM16 samples it copies; Flags are the ma_sound flags (spatialization). Returns the
	 * voice, or null when the device is off or the samples are invalid.
	 */
	FVoice* PlayPcm16(const int16* Samples, int32 NumFrames, int32 NumChannels, int32 SampleRate, ma_uint32 Flags,
		float VolumeMultiplier)
	{
		if (!bEngineOk || Samples == nullptr || NumFrames <= 0 || NumChannels <= 0 || SampleRate <= 0)
		{
			return nullptr;
		}
		ReapFinished();
		FVoice* Voice = AcquireVoice();
		Voice->Pcm16.Append(Samples, NumFrames * NumChannels);
		ma_audio_buffer_config BufferConfig = ma_audio_buffer_config_init(ma_format_s16,
			static_cast<ma_uint32>(NumChannels), static_cast<ma_uint64>(NumFrames), Voice->Pcm16.GetData(), nullptr);
		BufferConfig.sampleRate = static_cast<ma_uint32>(SampleRate);
		if (ma_audio_buffer_init(&BufferConfig, &Voice->Buffer) != MA_SUCCESS)
		{
			Voice->Pcm16.Empty();
			return nullptr;
		}
		if (ma_sound_init_from_data_source(&Engine, &Voice->Buffer, Flags, nullptr, &Voice->Sound) != MA_SUCCESS)
		{
			ma_audio_buffer_uninit(&Voice->Buffer);
			Voice->Pcm16.Empty();
			return nullptr;
		}
		Voice->bInUse = true;
		Voice->bOwnsBuffer = true;
		ma_sound_set_volume(&Voice->Sound, Clamp01(VolumeMultiplier));
		return Voice;
	}
};

FAudioDevice::FAudioDevice()
	: Impl(MakeUnique<FImpl>())
{
}

FAudioDevice::~FAudioDevice()
{
	Shutdown();
}

bool FAudioDevice::Initialize(bool bInSilent)
{
	Shutdown();
	bSilent = bInSilent;
	MasterVolume = 1.0f;
	if (bInSilent)
	{
		bInitialized = true;
		return true;
	}

	ma_engine_config Config = ma_engine_config_init();
	const ma_result Result = ma_engine_init(&Config, &Impl->Engine);
	if (Result != MA_SUCCESS)
	{
		UE_LOG(LogAudioMixer, Warning, "ma_engine_init failed (%d); audio disabled", static_cast<int32>(Result));
		bSilent = true;
		bInitialized = true;
		return false;
	}
	Impl->bEngineOk = true;
	ma_engine_set_volume(&Impl->Engine, MasterVolume);
	bInitialized = true;
	UE_LOG(LogAudioMixer, Log, "miniaudio engine ready");
	return true;
}

void FAudioDevice::Shutdown()
{
	if (Impl)
	{
		Impl->ReleaseAll();
		if (Impl->bEngineOk)
		{
			ma_engine_uninit(&Impl->Engine);
			Impl->bEngineOk = false;
			FMemory::Memzero(&Impl->Engine, sizeof(Impl->Engine));
		}
	}
	bInitialized = false;
	bSilent = true;
}

void FAudioDevice::Tick()
{
	if (Impl && Impl->bEngineOk)
	{
		Impl->ReapFinished();
	}
}

void FAudioDevice::SetMasterVolume(float Volume01)
{
	MasterVolume = Clamp01(Volume01);
	if (Impl && Impl->bEngineOk)
	{
		ma_engine_set_volume(&Impl->Engine, MasterVolume);
	}
}

void FAudioDevice::SetListener(const FVector& Location, const FVector& Forward, const FVector& Up)
{
	if (!Impl || !Impl->bEngineOk)
	{
		return;
	}
	const FVector Metres = ToAudioMetres(Location);
	const FVector AudioForward = ToAudioDirection(Forward);
	const FVector AudioUp = ToAudioDirection(Up);
	ma_engine_listener_set_position(&Impl->Engine, 0, Metres.X, Metres.Y, Metres.Z);
	ma_engine_listener_set_direction(&Impl->Engine, 0, AudioForward.X, AudioForward.Y, AudioForward.Z);
	ma_engine_listener_set_world_up(&Impl->Engine, 0, AudioUp.X, AudioUp.Y, AudioUp.Z);
}

void FAudioDevice::PlaySound2D(const FSoundWavePCM& Sound, float VolumeMultiplier)
{
	if (!Impl || !Sound.IsValid())
	{
		return;
	}
	if (FImpl::FVoice* Voice = Impl->PlayPcm16(Sound.Samples, Sound.NumFrames, Sound.NumChannels, Sound.SampleRate,
			MA_SOUND_FLAG_NO_SPATIALIZATION, VolumeMultiplier))
	{
		ma_sound_start(&Voice->Sound);
	}
}

void FAudioDevice::PlaySoundAtLocation(const FSoundWavePCM& Sound, const FVector& Location, float VolumeMultiplier)
{
	if (!Impl || !Sound.IsValid())
	{
		return;
	}
	if (FImpl::FVoice* Voice =
			Impl->PlayPcm16(Sound.Samples, Sound.NumFrames, Sound.NumChannels, Sound.SampleRate, 0, VolumeMultiplier))
	{
		ma_sound_set_spatialization_enabled(&Voice->Sound, MA_TRUE);
		const FVector Metres = ToAudioMetres(Location);
		ma_sound_set_position(&Voice->Sound, Metres.X, Metres.Y, Metres.Z);
		ma_sound_start(&Voice->Sound);
	}
}

void FAudioDevice::SetUiSound(EUISound InSound, const FSoundWavePCM& Sound)
{
	FImpl::FUiSound& Ui = Impl->UiSounds[static_cast<int32>(InSound)];
	Ui.Samples.Reset();
	Ui.NumChannels = 0;
	Ui.SampleRate = 0;
	if (Sound.IsValid())
	{
		Ui.Samples.Append(Sound.Samples, Sound.NumFrames * Sound.NumChannels);
		Ui.NumChannels = Sound.NumChannels;
		Ui.SampleRate = Sound.SampleRate;
	}
}

bool FAudioDevice::HasUiSound(EUISound InSound) const
{
	return Impl->UiSounds[static_cast<int32>(InSound)].Samples.Num() > 0;
}

void FAudioDevice::PlayUiSound(EUISound InSound, float VolumeMultiplier)
{
	if (!Impl || !Impl->bEngineOk)
	{
		return;
	}

	const FImpl::FUiSound& Ui = Impl->UiSounds[static_cast<int32>(InSound)];
	if (Ui.Samples.Num() > 0)
	{
		FSoundWavePCM Sound;
		Sound.Samples = Ui.Samples.GetData();
		Sound.NumChannels = Ui.NumChannels;
		Sound.NumFrames = Ui.Samples.Num() / Ui.NumChannels;
		Sound.SampleRate = Ui.SampleRate;
		PlaySound2D(Sound, VolumeMultiplier);
		return;
	}

	Impl->ReapFinished();
	TArray<float> Samples;
	int32 SampleRate = 44100;
	BuildUiTone(InSound, Samples, SampleRate);
	if (Samples.Num() == 0)
	{
		return;
	}

	FImpl::FVoice* Voice = Impl->AcquireVoice();
	Voice->Pcm = MoveTemp(Samples);

	ma_audio_buffer_config BufferConfig = ma_audio_buffer_config_init(
		ma_format_f32, 1, static_cast<ma_uint64>(Voice->Pcm.Num()), Voice->Pcm.GetData(), nullptr);
	BufferConfig.sampleRate = static_cast<ma_uint32>(SampleRate);

	if (ma_audio_buffer_init(&BufferConfig, &Voice->Buffer) != MA_SUCCESS)
	{
		Voice->Pcm.Empty();
		return;
	}
	if (ma_sound_init_from_data_source(&Impl->Engine, &Voice->Buffer,
			MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &Voice->Sound) != MA_SUCCESS)
	{
		ma_audio_buffer_uninit(&Voice->Buffer);
		Voice->Pcm.Empty();
		return;
	}
	Voice->bInUse = true;
	Voice->bOwnsBuffer = true;
	ma_sound_set_volume(&Voice->Sound, Clamp01(VolumeMultiplier));
	ma_sound_start(&Voice->Sound);
}

void FAudioDevice::PlayMusic(const FSoundWavePCM& Sound, float VolumeMultiplier)
{
	if (!Impl || !Impl->bEngineOk || !Sound.IsValid())
	{
		return;
	}
	Impl->ReleaseMusic();
	Impl->MusicPcm.Append(Sound.Samples, Sound.NumFrames * Sound.NumChannels);
	ma_audio_buffer_config BufferConfig =
		ma_audio_buffer_config_init(ma_format_s16, static_cast<ma_uint32>(Sound.NumChannels),
			static_cast<ma_uint64>(Sound.NumFrames), Impl->MusicPcm.GetData(), nullptr);
	BufferConfig.sampleRate = static_cast<ma_uint32>(Sound.SampleRate);
	if (ma_audio_buffer_init(&BufferConfig, &Impl->MusicBuffer) != MA_SUCCESS)
	{
		Impl->MusicPcm.Empty();
		return;
	}
	if (ma_sound_init_from_data_source(
			&Impl->Engine, &Impl->MusicBuffer, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &Impl->Music) != MA_SUCCESS)
	{
		ma_audio_buffer_uninit(&Impl->MusicBuffer);
		Impl->MusicPcm.Empty();
		return;
	}
	Impl->bMusicInUse = true;
	ma_sound_set_looping(&Impl->Music, MA_TRUE);
	ma_sound_set_volume(&Impl->Music, Clamp01(VolumeMultiplier));
	ma_sound_start(&Impl->Music);
}

void FAudioDevice::StopMusic()
{
	if (Impl)
	{
		Impl->ReleaseMusic();
	}
}

bool FAudioDevice::IsMusicPlaying() const
{
	return Impl != nullptr && Impl->bMusicInUse && ma_sound_is_playing(&Impl->Music);
}
