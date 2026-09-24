#include "PostProcess.h"

#include <glad/glad.h>

#include <iostream>

FSSAOTarget::~FSSAOTarget()
{
	Destroy();
}

bool FSSAOTarget::EnsureSize(int InWidth, int InHeight)
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

	for (int I = 0; I < 2; ++I)
	{
		glGenFramebuffers(1, &Fbo[I]);
		glGenTextures(1, &Color[I]);
		glBindTexture(GL_TEXTURE_2D, Color[I]);
		// R16F avoids contour banding that R8 + aoPower/tonemap makes visible on flat materials.
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, Width, Height, 0, GL_RED, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, Fbo[I]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Color[I], 0);
		const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (Status != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cerr << "SsaoTarget framebuffer incomplete\n";
			Destroy();
			return false;
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

void FSSAOTarget::Destroy()
{
	for (int I = 0; I < 2; ++I)
	{
		if (Color[I] != 0)
		{
			glDeleteTextures(1, &Color[I]);
			Color[I] = 0;
		}
		if (Fbo[I] != 0)
		{
			glDeleteFramebuffers(1, &Fbo[I]);
			Fbo[I] = 0;
		}
	}
	Width = 0;
	Height = 0;
}

void FSSAOTarget::BindWrite(int Index) const
{
	const int I = (Index == 0) ? 0 : 1;
	glBindFramebuffer(GL_FRAMEBUFFER, Fbo[I]);
	glViewport(0, 0, Width, Height);
}

void FSSAOTarget::BindColorTexture(int Index, unsigned int Unit) const
{
	const int I = (Index == 0) ? 0 : 1;
	glActiveTexture(GL_TEXTURE0 + Unit);
	glBindTexture(GL_TEXTURE_2D, Color[I]);
}
