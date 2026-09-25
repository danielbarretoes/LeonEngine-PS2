#include "Engine/Texture.h"

void FTexture2DMipMap::Serialize(FArchive& Ar, UObject* Owner, int32 MipIndex)
{
	Ar << SizeX << SizeY;
	BulkData.Serialize(Ar, Owner, MipIndex);
}

void FTexturePlatformData::Serialize(FArchive& Ar, UTexture* Owner)
{
	uint8 Format = static_cast<uint8>(PixelFormat);
	int32 NumMips = Mips.Num();
	Ar << SizeX << SizeY << Format << NumMips;
	if (Ar.IsLoading())
	{
		PixelFormat = static_cast<EPixelFormat>(Format);
		if (NumMips < 0 || Ar.IsError())
		{
			Ar.SetCriticalError();
			Mips.Empty();
			return;
		}
		Mips.Empty(NumMips);
		Mips.AddDefaulted(NumMips);
	}
	for (int32 MipIndex = 0; MipIndex < Mips.Num() && !Ar.IsError(); ++MipIndex)
	{
		Mips[MipIndex].Serialize(Ar, Owner, MipIndex);
	}
}

UTexture::UTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SRGB = 1;
}
