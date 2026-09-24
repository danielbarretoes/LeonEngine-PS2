#include "AudioDevice.h"

#include "Misc/Paths.h"

#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{

	[[nodiscard]] float Clamp01(float V)
	{
		return std::clamp(V, 0.0f, 1.0f);
	}

	void BuildUiTone(EUISound InSound, std::vector<float>& OutSamples, int& OutSampleRate)
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
		const int N = std::max(1, static_cast<int>(Duration * static_cast<float>(OutSampleRate)));
		OutSamples.resize(static_cast<std::size_t>(N));
		constexpr float Pi = 3.14159265f;
		for (int I = 0; I < N; ++I)
		{
			const float T = static_cast<float>(I) / static_cast<float>(OutSampleRate);
			const float Env = 1.0f - (static_cast<float>(I) / static_cast<float>(N));
			float Sample = std::sin(2.0f * Pi * Freq * T) * Amp * Env;
			if (InSound == EUISound::Confirm && T > 0.04f)
			{
				Sample += std::sin(2.0f * Pi * 780.0f * T) * Amp * 0.55f * Env;
			}
			if (InSound == EUISound::Error)
			{
				Sample = (std::sin(2.0f * Pi * Freq * T) + 0.5f * std::sin(2.0f * Pi * (Freq * 1.5f) * T)) * Amp * Env;
			}
			OutSamples[static_cast<std::size_t>(I)] = Sample;
		}
	}

} // namespace

struct FAudioDevice::FImpl
{
	static constexpr int MaxVoices = 24;

	struct FVoice
	{
		ma_sound Sound{};
		ma_audio_buffer Buffer{};
		std::vector<float> Pcm; // keeps buffer memory alive for UI tones
		bool bInUse = false;
		bool bOwnsBuffer = false;
	};

	ma_engine Engine{};
	bool bEngineOk = false;
	std::array<FVoice, MaxVoices> Voices{};
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
		Voice.Pcm.clear();
		Voice.bInUse = false;
		std::memset(&Voice.Sound, 0, sizeof(Voice.Sound));
		std::memset(&Voice.Buffer, 0, sizeof(Voice.Buffer));
	}

	void ReleaseMusic()
	{
		if (!bMusicInUse)
		{
			return;
		}
		ma_sound_uninit(&Music);
		std::memset(&Music, 0, sizeof(Music));
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

	[[nodiscard]] bool PlayFile2D(std::string_view AssetRelativePath, float VolumeMultiplier)
	{
		if (!bEngineOk || AssetRelativePath.empty())
		{
			return false;
		}
		ReapFinished();
		const std::string Path = FPaths::ResolveAssetPath(std::string(AssetRelativePath));
		if (Path.empty())
		{
			return false;
		}
		FVoice* Voice = AcquireVoice();
		const ma_result Result = ma_sound_init_from_file(&Engine, Path.c_str(),
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
	: Impl(std::make_unique<FImpl>())
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
		std::cerr << "AudioDevice: ma_engine_init failed (" << static_cast<int>(Result) << ") -- audio disabled\n";
		bSilent = true;
		bInitialized = true;
		return false;
	}
	Impl->bEngineOk = true;
	ma_engine_set_volume(&Impl->Engine, MasterVolume);
	bInitialized = true;
	std::cout << "AudioDevice: miniaudio engine ready\n";
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
			std::memset(&Impl->Engine, 0, sizeof(Impl->Engine));
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

void FAudioDevice::SetListener(const glm::vec3& Location, const glm::vec3& Forward, const glm::vec3& Up)
{
	if (!Impl || !Impl->bEngineOk)
	{
		return;
	}
	ma_engine_listener_set_position(&Impl->Engine, 0, Location.x, Location.y, Location.z);
	ma_engine_listener_set_direction(&Impl->Engine, 0, Forward.x, Forward.y, Forward.z);
	ma_engine_listener_set_world_up(&Impl->Engine, 0, Up.x, Up.y, Up.z);
}

void FAudioDevice::PlaySound2D(std::string_view AssetRelativePath, float VolumeMultiplier)
{
	if (!Impl)
	{
		return;
	}
	(void)Impl->PlayFile2D(AssetRelativePath, VolumeMultiplier);
}

void FAudioDevice::PlaySoundAtLocation(
	std::string_view AssetRelativePath, const glm::vec3& Location, float VolumeMultiplier)
{
	if (!Impl || !Impl->bEngineOk || AssetRelativePath.empty())
	{
		return;
	}
	Impl->ReapFinished();
	const std::string Path = FPaths::ResolveAssetPath(std::string(AssetRelativePath));
	if (Path.empty())
	{
		return;
	}
	FImpl::FVoice* Voice = Impl->AcquireVoice();
	const ma_result Result = ma_sound_init_from_file(
		&Impl->Engine, Path.c_str(), MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_DECODE, nullptr, nullptr, &Voice->Sound);
	if (Result != MA_SUCCESS)
	{
		return;
	}
	Voice->bInUse = true;
	Voice->bOwnsBuffer = false;
	ma_sound_set_spatialization_enabled(&Voice->Sound, MA_TRUE);
	ma_sound_set_position(&Voice->Sound, Location.x, Location.y, Location.z);
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
	std::vector<float> Samples;
	int SampleRate = 44100;
	BuildUiTone(InSound, Samples, SampleRate);
	if (Samples.empty())
	{
		return;
	}

	FImpl::FVoice* Voice = Impl->AcquireVoice();
	Voice->Pcm = std::move(Samples);

	ma_audio_buffer_config BufferConfig = ma_audio_buffer_config_init(
		ma_format_f32, 1, static_cast<ma_uint64>(Voice->Pcm.size()), Voice->Pcm.data(), nullptr);
	BufferConfig.sampleRate = static_cast<ma_uint32>(SampleRate);

	if (ma_audio_buffer_init(&BufferConfig, &Voice->Buffer) != MA_SUCCESS)
	{
		Voice->Pcm.clear();
		return;
	}
	if (ma_sound_init_from_data_source(&Impl->Engine, &Voice->Buffer,
			MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &Voice->Sound) != MA_SUCCESS)
	{
		ma_audio_buffer_uninit(&Voice->Buffer);
		Voice->Pcm.clear();
		return;
	}
	Voice->bInUse = true;
	Voice->bOwnsBuffer = true;
	ma_sound_set_volume(&Voice->Sound, Clamp01(VolumeMultiplier));
	ma_sound_start(&Voice->Sound);
}

void FAudioDevice::PlayMusic(std::string_view AssetRelativePath, float VolumeMultiplier)
{
	if (!Impl || !Impl->bEngineOk || AssetRelativePath.empty())
	{
		return;
	}
	const std::string Path = FPaths::ResolveAssetPath(std::string(AssetRelativePath));
	if (Path.empty())
	{
		return;
	}
	Impl->ReleaseMusic();
	const ma_result Result = ma_sound_init_from_file(&Impl->Engine, Path.c_str(),
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
