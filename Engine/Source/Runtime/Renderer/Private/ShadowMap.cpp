#include "ShadowMap.h"

#include <glad/glad.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

FShadowMap::~FShadowMap()
{
	Destroy();
}

bool FShadowMap::Create(int InSize)
{
	Destroy();
	InSize = std::max(InSize, 64);
	Size = InSize;

	glGenFramebuffers(1, &Fbo);
	glGenTextures(1, &DepthTexture);

	glBindTexture(GL_TEXTURE_2D, DepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, Size, Size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const std::array<float, 4> Border = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, Border.data());

	glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (Status != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "ShadowMap framebuffer incomplete\n";
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

glm::mat4 FShadowMap::FitLightSpaceMatrix(
	const glm::vec3& LightDirection, const glm::vec3& WorldMin, const glm::vec3& WorldMax, float Padding)
{
	glm::vec3 Dir = LightDirection;
	if (glm::dot(Dir, Dir) < 1e-8f)
	{
		Dir = {0.35f, -1.0f, -0.45f};
	}
	Dir = glm::normalize(Dir);

	const glm::vec3 Center = (WorldMin + WorldMax) * 0.5f;
	const glm::vec3 Extents = (WorldMax - WorldMin) * 0.5f + glm::vec3(Padding);
	const float Radius = glm::length(Extents);

	glm::vec3 Up{0.0f, 1.0f, 0.0f};
	if (std::abs(glm::dot(Dir, Up)) > 0.95f)
	{
		Up = {0.0f, 0.0f, 1.0f};
	}

	const glm::vec3 Eye = Center - (Dir * (Radius + 1.0f));
	const glm::mat4 LightView = glm::lookAt(Eye, Center, Up);

	glm::vec3 MinLs(std::numeric_limits<float>::max());
	glm::vec3 MaxLs(std::numeric_limits<float>::lowest());

	const std::array<glm::vec3, 8> Corners = {{
		{WorldMin.x, WorldMin.y, WorldMin.z},
		{WorldMax.x, WorldMin.y, WorldMin.z},
		{WorldMin.x, WorldMax.y, WorldMin.z},
		{WorldMax.x, WorldMax.y, WorldMin.z},
		{WorldMin.x, WorldMin.y, WorldMax.z},
		{WorldMax.x, WorldMin.y, WorldMax.z},
		{WorldMin.x, WorldMax.y, WorldMax.z},
		{WorldMax.x, WorldMax.y, WorldMax.z},
	}};

	for (const glm::vec3& Corner : Corners)
	{
		const glm::vec3 Ls = glm::vec3(LightView * glm::vec4(Corner, 1.0f));
		MinLs = glm::min(MinLs, Ls);
		MaxLs = glm::max(MaxLs, Ls);
	}

	// Eye-space Z is negative in front of the light camera.
	const float ZNear = std::max(0.05f, -MaxLs.z + Padding);
	const float ZFar = std::max(ZNear + 0.1f, -MinLs.z + Padding);

	const glm::mat4 LightProj =
		glm::ortho(MinLs.x - Padding, MaxLs.x + Padding, MinLs.y - Padding, MaxLs.y + Padding, ZNear, ZFar);
	return LightProj * LightView;
}
