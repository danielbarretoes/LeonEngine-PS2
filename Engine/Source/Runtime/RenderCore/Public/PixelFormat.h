#pragma once

#include "CoreMinimal.h"

/**
 * The layout of a texture's texels (UE: EPixelFormat, Core's PixelFormat.h; Leon keeps it in RenderCore, next to the
 * other CPU render data). Only the formats Leon stores are listed, with UE's values, which the texture assets save.
 */
enum EPixelFormat : uint8
{
	PF_Unknown = 0,
	/** 8-bit blue, green, red, alpha (UE's default for transient textures). */
	PF_B8G8R8A8 = 2,
	/** 8-bit red, green, blue, alpha: Leon's texture assets, uploaded as GL_RGBA. */
	PF_R8G8B8A8 = 37,
};

/** Bytes of one texel of Format (UE: GPixelFormats[Format].BlockBytes); 0 for PF_Unknown. */
[[nodiscard]] inline int32 GetPixelFormatBytes(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_B8G8R8A8:
		case PF_R8G8B8A8:
			return 4;
		default:
			return 0;
	}
}
