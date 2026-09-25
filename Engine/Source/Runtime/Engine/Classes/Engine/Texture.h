#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "Serialization/BulkData.h"
#include "UObject/Object.h"
#include "Texture.generated.h"

class UTexture;

/** One mip level of a texture (UE: FTexture2DMipMap, TextureResource.h): its size and its texels as bulk data. */
struct ENGINE_API FTexture2DMipMap
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	/** SizeX * SizeY texels of the platform data's pixel format, bottom row first (as OpenGL reads them). */
	FByteBulkData BulkData;

	/** The size, then the texels as bulk data (at the end of a package file, D13). */
	void Serialize(FArchive& Ar, UObject* Owner, int32 MipIndex);
};

/**
 * The texels of a texture as the renderer uploads them (UE: FTexturePlatformData): the size, the pixel format and the
 * mips. Leon stores mip 0 only; the renderer makes the others when it uploads the texture (glGenerateMipmap), as it
 * did before textures were assets.
 */
struct ENGINE_API FTexturePlatformData
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	EPixelFormat PixelFormat = PF_Unknown;
	TArray<FTexture2DMipMap> Mips;

	/** The size, the format and every mip (UE: FTexturePlatformData::Serialize). */
	void Serialize(FArchive& Ar, UTexture* Owner);
};

/**
 * The base of the texture assets (UE: UTexture, Engine/Classes/Engine/Texture.h). The texels live in the subclass's
 * platform data (bulk data in a package); the renderer keeps the GPU copy, which it makes the first time a material
 * that uses the texture is drawn.
 *
 * Leon has no texture source (the editor-only FTextureSource and its import settings come with the editor module) and
 * no compression, LOD groups or streaming.
 */
UCLASS(Abstract)
class ENGINE_API UTexture : public UObject
{
	GENERATED_BODY()

public:
	UTexture(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * The texels are sRGB-encoded colours (UE: SRGB); off for normal maps and other data. Recorded for the importer
	 * and the cook: the forward renderer uploads every texture as it is stored (Leon).
	 */
	UPROPERTY()
	uint8 SRGB : 1;

	/** Width of the texture in texels (UE: GetSurfaceWidth). */
	[[nodiscard]] virtual float GetSurfaceWidth() const
	{
		return 0.0f;
	}
	/** Height of the texture in texels (UE: GetSurfaceHeight). */
	[[nodiscard]] virtual float GetSurfaceHeight() const
	{
		return 0.0f;
	}
};
