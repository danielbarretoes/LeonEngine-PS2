#include "Debug/DebugDraw.h"

#include "Migration/LegacyContentPath.h"
#include "OpenGLVertexAttrib.h"

#include <glad/glad.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <utility>

bool FDebugDraw::Initialize(const std::string& /*shaderDirectory*/)
{
	const std::string Vert = ResolveLegacyContentPath("assets/Shaders/debug_line.vert");
	const std::string Frag = ResolveLegacyContentPath("assets/Shaders/debug_line.frag");
	if (!Shader.LoadFromFiles(Vert, Frag))
	{
		std::cerr << "Failed to load debug line shaders\n";
		return false;
	}

	glGenVertexArrays(1, &Vao);
	glGenBuffers(1, &Vbo);
	glBindVertexArray(Vao);
	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FVertex), GlAttribOffset(&FVertex::Position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FVertex), GlAttribOffset(&FVertex::Color));
	glBindVertexArray(0);
	return true;
}

EShaderReloadResult FDebugDraw::ReloadShader(bool bForce)
{
	return bForce ? Shader.ForceReloadFromDisk() : Shader.ReloadFromDiskIfChanged();
}

void FDebugDraw::Shutdown()
{
	if (Vbo != 0)
	{
		glDeleteBuffers(1, &Vbo);
		Vbo = 0;
	}
	if (Vao != 0)
	{
		glDeleteVertexArrays(1, &Vao);
		Vao = 0;
	}
	Shader.Destroy();
	Vertices.clear();
}

void FDebugDraw::Clear()
{
	Vertices.clear();
}

void FDebugDraw::AddLine(const glm::vec3& A, const glm::vec3& B, const glm::vec3& InColor)
{
	Vertices.push_back(FVertex{.Position = A, .Color = InColor});
	Vertices.push_back(FVertex{.Position = B, .Color = InColor});
}

void FDebugDraw::AddArrow(
	const glm::vec3& From, const glm::vec3& To, const glm::vec3& InColor, float HeadLength, float HeadWidth)
{
	AddLine(From, To, InColor);

	const glm::vec3 Shaft = To - From;
	const float Len = glm::length(Shaft);
	if (Len < 1.0e-4f)
	{
		return;
	}
	const glm::vec3 Dir = Shaft / Len;
	glm::vec3 Side = glm::cross(Dir, glm::vec3{0.0f, 1.0f, 0.0f});
	if (glm::dot(Side, Side) < 1.0e-6f)
	{
		Side = glm::cross(Dir, glm::vec3{1.0f, 0.0f, 0.0f});
	}
	Side = glm::normalize(Side) * HeadWidth;
	const glm::vec3 Back = To - (Dir * HeadLength);
	AddLine(To, Back + Side, InColor);
	AddLine(To, Back - Side, InColor);
}

void FDebugDraw::AddAabb(const glm::vec3& WorldMin, const glm::vec3& WorldMax, const glm::vec3& InColor)
{
	const glm::vec3& Mn = WorldMin;
	const glm::vec3& Mx = WorldMax;
	const std::array<glm::vec3, 8> C = {{
		{Mn.x, Mn.y, Mn.z},
		{Mx.x, Mn.y, Mn.z},
		{Mx.x, Mx.y, Mn.z},
		{Mn.x, Mx.y, Mn.z},
		{Mn.x, Mn.y, Mx.z},
		{Mx.x, Mn.y, Mx.z},
		{Mx.x, Mx.y, Mx.z},
		{Mn.x, Mx.y, Mx.z},
	}};

	const std::array<std::pair<int, int>, 12> Edges = {{
		{0, 1},
		{1, 2},
		{2, 3},
		{3, 0},
		{4, 5},
		{5, 6},
		{6, 7},
		{7, 4},
		{0, 4},
		{1, 5},
		{2, 6},
		{3, 7},
	}};
	for (const auto& [i, j] : Edges)
	{
		AddLine(C[static_cast<std::size_t>(i)], C[static_cast<std::size_t>(j)], InColor);
	}
}

void FDebugDraw::AddAxes(const glm::vec3& Origin, float Size)
{
	AddLine(Origin, Origin + glm::vec3{Size, 0.0f, 0.0f}, {1.0f, 0.2f, 0.2f});
	AddLine(Origin, Origin + glm::vec3{0.0f, Size, 0.0f}, {0.2f, 1.0f, 0.2f});
	AddLine(Origin, Origin + glm::vec3{0.0f, 0.0f, Size}, {0.2f, 0.4f, 1.0f});
}

void FDebugDraw::AddLightFrustum(const glm::mat4& LightSpace, const glm::vec3& InColor)
{
	const glm::mat4 Inv = glm::inverse(LightSpace);
	const std::array<glm::vec3, 8> Ndc = {{
		{-1.0f, -1.0f, -1.0f},
		{1.0f, -1.0f, -1.0f},
		{1.0f, 1.0f, -1.0f},
		{-1.0f, 1.0f, -1.0f},
		{-1.0f, -1.0f, 1.0f},
		{1.0f, -1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f},
		{-1.0f, 1.0f, 1.0f},
	}};

	std::array<glm::vec3, 8> World{};
	for (std::size_t I = 0; I < Ndc.size(); ++I)
	{
		glm::vec4 P = Inv * glm::vec4(Ndc[I], 1.0f);
		if (std::abs(P.w) > 1e-6f)
		{
			P /= P.w;
		}
		World[I] = glm::vec3(P);
	}

	const std::array<std::pair<int, int>, 12> Edges = {{
		{0, 1},
		{1, 2},
		{2, 3},
		{3, 0},
		{4, 5},
		{5, 6},
		{6, 7},
		{7, 4},
		{0, 4},
		{1, 5},
		{2, 6},
		{3, 7},
	}};
	for (const auto& [i, j] : Edges)
	{
		AddLine(World[static_cast<std::size_t>(i)], World[static_cast<std::size_t>(j)], InColor);
	}
}

void FDebugDraw::Flush(const glm::mat4& ViewProjection) const
{
	if (!IsValid() || Vertices.empty())
	{
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(
		GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(Vertices.size() * sizeof(FVertex)), Vertices.data(), GL_DYNAMIC_DRAW);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	Shader.Bind();
	Shader.SetMat4("uViewProjection", glm::value_ptr(ViewProjection));
	glBindVertexArray(Vao);
	glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(Vertices.size()));
	glBindVertexArray(0);

	glDepthFunc(GL_LESS);
}
