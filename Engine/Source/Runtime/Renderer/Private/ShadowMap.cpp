#include "ShadowMap.h"

#include "GLClipSpace.h"
#include "RendererLog.h"
#include "ViewMatrices.h"

#include <glad/glad.h>

FShadowMap::~FShadowMap()
{
	Destroy();
}

bool FShadowMap::Create(int InSize)
{
	Destroy();
	InSize = FMath::Max(InSize, 64);
	Size = InSize;

	glGenFramebuffers(1, &Fbo);
	glGenTextures(1, &DepthTexture);

	glBindTexture(GL_TEXTURE_2D, DepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, Size, Size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float Border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, Border);

	glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (Status != GL_FRAMEBUFFER_COMPLETE)
	{
		UE_LOG(LogRenderer, Error, "ShadowMap framebuffer incomplete");
		Destroy();
		return false;
	}
	return true;
}

void FShadowMap::Destroy()
{
	if (DepthTexture != 0)
	{
		glDeleteTextures(1, &DepthTexture);
		DepthTexture = 0;
	}
	if (Fbo != 0)
	{
		glDeleteFramebuffers(1, &Fbo);
		Fbo = 0;
	}
	Size = 0;
}

void FShadowMap::Begin() const
{
	glViewport(0, 0, Size, Size);
	glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
	glClear(GL_DEPTH_BUFFER_BIT);
	// No face cull so one-sided casters (planes, cards) still write depth.
	glDisable(GL_CULL_FACE);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 2.0f);
}

void FShadowMap::End(int FramebufferWidth, int FramebufferHeight, unsigned int RestoreFbo) const
{
	glDisable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glBindFramebuffer(GL_FRAMEBUFFER, RestoreFbo);
	glViewport(0, 0, FramebufferWidth, FramebufferHeight);
}

void FShadowMap::BindDepthTexture(unsigned int Unit) const
{
	glActiveTexture(GL_TEXTURE0 + Unit);
	glBindTexture(GL_TEXTURE_2D, DepthTexture);
}
