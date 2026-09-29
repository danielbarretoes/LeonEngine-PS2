#include "Engine/Texture2D.h"

#include "EngineLogs.h"
#include "HAL/LowLevelMemTracker.h"
#include "UObject/Package.h"

UTexture2D::UTexture2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UTexture2D* UTexture2D::CreateTransient(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, FName InName)
{
	if (GetPixelFormatDataSize(InFormat, InSizeX, InSizeY) == 0)
	{
		UE_LOG(LogEngine, Warning, "UTexture2D::CreateTransient: invalid size %dx%d or format %d", InSizeX, InSizeY,
			static_cast<int32>(InFormat));
		return nullptr;
	}
	UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage(), InName, RF_Transient);
	(void)Texture->SetPlatformData(InSizeX, InSizeY, InFormat, nullptr);
	return Texture;
}

bool UTexture2D::SetPlatformData(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, const void* TexelData)
{
	const int64 NumBytes = GetPixelFormatDataSize(InFormat, InSizeX, InSizeY);
	if (NumBytes == 0)
	{
		return false;
	}
	PlatformData.SizeX = InSizeX;
	PlatformData.SizeY = InSizeY;
	PlatformData.PixelFormat = InFormat;
	PlatformData.Mips.Empty(1);
	FTexture2DMipMap& Mip = PlatformData.Mips.AddDefaulted_GetRef();
	Mip.SizeX = InSizeX;
	Mip.SizeY = InSizeY;
	Mip.BulkData.SetPayloadAlignment(GetPixelFormatDataAlignment(InFormat));
	(void)Mip.BulkData.Lock(LOCK_READ_WRITE);
	void* Data = Mip.BulkData.Realloc(NumBytes);
	if (TexelData != nullptr)
	{
		FMemory::Memcpy(Data, TexelData, static_cast<SIZE_T>(NumBytes));
	}
	else
	{
		FMemory::Memzero(Data, static_cast<SIZE_T>(NumBytes));
	}
	Mip.BulkData.Unlock();
	UpdateResource();
	return true;
}

bool UTexture2D::AddMip(const void* TexelData)
{
	if (PlatformData.Mips.Num() == 0 || TexelData == nullptr)
	{
		return false;
	}
	const FTexture2DMipMap& Last = PlatformData.Mips.Last();
	if (Last.SizeX <= 1 && Last.SizeY <= 1)
	{
		return false;
	}
	const int32 MipSizeX = FMath::Max(1, Last.SizeX / 2);
	const int32 MipSizeY = FMath::Max(1, Last.SizeY / 2);
	const int64 NumBytes =
		GetPixelFormatMipDataSize(PlatformData.PixelFormat, MipSizeX, MipSizeY, PlatformData.Mips.Num());
	if (NumBytes == 0)
	{
		return false;
	}
	FTexture2DMipMap& Mip = PlatformData.Mips.AddDefaulted_GetRef();
	Mip.SizeX = MipSizeX;
	Mip.SizeY = MipSizeY;
	Mip.BulkData.SetPayloadAlignment(GetPixelFormatDataAlignment(PlatformData.PixelFormat));
	(void)Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Mip.BulkData.Realloc(NumBytes), TexelData, static_cast<SIZE_T>(NumBytes));
	Mip.BulkData.Unlock();
	UpdateResource();
	return true;
}

bool UTexture2D::HasValidPlatformData() const
{
	const int64 NumBytes = GetPixelFormatDataSize(PlatformData.PixelFormat, PlatformData.SizeX, PlatformData.SizeY);
	if (NumBytes == 0 || PlatformData.Mips.Num() == 0)
	{
		return false;
	}
	const FTexture2DMipMap& Mip = PlatformData.Mips[0];
	return Mip.SizeX == PlatformData.SizeX && Mip.SizeY == PlatformData.SizeY &&
		Mip.BulkData.GetBulkDataSize() == NumBytes;
}

void UTexture2D::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Textures);
	Super::Serialize(Ar);
	PlatformData.Serialize(Ar, this);
}
