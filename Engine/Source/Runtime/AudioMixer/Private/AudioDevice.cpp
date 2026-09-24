#include "AudioDevice.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include "Misc/Paths.h"
#include <string>
#include <vector>

#include <miniaudio.h>

namespace {

[[nodiscard]] float Clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

void BuildUiTone(EUISound sound, std::vector<float>& outSamples, int& outSampleRate) {
    outSampleRate = 44100;
    float freq = 660.0f;
    float duration = 0.045f;
    float amp = 0.22f;
    switch (sound) {
    case EUISound::Click:
        freq = 880.0f;
        duration = 0.035f;
        amp = 0.18f;
        break;
    case EUISound::Confirm:
        freq = 520.0f;
        duration = 0.08f;
        amp = 0.22f;
        break;
    case EUISound::Back:
        freq = 320.0f;
        duration = 0.05f;
        amp = 0.16f;
        break;
    case EUISound::Error:
        freq = 180.0f;
        duration = 0.12f;
        amp = 0.2f;
        break;
    }
    const int n = std::max(1, static_cast<int>(duration * static_cast<float>(outSampleRate)));
    outSamples.resize(static_cast<std::size_t>(n));
    constexpr float kPi = 3.14159265f;
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(outSampleRate);
        const float env = 1.0f - (static_cast<float>(i) / static_cast<float>(n));
        float sample = std::sin(2.0f * kPi * freq * t) * amp * env;
        if (sound == EUISound::Confirm && t > 0.04f) {
            sample += std::sin(2.0f * kPi * 780.0f * t) * amp * 0.55f * env;
        }
        if (sound == EUISound::Error) {
            sample = (std::sin(2.0f * kPi * freq * t) +
                      0.5f * std::sin(2.0f * kPi * (freq * 1.5f) * t)) *
                     amp * env;
        }
        outSamples[static_cast<std::size_t>(i)] = sample;
    }
}

} // namespace

struct FAudioDevice::FImpl {
    static constexpr int kMaxVoices = 24;

    struct FVoice {
        ma_sound sound{};
        ma_audio_buffer buffer{};
        std::vector<float> pcm; // keeps buffer memory alive for UI tones
        bool inUse = false;
        bool ownsBuffer = false;
    };

    ma_engine engine{};
    bool engineOk = false;
    std::array<FVoice, kMaxVoices> voices{};
    ma_sound music{};
    bool musicInUse = false;

    [[nodiscard]] FVoice* AcquireVoice() {
        for (FVoice& voice : voices) {
            if (!voice.inUse) {
                return &voice;
            }
        }
        // Steal oldest finished or first slot.
        for (FVoice& voice : voices) {
            if (voice.inUse && !ma_sound_is_playing(&voice.sound)) {
                ReleaseVoice(voice);
                return &voice;
            }
        }
        ReleaseVoice(voices[0]);
        return &voices[0];
    }

    void ReleaseVoice(FVoice& voice) {
        if (!voice.inUse) {
            return;
        }
        ma_sound_uninit(&voice.sound);
        if (voice.ownsBuffer) {
            ma_audio_buffer_uninit(&voice.buffer);
            voice.ownsBuffer = false;
        }
        voice.pcm.clear();
        voice.inUse = false;
        std::memset(&voice.sound, 0, sizeof(voice.sound));
        std::memset(&voice.buffer, 0, sizeof(voice.buffer));
    }

    void ReleaseMusic() {
        if (!musicInUse) {
            return;
        }
        ma_sound_uninit(&music);
        std::memset(&music, 0, sizeof(music));
        musicInUse = false;
    }

    void ReapFinished() {
        for (FVoice& voice : voices) {
            if (voice.inUse && !ma_sound_is_playing(&voice.sound)) {
                ReleaseVoice(voice);
            }
        }
    }

    void ReleaseAll() {
        ReleaseMusic();
        for (FVoice& voice : voices) {
            ReleaseVoice(voice);
        }
    }

    [[nodiscard]] bool PlayFile2D(std::string_view assetRelativePath, float volumeMultiplier) {
        if (!engineOk || assetRelativePath.empty()) {
            return false;
        }
        ReapFinished();
        const std::string path = FPaths::ResolveAssetPath(std::string(assetRelativePath));
        if (path.empty()) {
            return false;
        }
        FVoice* voice = AcquireVoice();
        const ma_result result =
            ma_sound_init_from_file(&engine, path.c_str(),
                                    MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_STREAM |
                                        MA_SOUND_FLAG_NO_SPATIALIZATION,
                                    nullptr, nullptr, &voice->sound);
        if (result != MA_SUCCESS) {
            return false;
        }
        voice->inUse = true;
        voice->ownsBuffer = false;
        ma_sound_set_volume(&voice->sound, Clamp01(volumeMultiplier));
        ma_sound_start(&voice->sound);
        return true;
    }
};

FAudioDevice::FAudioDevice() : impl_(std::make_unique<FImpl>()) {}

FAudioDevice::~FAudioDevice() {
    Shutdown();
}

bool FAudioDevice::Initialize(bool silent) {
    Shutdown();
    silent_ = silent;
    masterVolume_ = 1.0f;
    if (silent) {
        initialized_ = true;
        return true;
    }

    ma_engine_config config = ma_engine_config_init();
    const ma_result result = ma_engine_init(&config, &impl_->engine);
    if (result != MA_SUCCESS) {
        std::cerr << "AudioDevice: ma_engine_init failed (" << static_cast<int>(result)
                  << ") -- audio disabled\n";
        silent_ = true;
        initialized_ = true;
        return false;
    }
    impl_->engineOk = true;
    ma_engine_set_volume(&impl_->engine, masterVolume_);
    initialized_ = true;
    std::cout << "AudioDevice: miniaudio engine ready\n";
    return true;
}

void FAudioDevice::Shutdown() {
    if (impl_) {
        impl_->ReleaseAll();
        if (impl_->engineOk) {
            ma_engine_uninit(&impl_->engine);
            impl_->engineOk = false;
            std::memset(&impl_->engine, 0, sizeof(impl_->engine));
        }
    }
    initialized_ = false;
    silent_ = true;
}

void FAudioDevice::Tick() {
    if (impl_ && impl_->engineOk) {
        impl_->ReapFinished();
    }
}

void FAudioDevice::SetMasterVolume(float volume01) {
    masterVolume_ = Clamp01(volume01);
    if (impl_ && impl_->engineOk) {
        ma_engine_set_volume(&impl_->engine, masterVolume_);
    }
}

void FAudioDevice::SetListener(const glm::vec3& location, const glm::vec3& forward,
                              const glm::vec3& up) {
    if (!impl_ || !impl_->engineOk) {
        return;
    }
    ma_engine_listener_set_position(&impl_->engine, 0, location.x, location.y, location.z);
    ma_engine_listener_set_direction(&impl_->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&impl_->engine, 0, up.x, up.y, up.z);
}

void FAudioDevice::PlaySound2D(std::string_view assetRelativePath, float volumeMultiplier) {
    if (!impl_) {
        return;
    }
    (void)impl_->PlayFile2D(assetRelativePath, volumeMultiplier);
}

void FAudioDevice::PlaySoundAtLocation(std::string_view assetRelativePath, const glm::vec3& location,
                                      float volumeMultiplier) {
    if (!impl_ || !impl_->engineOk || assetRelativePath.empty()) {
        return;
    }
    impl_->ReapFinished();
    const std::string path = FPaths::ResolveAssetPath(std::string(assetRelativePath));
    if (path.empty()) {
        return;
    }
    FImpl::FVoice* voice = impl_->AcquireVoice();
    const ma_result result =
        ma_sound_init_from_file(&impl_->engine, path.c_str(),
                                MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_DECODE, nullptr, nullptr,
                                &voice->sound);
    if (result != MA_SUCCESS) {
        return;
    }
    voice->inUse = true;
    voice->ownsBuffer = false;
    ma_sound_set_spatialization_enabled(&voice->sound, MA_TRUE);
    ma_sound_set_position(&voice->sound, location.x, location.y, location.z);
    ma_sound_set_volume(&voice->sound, Clamp01(volumeMultiplier));
    ma_sound_start(&voice->sound);
}

void FAudioDevice::PlayUiSound(EUISound sound, float volumeMultiplier) {
    if (!impl_ || !impl_->engineOk) {
        return;
    }

    const char* assetPath = nullptr;
    switch (sound) {
    case EUISound::Click:
        assetPath = "assets/Audio/UI/UI_Click.wav";
        break;
    case EUISound::Confirm:
        assetPath = "assets/Audio/UI/UI_Confirm.wav";
        break;
    case EUISound::Back:
        assetPath = "assets/Audio/UI/UI_Back.wav";
        break;
    case EUISound::Error:
        assetPath = "assets/Audio/UI/UI_Error.wav";
        break;
    }
    if (assetPath != nullptr && impl_->PlayFile2D(assetPath, volumeMultiplier)) {
        return;
    }

    impl_->ReapFinished();
    std::vector<float> samples;
    int sampleRate = 44100;
    BuildUiTone(sound, samples, sampleRate);
    if (samples.empty()) {
        return;
    }

    FImpl::FVoice* voice = impl_->AcquireVoice();
    voice->pcm = std::move(samples);

    ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
        ma_format_f32, 1, static_cast<ma_uint64>(voice->pcm.size()), voice->pcm.data(), nullptr);
    bufferConfig.sampleRate = static_cast<ma_uint32>(sampleRate);

    if (ma_audio_buffer_init(&bufferConfig, &voice->buffer) != MA_SUCCESS) {
        voice->pcm.clear();
        return;
    }
    if (ma_sound_init_from_data_source(&impl_->engine, &voice->buffer,
                                       MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr,
                                       &voice->sound) != MA_SUCCESS) {
        ma_audio_buffer_uninit(&voice->buffer);
        voice->pcm.clear();
        return;
    }
    voice->inUse = true;
    voice->ownsBuffer = true;
    ma_sound_set_volume(&voice->sound, Clamp01(volumeMultiplier));
    ma_sound_start(&voice->sound);
}

void FAudioDevice::PlayMusic(std::string_view assetRelativePath, float volumeMultiplier) {
    if (!impl_ || !impl_->engineOk || assetRelativePath.empty()) {
        return;
    }
    const std::string path = FPaths::ResolveAssetPath(std::string(assetRelativePath));
    if (path.empty()) {
        return;
    }
    impl_->ReleaseMusic();
    const ma_result result =
        ma_sound_init_from_file(&impl_->engine, path.c_str(),
                                MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr,
                                nullptr, &impl_->music);
    if (result != MA_SUCCESS) {
        return;
    }
    impl_->musicInUse = true;
    ma_sound_set_looping(&impl_->music, MA_TRUE);
    ma_sound_set_volume(&impl_->music, Clamp01(volumeMultiplier));
    ma_sound_start(&impl_->music);
}

void FAudioDevice::StopMusic() {
    if (impl_) {
        impl_->ReleaseMusic();
    }
}

bool FAudioDevice::IsMusicPlaying() const {
    return impl_ != nullptr && impl_->musicInUse && ma_sound_is_playing(&impl_->music);
}

