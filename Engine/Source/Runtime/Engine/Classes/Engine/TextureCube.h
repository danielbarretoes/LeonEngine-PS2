#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "TextureCube.generated.h"

class UTexture2D;

/** The faces of a cube map, in UE's order (UE: ECubeFace of RHIDefinitions.h, CubeFace_PosX ... CubeFace_MAX). */
enum class ECubeFace : uint8
{
	PosX,
	NegX,
	PosY,
	NegY,
	PosZ,
	NegZ,
	Num
};

/**
 * A cube map (UE: UTextureCube): six square faces around a point, what a sky is drawn from (Docs/PLANS/ps2-polish.md
 * P8). LeonEd's UTextureCubeFactory makes one from a long-lat HDR image, tone-mapped to sRGB bytes
 * (Docs/ASSET_FORMATS.md, "Cube maps").
 *
 * The GS samples no cube: each face is a UTexture2D subobject of the cube (`PosX` ... `NegZ`), which the PS2 cook
 * palettes as any texture and the GS texture cache binds on its own (UE keeps the six faces as the slices of one
 * platform data). A face is seen from inside the cube: face F holds the directions Forward + u Right + v Up
 * (GetFaceBasis) for u and v from -1 to 1, its texel columns going along Right and its rows along Up, the bottom row
 * first (as every texture's texels), in the world's axes (X forward, Y right, Z up).
 */
UCLASS()
class ENGINE_API UTextureCube : public UTexture
{
	GENERATED_BODY()

public:
	UTextureCube(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The six faces in ECubeFace's order, each a square UTexture2D of the same size (Leon). */
	UPROPERTY()
	TArray<UTexture2D*> Faces;

	/**
	 * The colour of the horizon as the faces store it (sRGB-encoded, 1 = 255): the average of the texels within a few
	 * degrees above it, all around. What a map's fog takes when it follows the sky (FWorldFogSettings; Leon).
	 */
	UPROPERTY()
	FLinearColor HorizonColor = FLinearColor::White;

	/** A face's name, its subobject's (`PosX`, `NegX`, `PosY`, `NegY`, `PosZ`, `NegZ`). */
	[[nodiscard]] static const TCHAR* GetFaceName(ECubeFace Face);

	/**
	 * The directions of a face, seen from the cube's centre: Forward (its centre), Right (its texel columns) and Up
	 * (its rows), each a world axis. The side faces keep +Z up (Right = Up ^ Forward: +X's right is +Y); +Z's up is
	 * -X and -Z's is +X, as if the view pitched up or down from +X.
	 */
	static void GetFaceBasis(ECubeFace Face, FVector& OutForward, FVector& OutRight, FVector& OutUp);

	/** The unit direction through a face at (U, V), each from 0 (left, bottom) to 1 (right, top). */
	[[nodiscard]] static FVector GetFaceDirection(ECubeFace Face, float U, float V);

	/** A face (null when the cube has none). */
	[[nodiscard]] UTexture2D* GetFace(ECubeFace Face) const
	{
		return Faces.IsValidIndex(int32(Face)) ? Faces[int32(Face)] : nullptr;
	}

	/** True with six faces, each square, of one size, with texels the renderer can upload (Leon). */
	[[nodiscard]] bool HasValidFaces() const;

	/** The faces' side, texels (UE: GetSizeX); 0 without faces. */
	[[nodiscard]] int32 GetSizeX() const;
	[[nodiscard]] int32 GetSizeY() const
	{
		return GetSizeX();
	}

	float GetSurfaceWidth() const override
	{
		return static_cast<float>(GetSizeX());
	}
	float GetSurfaceHeight() const override
	{
		return static_cast<float>(GetSizeY());
	}
};
