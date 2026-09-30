#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Engine/TextureCube.h"
#include "Factories/Factory.h"
#include "TextureCubeFactory.generated.h"

/**
 * Imports a high dynamic range long-lat image (Radiance `.hdr`) as a UTextureCube (UE: UTextureFactory imports an
 * `.hdr` as a cube map from its long-lat source; Leon, Docs/PLANS/ps2-polish.md P8). The GS shows bytes, so the import
 * bakes the display in (Docs/ASSET_FORMATS.md, "Cube maps"):
 *
 * - each face texel of CubeFaceSize x CubeFaceSize looks along its direction (UTextureCube::GetFaceDirection) and
 *   averages four bilinear samples of the panorama there (column x at the yaw (x + 0.5) / Width x 360 - 180 degrees,
 *   the middle column along +X; row y, top first, at the pitch 90 - (y + 0.5) / Height x 180 degrees);
 * - the radiance, times 2^ExposureBias, is tone-mapped by the ACES filmic curve (Krzysztof Narkowicz's fit,
 *   x (2.51 x + 0.03) / (x (2.43 x + 0.59) + 0.14), per channel, clamped to 0..1), then encoded to sRGB bytes;
 * - the faces are the cube's UTexture2D subobjects (`PosX` ... `NegZ`, PF_R8G8B8A8, sRGB, opaque), which the PS2 cook
 *   palettes (PSMT8); HorizonColor is the average of the side faces' texels within HorizonDegrees above the horizon.
 *
 * The same file and settings give the same bytes (gate G5); ImportList.ini keys set the options.
 */
UCLASS()
class LEONED_API UTextureCubeFactory
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UTextureCubeFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The side of each face, texels: a power of two from 8 to 256 (the GS's textures). */
	UPROPERTY()
	int32 CubeFaceSize = 128;

	/** Stops of exposure before the tone mapping (the radiance times 2^ExposureBias; UE: a post process's
	 * ExposureBias). */
	UPROPERTY()
	float ExposureBias = 0.0f;

	/** How high above the horizon the texels HorizonColor averages reach, degrees. */
	UPROPERTY()
	float HorizonDegrees = 5.0f;

	UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context,
		const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled) override;

	/** The ACES filmic tone curve (Narkowicz's fit) of a linear value, clamped to 0..1 (Leon). */
	[[nodiscard]] static float ToneMapACES(float Value);

	/**
	 * Fills Cube from a Width x Height long-lat panorama of linear RGB (three floats a texel, the top row first) with
	 * the factory's settings: the six faces and HorizonColor (Leon). False, with an error, for a bad size.
	 */
	bool FillCube(
		UTextureCube& Cube, int32 Width, int32 Height, const TArray<float>& Radiance, FString& OutError) const;

	// FReimportHandler
	bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	EReimportResult::Type Reimport(UObject* Obj) override;
	void GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const override;
};
