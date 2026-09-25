#include "AudioDevice.h"

#include "Containers/StringConv.h"
#include "Misc/Paths.h"

#include <miniaudio.h>

DEFINE_LOG_CATEGORY_STATIC(LogAudioMixer, Log, All);

namespace
{

	[[nodiscard]] float Clamp01(float V)
	{
		return FMath::Clamp(V, 0.0f, 1.0f);
	}

	/** Legacy content path of a sound; empty for an empty name. */
	[[nodiscard]] FString ResolveSoundPath(const TCHAR* AssetRelativePath)
	{
		if (AssetRelativePath == nullptr || AssetRelativePath[0] == '\0')
		{
			return FString();
		}
		return FPaths::ResolveLegacyContentPath(AssetRelativePath);
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
		TArray<float> Pcm; // keeps buffer memory alive for UI tones
		bool bInUse = false;
		bool bOwnsBuffer = false;
	};

	ma_engine Engine{};
	bool bEngineOk = false;
	FVoice Voices[MaxVoices]{};
	ma_sound Music{};
	bool bMusicInUse = false;

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
		FMemory::Memzero(&Music, sizeof(Music));
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

	[[nodiscard]] bool PlayFile2D(const TCHAR* AssetRelativePath, float VolumeMultiplier)
	{
		if (!bEngineOk)
		{
			return false;
		}
		const FString Path = ResolveSoundPath(AssetRelativePath);
		if (Path.IsEmpty())
		{
			return false;
		}
		ReapFinished();
		FVoice* Voice = AcquireVoice();
		const ma_result Result = ma_sound_init_from_file(&Engine, TCHAR_TO_UTF8(*Path),
			MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr,
			&Voice->Sound);
		if (Result != MA_SUCCESS)
		{
			return false;
		}
		Voice->bInUse = true;
		Voice->bOwnsBuffer = false;
		ma_sound_set_volume(&Voice->Sound, Clamp01(VolumeMultiplier));
		ma_sound_start(&Voice->Sound);
		return true;
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
	ma_engine_listener_set_position(&Impl->Engine, 0, Location.X, Location.Y, Location.Z);
	ma_engine_listener_set_direction(&Impl->Engine, 0, Forward.X, Forward.Y, Forward.Z);
	ma_engine_listener_set_world_up(&Impl->Engine, 0, Up.X, Up.Y, Up.Z);
}

void FAudioDevice::PlaySound2D(const TCHAR* AssetRelativePath, float VolumeMultiplier)
{
	if (!Impl)
	{
		return;
	}
	(void)Impl->PlayFile2D(AssetRelativePath, VolumeMultiplier);
}

void FAudioDevice::PlaySoundAtLocation(const TCHAR* AssetRelativePath, const FVector& Location, float VolumeMultiplier)
{
	if (!Impl || !Impl->bEngineOk)
	{
		return;
	}
	const FString Path = ResolveSoundPath(AssetRelativePath);
	if (Path.IsEmpty())
	{
		return;
	}
	Impl->ReapFinished();
	FImpl::FVoice* Voice = Impl->AcquireVoice();
	const ma_result Result = ma_sound_init_from_file(&Impl->Engine, TCHAR_TO_UTF8(*Path),
		MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_DECODE, nullptr, nullptr, &Voice->Sound);
	if (Result != MA_SUCCESS)
	{
		return;
	}
	Voice->bInUse = true;
	Voice->bOwnsBuffer = false;
	ma_sound_set_spatialization_enabled(&Voice->Sound, MA_TRUE);
	ma_sound_set_position(&Voice->Sound, Location.X, Location.Y, Location.Z);
	ma_sound_set_volume(&Voice->Sound, Clamp01(VolumeMultiplier));
	ma_sound_start(&Voice->Sound);
}

void FAudioDevice::PlayUiSound(EUISound InSound, float VolumeMultiplier)
{
	if (!Impl || !Impl->bEngineOk)
	{
		return;
	}

	const char* AssetPath = nullptr;
	switch (InSound)
	{
		case EUISound::Click:
			AssetPath = "assets/Audio/UI/UI_Click.wav";
			break;
		case EUISound::Confirm:
			AssetPath = "assets/Audio/UI/UI_Confirm.wav";
			break;
		case EUISound::Back:
			AssetPath = "assets/Audio/UI/UI_Back.wav";
			break;
		case EUISound::Error:
			AssetPath = "assets/Audio/UI/UI_Error.wav";
			break;
	}
	if (AssetPath != nullptr && Impl->PlayFile2D(AssetPath, VolumeMultiplier))
	{
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

void FAudioDevice::PlayMusic(const TCHAR* AssetRelativePath, float VolumeMultiplier)
{
	if (!Impl || !Impl->bEngineOk)
	{
		return;
	}
	const FString Path = ResolveSoundPath(AssetRelativePath);
	if (Path.IsEmpty())
	{
		return;
	}
	Impl->ReleaseMusic();
	const ma_result Result = ma_sound_init_from_file(&Impl->Engine, TCHAR_TO_UTF8(*Path),
		MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, &Impl->Music);
	if (Result != MA_SUCCESS)
	{
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
