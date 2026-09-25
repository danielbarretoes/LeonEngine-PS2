#include "Texture2DResource.h"

#include "Engine/Texture2D.h"

#include <glad/glad.h>

FTexture2DResource::FTexture2DResource(const UTexture2D& Texture)
{
	if (!Texture.Valid())
	{
		return;
	}
	glGenTextures(1, &Id);
	glBindTexture(GL_TEXTURE_2D, Id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Texture.GetSizeX(), Texture.GetSizeY(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
		Texture.GetPixels().GetData());
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
