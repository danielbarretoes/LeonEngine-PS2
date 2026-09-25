#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "TextureFactory.generated.h"

class UTexture2D;

/** How the imported texels are read (UE: ETextureSourceColorSpace). */
UENUM()
enum class ETextureSourceColorSpace : uint8
{
	/** Leon: linear for a name ending in `_N` or `_Normal` (UE's normal map suffixes), sRGB otherwise. */
	Auto,
	Linear,
	SRGB,
};

/**
 * Imports images as UTexture2D (UE: UTextureFactory): PNG, JPEG, TGA and BMP through stb_image, the only place in the
 * engine that decodes image files. The texels are stored as PF_R8G8B8A8, bottom row first (what the renderer uploads);
 * UTexture::SRGB comes from ColorSpaceMode. Leon keeps no texture source and compresses nothing.
 */
UCLASS()
class LEONED_API UTextureFactory
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UTextureFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** sRGB or linear texels (UE: ColorSpaceMode). ImportList.ini: `ColorSpaceMode=Linear`. */
	UPROPERTY()
	ETextureSourceColorSpace ColorSpaceMode = ETextureSourceColorSpace::Auto;

	UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context,
		const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled) override;

	/** Decodes an image file into RGBA8 texels, bottom row first; false with an error (Leon). */
	static bool DecodeImage(
		const uint8* Buffer, int64 Size, int32& OutWidth, int32& OutHeight, TArray<uint8>& OutRGBA, FString& OutError);

	/** True when a texture named Name is read as sRGB under ColorSpaceMode (Leon). */
	[[nodiscard]] bool IsSRGB(FName Name) const;

	// FReimportHandler
	bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	EReimportResult::Type Reimport(UObject* Obj) override;
	void GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const override;
};
