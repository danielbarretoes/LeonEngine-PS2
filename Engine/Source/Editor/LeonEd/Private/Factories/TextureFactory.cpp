#include "Factories/TextureFactory.h"

#include "Engine/Texture2D.h"
#include "LeonEdLog.h"

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable : 4244 4456 4457 4505 4702) // stb_image's conversions, shadowing, unused and dead code
#endif
// The engine's only image decoder: internal to this file, from memory only.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#include <stb_image.h>
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

UTextureFactory::UTextureFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UTexture2D::StaticClass();
	Formats.Add(TEXT("png;PNG image"));
	Formats.Add(TEXT("jpg;JPEG image"));
	Formats.Add(TEXT("jpeg;JPEG image"));
	Formats.Add(TEXT("tga;Targa image"));
	Formats.Add(TEXT("bmp;Bitmap image"));
	bEditorImport = 1;
}

bool UTextureFactory::DecodeImage(
	const uint8* Buffer, int64 Size, int32& OutWidth, int32& OutHeight, TArray<uint8>& OutRGBA, FString& OutError)
{
	if (Buffer == nullptr || Size <= 0 || Size > static_cast<int64>(MAX_int32))
	{
		OutError = TEXT("empty image");
		return false;
	}
	// The renderer uploads the bottom row first (OpenGL's order).
	stbi_set_flip_vertically_on_load(1);
	int32 Channels = 0;
	uint8* Pixels =
		stbi_load_from_memory(Buffer, static_cast<int32>(Size), &OutWidth, &OutHeight, &Channels, STBI_rgb_alpha);
	if (Pixels == nullptr)
	{
		OutError = stbi_failure_reason();
		return false;
	}
	OutRGBA.Reset();
	OutRGBA.Append(Pixels, OutWidth * OutHeight * 4);
	stbi_image_free(Pixels);
	return OutWidth > 0 && OutHeight > 0;
}

bool UTextureFactory::IsSRGB(FName Name) const
{
	switch (ColorSpaceMode)
	{
		case ETextureSourceColorSpace::Linear:
			return false;
		case ETextureSourceColorSpace::SRGB:
			return true;
		case ETextureSourceColorSpace::Auto:
			break;
	}
	const FString NameString = Name.ToString();
	return !NameString.EndsWith(TEXT("_N")) && !NameString.EndsWith(TEXT("_Normal"));
}

UObject* UTextureFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled)
{
	(void)InClass;
	(void)Context;
	(void)Type;
	bOutOperationCanceled = false;
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> Texels;
	FString Error;
	if (!DecodeImage(Buffer, BufferEnd - Buffer, Width, Height, Texels, Error))
	{
		UE_LOG(LogLeonEd, Error, "TextureFactory: cannot decode '%s' (%s)", *CurrentFilename, *Error);
		return nullptr;
	}
	UTexture2D* Texture = CreateOrOverwriteAsset<UTexture2D>(InParent, InName, Flags);
	if (Texture == nullptr)
	{
		return nullptr;
	}
	Texture->SRGB = IsSRGB(InName) ? 1 : 0;
	(void)Texture->SetPlatformData(Width, Height, PF_R8G8B8A8, Texels.GetData());
	UpdateAssetImportData(Texture, CurrentFilename);
	Buffer = BufferEnd;
	return Texture;
}

bool UTextureFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return FactoryCanReimport(Obj, OutFilenames);
}

void UTextureFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	FactorySetReimportPaths(Obj, NewReimportPaths);
}

EReimportResult::Type UTextureFactory::Reimport(UObject* Obj)
{
	return FactoryReimport(Obj);
}

void UTextureFactory::GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
{
	FactoryGetAdditionalReimportedObjects(OutObjects);
}
