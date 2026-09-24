#pragma once

#include "Shader.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vector>

/// Immediate-mode colored line batch for 3D debug (AABBs, light frustum, etc.).
class RENDERER_API FDebugDraw
{
public:
	bool Initialize(const std::string& ShaderDirectory);
	void Shutdown();
	/// Reload line shader from disk if timestamps changed (or force).
	[[nodiscard]] EShaderReloadResult ReloadShader(bool bForce = false);

	void Clear();
	void AddLine(const glm::vec3& A, const glm::vec3& B, const glm::vec3& InColor);
	/// Shaft + V-shaped head for a world-space direction vector.
	void AddArrow(const glm::vec3& From, const glm::vec3& To, const glm::vec3& InColor, float HeadLength = 0.28f,
		float HeadWidth = 0.14f);
	void AddAabb(const glm::vec3& WorldMin, const glm::vec3& WorldMax, const glm::vec3& InColor);
	/// RGB axes at a USceneComponent world location (editor / PIE debug).
	void AddAxes(const glm::vec3& Origin, float Size = 0.35f);
	/// Clip-space cube (±1) transformed by inverse(lightSpace) → world-space ortho frustum.
	void AddLightFrustum(const glm::mat4& LightSpace, const glm::vec3& InColor);

	void Flush(const glm::mat4& ViewProjection) const;

	[[nodiscard]] bool IsValid() const
	{
		return Shader.Valid() && Vao != 0;
	}
	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.empty();
	}

private:
	struct FVertex
	{
		glm::vec3 Position{};
		glm::vec3 Color{};
	};

	FShader Shader;
	unsigned int Vao = 0;
	unsigned int Vbo = 0;
	std::vector<FVertex> Vertices;
};
