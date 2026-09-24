#pragma once

#include "RHIHandles.h"
#include <string>


/// HDR environment as OpenGL cubemaps (RGB16F):
/// - specular/env map with mips (roughness → textureLod)
/// - low-res irradiance map (diffuse IBL / Lambertian convolution)
class FEnvironmentMap {
public:
    static constexpr int kDefaultFaceSize = 512;
    static constexpr int kDefaultIrradianceSize = 32;

    FEnvironmentMap() = default;
    ~FEnvironmentMap();

    FEnvironmentMap(const FEnvironmentMap&) = delete;
    FEnvironmentMap& operator=(const FEnvironmentMap&) = delete;
    FEnvironmentMap(FEnvironmentMap&& other) noexcept;
    FEnvironmentMap& operator=(FEnvironmentMap&& other) noexcept;

    /// Load Radiance HDR (.hdr) equirectangular → env cubemap + irradiance cubemap.
    [[nodiscard]] static FEnvironmentMap LoadFromHdr(const std::string& path,
                                            int faceSize = kDefaultFaceSize,
                                            int irradianceSize = kDefaultIrradianceSize);

    void Bind(unsigned int unit = 0) const;
    void BindIrradiance(unsigned int unit) const;

    [[nodiscard]] bool Valid() const { return id_ != kInvalidTexture; }
    [[nodiscard]] bool HasIrradiance() const { return irradianceId_ != kInvalidTexture; }
    [[nodiscard]] FRHITextureId Id() const { return id_; }
    [[nodiscard]] int FaceSize() const { return faceSize_; }
    /// Number of mip levels (base + mips). Max LOD index is MipCount()-1.
    [[nodiscard]] int MipCount() const { return mipCount_; }
    [[nodiscard]] float MaxLod() const {
        return mipCount_ > 0 ? static_cast<float>(mipCount_ - 1) : 0.0f;
    }

private:
    void Destroy();

    FRHITextureId id_ = kInvalidTexture;
    FRHITextureId irradianceId_ = kInvalidTexture;
    int faceSize_ = 0;
    int mipCount_ = 0;
};

