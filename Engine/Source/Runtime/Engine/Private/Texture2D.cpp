#include "Engine/Texture2D.h"

#include "EngineLogs.h"
#include "UObject/Package.h"

UTexture2D::UTexture2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UTexture2D* UTexture2D::CreateTransient(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, FName InName)
{
	if (InSizeX <= 0 || InSizeY <= 0 || GetPixelFormatBytes(InFormat) == 0)
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
	const int32 TexelBytes = GetPixelFormatBytes(InFormat);
	if (InSizeX <= 0 || InSizeY <= 0 || TexelBytes == 0)
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
	const int64 NumBytes = static_cast<int64>(InSizeX) * InSizeY * TexelBytes;
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

bool UTexture2D::HasValidPlatformData() const
{
	const int32 TexelBytes = GetPixelFormatBytes(PlatformData.PixelFormat);
	if (PlatformData.SizeX <= 0 || PlatformData.SizeY <= 0 || TexelBytes == 0 || PlatformData.Mips.Num() == 0)
	{
		return false;
	}
	const FTexture2DMipMap& Mip = PlatformData.Mips[0];
	return Mip.SizeX == PlatformData.SizeX && Mip.SizeY == PlatformData.SizeY &&
		Mip.BulkData.GetBulkDataSize() == static_cast<int64>(Mip.SizeX) * Mip.SizeY * TexelBytes;
}

void UTexture2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	PlatformData.Serialize(Ar, this);
}
