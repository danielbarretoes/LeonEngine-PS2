#include "GSEmulator/PS2TexturePreview.h"

#include "PalettedTexture.h"

bool ConvertTextureAsPS2Cook(const uint8* Rgba, int32 Width, int32 Height, int32& OutSizeX, int32& OutSizeY,
	EPixelFormat& OutFormat, TArray<uint8>& OutData)
{
	FPalettedTexture Texture;
	if (!FPalettedTextureBuilder::Build(Rgba, Width, Height, Texture))
	{
		return false;
	}
	OutSizeX = Texture.SizeX;
	OutSizeY = Texture.SizeY;
	OutFormat = Texture.Format;
	OutData = MoveTemp(Texture.Data);
	return true;
}
