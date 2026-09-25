#pragma once

#include "CoreMinimal.h"
#include "RHIHandles.h"

class UTexture2D;

/**
 * The GPU texture of a UTexture2D (UE: FTexture2DResource): mip 0 of its platform data with generated mipmaps,
 * trilinear filtering and repeat wrapping. FRenderResourceCache makes it the first time the texture is bound; the
 * renderer also makes its own (white, flat normal) from RGBA8 texels.
 */
class FTexture2DResource
{
public:
	explicit FTexture2DResource(const UTexture2D& Texture);
	/** SizeX x SizeY RGBA8 texels, bottom row first. */
	FTexture2DResource(int32 SizeX, int32 SizeY, const uint8* Rgba);
	~FTexture2DResource();

	FTexture2DResource(const FTexture2DResource&) = delete;
	FTexture2DResource& operator=(const FTexture2DResource&) = delete;

	void Bind(uint32 Unit = 0) const;
	[[nodiscard]] bool Valid() const
	{
		return Id != 0;
	}

private:
	/** Uploads SizeX x SizeY texels of the GL Format (GL_RGBA or GL_BGRA, 8 bits each). */
	void Create(int32 SizeX, int32 SizeY, uint32 Format, const void* Texels);

	FRHITextureId Id = InvalidTexture;
};
