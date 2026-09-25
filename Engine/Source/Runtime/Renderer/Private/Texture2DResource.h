#pragma once

#include "CoreMinimal.h"
#include "RHIHandles.h"

class UTexture2D;

/**
 * The GPU texture of a UTexture2D (UE: FTexture2DResource): RGBA8 with mipmaps, trilinear filtering and repeat
 * wrapping. FRenderResourceCache makes it the first time the texture is bound; the renderer also makes its own
 * (white, flat normal) from CPU textures.
 */
class FTexture2DResource
{
public:
	explicit FTexture2DResource(const UTexture2D& Texture);
	~FTexture2DResource();

	FTexture2DResource(const FTexture2DResource&) = delete;
	FTexture2DResource& operator=(const FTexture2DResource&) = delete;

	void Bind(uint32 Unit = 0) const;
	[[nodiscard]] bool Valid() const
	{
		return Id != 0;
	}

private:
	FRHITextureId Id = InvalidTexture;
};
