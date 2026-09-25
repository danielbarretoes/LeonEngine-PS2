#pragma once

#include "CoreMinimal.h"
#include "RHIHandles.h"

/**
 * Depth-only shadow map for directional light 0 (orthographic + manual PCF in the lit shader).
 * The depth texture uses GL_NEAREST so PCF samples discrete texels (not hardware-filtered depth).
 */
class RENDERER_API FShadowMap
{
public:
	static constexpr int DefaultSize = 2048;

	FShadowMap() = default;
	~FShadowMap();

	FShadowMap(const FShadowMap&) = delete;
	FShadowMap& operator=(const FShadowMap&) = delete;

	bool Create(int InSize = DefaultSize);
	void Destroy();

	void Begin() const;
	/** Restores the draw target to RestoreFbo (0 = default framebuffer). */
	void End(int FramebufferWidth, int FramebufferHeight, FRHIFramebufferId RestoreFbo = InvalidFramebuffer) const;

	void BindDepthTexture(unsigned int Unit) const;
	[[nodiscard]] bool Valid() const
	{
		return Fbo != 0 && DepthTexture != 0;
	}
	[[nodiscard]] int GetSize() const
	{
		return Size;
	}

	/**
	 * Ortho light matrix tightly fitted to a world-space AABB of shadow casters: world to the light's GL clip space
	 * (UE light view, UE ortho, then ToGLClipSpace), as the shadow pass and the lit shader's lookup use it.
	 */
	[[nodiscard]] static FMatrix FitLightSpaceMatrix(
		const FVector& LightDirection, const FVector& WorldMin, const FVector& WorldMax, float Padding = 0.5f);

private:
	FRHIFramebufferId Fbo = InvalidFramebuffer;
	FRHITextureId DepthTexture = InvalidTexture;
	int Size = 0;
};
