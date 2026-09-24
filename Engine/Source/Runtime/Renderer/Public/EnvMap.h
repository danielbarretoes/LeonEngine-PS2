#pragma once

#include "RHIHandles.h"
#include <string>

namespace leon {

/// HDR environment as OpenGL cubemaps (RGB16F):
/// - specular/env map with mips (roughness → textureLod)
/// - low-res irradiance map (diffuse IBL / Lambertian convolution)
class EnvMap {
public:
    static constexpr int kDefaultFaceSize = 512;
    static constexpr int kDefaultIrradianceSize = 32;

    EnvMap() = default;
    ~EnvMap();

    EnvMap(const EnvMap&) = delete;
    EnvMap& operator=(const EnvMap&) = delete;
    EnvMap(EnvMap&& other) noexcept;
    EnvMap& operator=(EnvMap&& other) noexcept;

    /// Load Radiance HDR (.hdr) equirectangular → env cubemap + irradiance cubemap.
    [[nodiscard]] static EnvMap LoadFromHdr(const std::string& path,
                                            int faceSize = kDefaultFaceSize,
                                            int irradianceSize = kDefaultIrradianceSize);

    void Bind(unsigned int unit = 0) const;
    void BindIrradiance(unsigned int unit) const;

    [[nodiscard]] bool Valid() const { return id_ != rhi::kInvalidTexture; }
    [[nodiscard]] bool HasIrradiance() const { return irradianceId_ != rhi::kInvalidTexture; }
    [[nodiscard]] rhi::RHITextureId Id() const { return id_; }
    [[nodiscard]] int FaceSize() const { return faceSize_; }
    /// Number of mip levels (base + mips). Max LOD index is MipCount()-1.
    [[nodiscard]] int MipCount() const { return mipCount_; }
    [[nodiscard]] float MaxLod() const {
        return mipCount_ > 0 ? static_cast<float>(mipCount_ - 1) : 0.0f;
    }

private:
    void Destroy();

    rhi::RHITextureId id_ = rhi::kInvalidTexture;
    rhi::RHITextureId irradianceId_ = rhi::kInvalidTexture;
    int faceSize_ = 0;
    int mipCount_ = 0;
};

} // namespace leon
