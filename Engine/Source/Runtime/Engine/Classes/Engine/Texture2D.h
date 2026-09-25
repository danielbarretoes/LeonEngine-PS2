#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "Texture2D.generated.h"

/**
 * A 2D texture asset (UE: UTexture2D): its texels in the platform data, mip 0 as bulk data, bottom row first (as
 * OpenGL reads them). The renderer uploads it with mipmaps, trilinear filtering and repeat wrapping, and keeps the GPU
 * copy.
 *
 * Made by CreateTransient (UE) or NewObject followed by SetPlatformData, and saved and loaded in `.lasset` packages.
 */
UCLASS()
class ENGINE_API UTexture2D : public UTexture
{
	GENERATED_BODY()

public:
	UTexture2D(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * A texture made at run time in the transient package (UE: CreateTransient): InSizeX x InSizeY zeroed texels of
	 * InFormat in mip 0. Null for an empty size or a format Leon cannot store. Leon's default format is PF_R8G8B8A8,
	 * the texel order of its textures (UE: PF_B8G8R8A8).
	 */
	[[nodiscard]] static UTexture2D* CreateTransient(
		int32 InSizeX, int32 InSizeY, EPixelFormat InFormat = PF_R8G8B8A8, FName InName = NAME_None);

	/**
	 * Replaces the texels with InSizeX x InSizeY texels of InFormat copied from TexelData (bottom row first); a null
	 * TexelData leaves them zeroed. False (and nothing changed) for an empty size or an unknown format (Leon; UE
	 * fills PlatformData->Mips[0].BulkData by hand).
	 */
	bool SetPlatformData(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, const void* TexelData);

	/** Width of mip 0 (UE: GetSizeX). */
	[[nodiscard]] int32 GetSizeX() const
	{
		return PlatformData.SizeX;
	}
	/** Height of mip 0 (UE: GetSizeY). */
	[[nodiscard]] int32 GetSizeY() const
	{
		return PlatformData.SizeY;
	}
	/** UE: GetPixelFormat. */
	[[nodiscard]] EPixelFormat GetPixelFormat() const
	{
		return PlatformData.PixelFormat;
	}
	/** UE: GetNumMips. */
	[[nodiscard]] int32 GetNumMips() const
	{
		return PlatformData.Mips.Num();
	}
	/** True when mip 0 holds every texel of the size and format (Leon: something the renderer can upload). */
	[[nodiscard]] bool HasValidPlatformData() const;

	/** The texels (UE: GetPlatformData). */
	[[nodiscard]] const FTexturePlatformData& GetPlatformData() const
	{
		return PlatformData;
	}
	[[nodiscard]] FTexturePlatformData& GetPlatformData()
	{
		return PlatformData;
	}

	float GetSurfaceWidth() const override
	{
		return static_cast<float>(GetSizeX());
	}
	float GetSurfaceHeight() const override
	{
		return static_cast<float>(GetSizeY());
	}

	/** The tagged properties, then the platform data (its mips' texels as bulk data). */
	void Serialize(FArchive& Ar) override;

private:
	FTexturePlatformData PlatformData;
};
