#include "LDRColorTarget.h"

#include "RendererLog.h"

#include <glad/glad.h>

FLDRColorTarget::~FLDRColorTarget()
{
	Destroy();
}

bool FLDRColorTarget::EnsureSize(int InWidth, int InHeight)
{
	if (InWidth < 1 || InHeight < 1)
	{
		return false;
	}
	if (Valid() && Width == InWidth && Height == InHeight)
	{
		return true;
	}

	Destroy();
	Width = InWidth;
	Height = InHeight;

	glGenFramebuffers(1, &Fbo);
	glGenTextures(1, &ColorTexture);

	glBindTexture(GL_TEXTURE_2D, ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, Width, Height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTexture, 0);

	const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (Status != GL_FRAMEBUFFER_COMPLETE)
	{
		UE_LOG(LogRenderer, Error, "LdrColorTarget framebuffer incomplete");
		Destroy();
		return false;
	}
	return true;
}

void FLDRColorTarget::Destroy()
{
	if (ColorTexture != 0)
	{
		glDeleteTextures(1, &ColorTexture);
		ColorTexture = 0;
	}
	if (Fbo != 0)
	{
		glDeleteFramebuffers(1, &Fbo);
		Fbo = 0;
	}
	Width = 0;
	Height = 0;
}

void FLDRColorTarget::BindWrite() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
	glViewport(0, 0, Width, Height);
}

void FLDRColorTarget::BindColorTexture(unsigned int Unit) const
{
	glActiveTexture(GL_TEXTURE0 + Unit);
	glBindTexture(GL_TEXTURE_2D, ColorTexture);
}
