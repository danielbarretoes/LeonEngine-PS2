#include "Texture2DResource.h"

#include "Engine/Texture2D.h"

#include <glad/glad.h>

FTexture2DResource::FTexture2DResource(const UTexture2D& Texture)
{
	if (!Texture.HasValidPlatformData())
	{
		return;
	}
	const FByteBulkData& BulkData = Texture.GetPlatformData().Mips[0].BulkData;
	const uint32 Format = Texture.GetPixelFormat() == PF_B8G8R8A8 ? GL_BGRA : GL_RGBA;
	Create(Texture.GetSizeX(), Texture.GetSizeY(), Format, BulkData.LockReadOnly());
	BulkData.Unlock();
}

FTexture2DResource::FTexture2DResource(int32 SizeX, int32 SizeY, const uint8* Rgba)
{
	if (SizeX > 0 && SizeY > 0 && Rgba != nullptr)
	{
		Create(SizeX, SizeY, GL_RGBA, Rgba);
	}
}

void FTexture2DResource::Create(int32 SizeX, int32 SizeY, uint32 Format, const void* Texels)
{
	glGenTextures(1, &Id);
	glBindTexture(GL_TEXTURE_2D, Id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SizeX, SizeY, 0, Format, GL_UNSIGNED_BYTE, Texels);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);
}

FTexture2DResource::~FTexture2DResource()
{
	if (Id != 0)
	{
		glDeleteTextures(1, &Id);
	}
}

void FTexture2DResource::Bind(uint32 Unit) const
{
	glActiveTexture(GL_TEXTURE0 + Unit);
	glBindTexture(GL_TEXTURE_2D, Id);
}
