#pragma once

#include "RHIHandles.h"

#include <cstdint>
#include <vector>

/// Full-res SSAO ping-pong targets (R16F).
class RENDERER_API FSSAOTarget
{
public:
	FSSAOTarget() = default;
	~FSSAOTarget();

	FSSAOTarget(const FSSAOTarget&) = delete;
	FSSAOTarget& operator=(const FSSAOTarget&) = delete;

	[[nodiscard]] bool EnsureSize(int InWidth, int InHeight);
	void Destroy();

	void BindWrite(int Index) const; // 0 or 1
	void BindColorTexture(int Index, unsigned int Unit) const;

	[[nodiscard]] bool Valid() const
	{
		return Fbo[0] != InvalidFramebuffer && Fbo[1] != InvalidFramebuffer;
	}
	[[nodiscard]] int GetWidth() const
	{
		return Width;
	}
	[[nodiscard]] int GetHeight() const
	{
		return Height;
	}

private:
	FRHIFramebufferId Fbo[2]{};
	FRHITextureId Color[2]{};
	int Width = 0;
	int Height = 0;
};

/// Runtime quality for post-process / shadows (Unreal-like scalability group lite).
enum class EPostProcessQuality : std::uint8_t
{
	Off = 0,
	Low = 1,
	Medium = 2,
	High = 3,
};

struct RENDERER_API FPostProcessSettings
{
	bool bEnabled = true;
	/// Default: Low — light SSAO, no FXAA, 1024 shadows (good for editor / mid PCs).
	EPostProcessQuality Quality = EPostProcessQuality::Low;
	bool bAmbientOcclusion = true;
	bool bFxaa = false;
	bool bEarlyZ = false;
	float AoIntensity = 0.75f;
	float AoRadius = 0.45f;
	float AoBias = 0.04f;
	float AoPower = 1.0f;
	float Exposure = 1.0f; // scene exposure applied in the composite pass
	int ShadowMapSize = 1024;
	int AoSampleCount = 8;
};

/// Apply Low / Medium / High scalability (Off disables the whole post stack).
inline void ApplyPostProcessQuality(FPostProcessSettings& Settings, EPostProcessQuality InQuality)
{
	Settings.Quality = InQuality;
	switch (InQuality)
	{
		case EPostProcessQuality::Off:
			Settings.bEnabled = false;
			Settings.bAmbientOcclusion = false;
			Settings.bFxaa = false;
			Settings.bEarlyZ = false;
			Settings.ShadowMapSize = 1024;
			Settings.AoSampleCount = 0;
			break;
		case EPostProcessQuality::Low:
			Settings.bEnabled = true;
			Settings.bAmbientOcclusion = true;
			Settings.bFxaa = false;
			Settings.bEarlyZ = false;
			Settings.ShadowMapSize = 1024;
			Settings.AoSampleCount = 8;
			Settings.AoIntensity = 0.75f;
			Settings.AoBias = 0.04f;
			Settings.AoPower = 1.0f;
			break;
		case EPostProcessQuality::Medium:
			Settings.bEnabled = true;
			Settings.bAmbientOcclusion = true;
			Settings.bFxaa = true;
			Settings.bEarlyZ = false;
			Settings.ShadowMapSize = 2048;
			Settings.AoSampleCount = 16;
			Settings.AoIntensity = 0.85f;
			Settings.AoBias = 0.035f;
			Settings.AoPower = 1.0f;
			break;
		case EPostProcessQuality::High:
			Settings.bEnabled = true;
			Settings.bAmbientOcclusion = true;
			Settings.bFxaa = true;
			Settings.bEarlyZ = true;
			Settings.ShadowMapSize = 2048;
			Settings.AoSampleCount = 32;
			Settings.AoIntensity = 0.9f;
			Settings.AoBias = 0.035f;
			Settings.AoPower = 1.0f;
			break;
	}
}
