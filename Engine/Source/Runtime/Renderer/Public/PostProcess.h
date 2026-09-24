#pragma once

#include <cstdint>
#include "RHIHandles.h"
#include <vector>


/// Full-res SSAO ping-pong targets (R16F).
class FSSAOTarget {
public:
    FSSAOTarget() = default;
    ~FSSAOTarget();

    FSSAOTarget(const FSSAOTarget&) = delete;
    FSSAOTarget& operator=(const FSSAOTarget&) = delete;

    [[nodiscard]] bool EnsureSize(int width, int height);
    void Destroy();

    void BindWrite(int index) const; // 0 or 1
    void BindColorTexture(int index, unsigned int unit) const;

    [[nodiscard]] bool Valid() const {
        return fbo_[0] != kInvalidFramebuffer && fbo_[1] != kInvalidFramebuffer;
    }
    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }

private:
    FRHIFramebufferId fbo_[2]{};
    FRHITextureId color_[2]{};
    int width_ = 0;
    int height_ = 0;
};

/// Runtime quality for post-process / shadows (Unreal-like scalability group lite).
enum class EPostProcessQuality : std::uint8_t {
    Off = 0,
    Low = 1,
    Medium = 2,
    High = 3,
};

struct FPostProcessSettings {
    bool enabled = true;
    /// Default: Low — light SSAO, no FXAA, 1024 shadows (good for editor / mid PCs).
    EPostProcessQuality quality = EPostProcessQuality::Low;
    bool ambientOcclusion = true;
    bool fxaa = false;
    bool earlyZ = false;
    float aoIntensity = 0.75f;
    float aoRadius = 0.45f;
    float aoBias = 0.04f;
    float aoPower = 1.0f;
    float exposure = 1.0f; // multiplied with level EnvironmentExposure in composite
    int shadowMapSize = 1024;
    int aoSampleCount = 8;
};

/// Apply Low / Medium / High scalability (Off disables the whole post stack).
inline void ApplyPostProcessQuality(FPostProcessSettings& settings, EPostProcessQuality quality) {
    settings.quality = quality;
    switch (quality) {
    case EPostProcessQuality::Off:
        settings.enabled = false;
        settings.ambientOcclusion = false;
        settings.fxaa = false;
        settings.earlyZ = false;
        settings.shadowMapSize = 1024;
        settings.aoSampleCount = 0;
        break;
    case EPostProcessQuality::Low:
        settings.enabled = true;
        settings.ambientOcclusion = true;
        settings.fxaa = false;
        settings.earlyZ = false;
        settings.shadowMapSize = 1024;
        settings.aoSampleCount = 8;
        settings.aoIntensity = 0.75f;
        settings.aoBias = 0.04f;
        settings.aoPower = 1.0f;
        break;
    case EPostProcessQuality::Medium:
        settings.enabled = true;
        settings.ambientOcclusion = true;
        settings.fxaa = true;
        settings.earlyZ = false;
        settings.shadowMapSize = 2048;
        settings.aoSampleCount = 16;
        settings.aoIntensity = 0.85f;
        settings.aoBias = 0.035f;
        settings.aoPower = 1.0f;
        break;
    case EPostProcessQuality::High:
        settings.enabled = true;
        settings.ambientOcclusion = true;
        settings.fxaa = true;
        settings.earlyZ = true;
        settings.shadowMapSize = 2048;
        settings.aoSampleCount = 32;
        settings.aoIntensity = 0.9f;
        settings.aoBias = 0.035f;
        settings.aoPower = 1.0f;
        break;
    }
}

