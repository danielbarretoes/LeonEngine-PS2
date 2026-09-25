#pragma once

#include "CoreMinimal.h"

/**
 * A 2D texture asset (UE: UTexture2D): RGBA8 pixels, bottom row first (as OpenGL reads them). The renderer uploads it,
 * with mipmaps, linear filtering and repeat wrapping, the first time a material that uses it is drawn.
 *
 * Plain C++ shared through TSharedPtr and FResourceCache (FMaterial holds its maps as TSharedPtr<UTexture2D>) until
 * P14 makes it a UObject asset.
 */
class ENGINE_API UTexture2D
{
public:
	/** A texture of Width x Height RGBA8 pixels; empty (not Valid) for a null or empty image. */
	[[nodiscard]] static UTexture2D Create(int32 Width, int32 Height, const uint8* Rgba);
	/** A grey checker with 8 cells per side (the default material's map). */
	[[nodiscard]] static UTexture2D CreateChecker(int32 Size = 64);
	/** Flat normal map in tangent space (points along +Z). */
	[[nodiscard]] static UTexture2D CreateFlatNormal(int32 Size = 4);
	/** Strong procedural bumps for demo normal mapping (tileable). */
	[[nodiscard]] static UTexture2D CreateBumpNormal(int32 Size = 256);
	/** Decodes an image file (PNG, JPEG, TGA, ... through stb_image), flipped so the bottom row comes first. */
	[[nodiscard]] static UTexture2D LoadFromFile(const FString& Path);

	[[nodiscard]] bool Valid() const
	{
		return SizeX > 0 && SizeY > 0;
	}
	/** UE: GetSizeX / GetSizeY. */
	[[nodiscard]] int32 GetSizeX() const
	{
		return SizeX;
	}
	[[nodiscard]] int32 GetSizeY() const
	{
		return SizeY;
	}
	/** SizeX * SizeY RGBA8 pixels, bottom row first. */
	[[nodiscard]] const TArray<uint8>& GetPixels() const
	{
		return Pixels;
	}

private:
	int32 SizeX = 0;
	int32 SizeY = 0;
	TArray<uint8> Pixels;
};
