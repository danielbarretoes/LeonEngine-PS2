#pragma once

#include <glm/vec3.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>


/// Built-in UI / feedback cues. Prefers Content WAVs when present; procedural fallback.
enum class EUiSound : std::uint8_t {
    Click = 0,
    Confirm = 1,
    Back = 2,
    Error = 3,
};

/// Unreal-like audio subsystem (UAudioDevice / UGameplayStatics PlaySound lite).
/// Backed by miniaudio. Safe no-op when Initialize fails or headless silent mode.
class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    /// `silent` skips device open (dedicated / CI). Returns false only on hard failure when
    /// not silent (engine still runs; subsequent Play* become no-ops).
    bool Initialize(bool silent = false);
    void Shutdown();
    /// Reap finished one-shots (call once per frame from Engine).
    void Tick();
    [[nodiscard]] bool IsInitialized() const { return initialized_; }
    [[nodiscard]] bool IsSilent() const { return silent_; }

    void SetMasterVolume(float volume01);
    [[nodiscard]] float GetMasterVolume() const { return masterVolume_; }

    /// Listener for 3D (Unreal SetListener). Call from Engine after camera update.
    void SetListener(const glm::vec3& location, const glm::vec3& forward, const glm::vec3& up);

    /// Unreal PlaySound2D — fire-and-forget WAV/FLAC/MP3/OGG under FPaths::ResolveAssetPath.
    void PlaySound2D(std::string_view assetRelativePath, float volumeMultiplier = 1.0f);

    /// Unreal PlaySoundAtLocation — spatialized one-shot.
    void PlaySoundAtLocation(std::string_view assetRelativePath, const glm::vec3& location,
                             float volumeMultiplier = 1.0f);

    /// UI cue: tries Content `assets/Audio/UI/UI_*.wav`, else procedural tone.
    void PlayUiSound(EUiSound sound, float volumeMultiplier = 1.0f);

    /// Looping 2D music bed (dedicated slot, not the one-shot voice pool). Replaces any prior bed.
    void PlayMusic(std::string_view assetRelativePath, float volumeMultiplier = 0.35f);
    void StopMusic();
    [[nodiscard]] bool IsMusicPlaying() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool initialized_ = false;
    bool silent_ = true;
    float masterVolume_ = 1.0f;
};

