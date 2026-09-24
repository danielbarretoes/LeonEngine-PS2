#pragma once

#include "RHIHandles.h"

#include <string>

/// HDR environment as OpenGL cubemaps (RGB16F):
/// - specular/env map with mips (roughness → textureLod)
/// - low-res irradiance map (diffuse IBL / Lambertian convolution)
class RENDERER_API FEnvironmentMap
{
public:
	static constexpr int DefaultFaceSize = 512;
	static constexpr int DefaultIrradianceSize = 32;

	FEnvironmentMap() = default;
	~FEnvironmentMap();

	FEnvironmentMap(const FEnvironmentMap&) = delete;
	FEnvironmentMap& operator=(const FEnvironmentMap&) = delete;
	FEnvironmentMap(FEnvironmentMap&& Other) noexcept;
	FEnvironmentMap& operator=(FEnvironmentMap&& Other) noexcept;

	/// Load Radiance HDR (.hdr) equirectangular → env cubemap + irradiance cubemap.
	[[nodiscard]] static FEnvironmentMap LoadFromHdr(
		const std::string& Path, int InFaceSize = DefaultFaceSize, int IrradianceSize = DefaultIrradianceSize);

	void Bind(unsigned int Unit = 0) const;
	void BindIrradiance(unsigned int Unit) const;

	[[nodiscard]] bool Valid() const
	{
		return Id != InvalidTexture;
	}
	[[nodiscard]] bool HasIrradiance() const
	{
		return IrradianceId != InvalidTexture;
	}
	[[nodiscard]] FRHITextureId GetId() const
	{
		return Id;
	}
	[[nodiscard]] int GetFaceSize() const
	{
		return FaceSize;
	}
	/// Number of mip levels (base + mips). Max LOD index is MipCount()-1.
	[[nodiscard]] int GetMipCount() const
	{
		return MipCount;
	}
	[[nodiscard]] float MaxLod() const
	{
		return MipCount > 0 ? static_cast<float>(MipCount - 1) : 0.0f;
	}

private:
	void Destroy();

	FRHITextureId Id = InvalidTexture;
	FRHITextureId IrradianceId = InvalidTexture;
	int FaceSize = 0;
	int MipCount = 0;
};
