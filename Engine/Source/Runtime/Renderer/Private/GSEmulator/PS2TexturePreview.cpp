#include "GSEmulator/PS2TexturePreview.h"

#include "PalettedTexture.h"

bool ConvertTextureAsPS2Cook(
	const uint8* Rgba, int32 Width, int32 Height, bool bSRGB, FGSTextureCache::FConvertedTexture& Out)
{
	FPalettedTexture Texture;
	if (!FPalettedTextureBuilder::Build(Rgba, Width, Height, bSRGB, Texture))
	{
		return false;
	}
	Out.SizeX = Texture.SizeX;
	Out.SizeY = Texture.SizeY;
	Out.Format = Texture.Format;
	Out.Data = MoveTemp(Texture.Data);
	Out.Mips = MoveTemp(Texture.Mips);
	return true;
}
