#pragma once

#include <glm/vec3.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>


/// Built-in UI / feedback cues. Prefers Content WAVs when present; procedural fallback.
enum class EUISound : std::uint8_t {
    Click = 0,
    Confirm = 1,
    Back = 2,
    Error = 3,
};

/// Unreal-like audio subsystem (UAudioDevice / UGameplayStatics PlaySound lite).
/// Backed by miniaudio. Safe no-op when Initialize fails or headless silent mode.
class FAudioDevice {
public:
    FAudioDevice();
    ~FAudioDevice();

    FAudioDevice(const FAudioDevice&) = delete;
    FAudioDevice& operator=(const FAudioDevice&) = delete;

    /// `silent` skips device open (dedicated / CI). Returns false only on hard failure when
    /// not silent (engine still runs; subsequent Play* become no-ops).
    bool Initialize(bool bInSilent = false);
    void Shutdown();
    /// Reap finished one-shots (call once per frame from Engine).
    void Tick();
    [[nodiscard]] bool IsInitialized() const { return bInitialized; }
    [[nodiscard]] bool IsSilent() const { return bSilent; }

    void SetMasterVolume(float Volume01);
    [[nodiscard]] float GetMasterVolume() const { return MasterVolume; }

    /// Listener for 3D (Unreal SetListener). Call from Engine after camera update.
    void SetListener(const glm::vec3& Location, const glm::vec3& Forward, const glm::vec3& Up);

    /// Unreal PlaySound2D — fire-and-forget WAV/FLAC/MP3/OGG under FPaths::ResolveAssetPath.
    void PlaySound2D(std::string_view AssetRelativePath, float VolumeMultiplier = 1.0f);

    /// Unreal PlaySoundAtLocation — spatialized one-shot.
    void PlaySoundAtLocation(std::string_view AssetRelativePath, const glm::vec3& Location,
                             float VolumeMultiplier = 1.0f);

    /// UI cue: tries Content `assets/Audio/UI/UI_*.wav`, else procedural tone.
    void PlayUiSound(EUISound InSound, float VolumeMultiplier = 1.0f);

    /// Looping 2D music bed (dedicated slot, not the one-shot voice pool). Replaces any prior bed.
    void PlayMusic(std::string_view AssetRelativePath, float VolumeMultiplier = 0.35f);
    void StopMusic();
    [[nodiscard]] bool IsMusicPlaying() const;

private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
    bool bInitialized = false;
    bool bSilent = true;
    float MasterVolume = 1.0f;
};

