#pragma once

#include "RHIHandles.h"

/// HDR scene color + sampleable depth for post-process (SSAO / tonemap).
class RENDERER_API FSceneColorTarget
{
public:
	FSceneColorTarget() = default;
	~FSceneColorTarget();

	FSceneColorTarget(const FSceneColorTarget&) = delete;
	FSceneColorTarget& operator=(const FSceneColorTarget&) = delete;

	/// Allocate or resize color (RGB16F) + depth texture.
	[[nodiscard]] bool EnsureSize(int InWidth, int InHeight);
	void Destroy();

	void Begin() const;
	/// Restore draw target to `restoreFbo` (0 = default framebuffer).
	void End(int FramebufferWidth, int FramebufferHeight, FRHIFramebufferId RestoreFbo = InvalidFramebuffer) const;

	void BindColorTexture(unsigned int Unit) const;
	void BindDepthTexture(unsigned int Unit) const;

	[[nodiscard]] bool Valid() const
	{
		return Fbo != 0 && ColorTexture != 0 && DepthTexture != 0;
	}
	[[nodiscard]] FRHIFramebufferId Framebuffer() const
	{
		return Fbo;
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
	FRHIFramebufferId Fbo = InvalidFramebuffer;
	FRHITextureId ColorTexture = InvalidTexture;
	FRHITextureId DepthTexture = InvalidTexture;
	int Width = 0;
	int Height = 0;
};
