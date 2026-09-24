#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Shader.h"
#include <string>
#include <vector>


/// Immediate-mode colored line batch for 3D debug (AABBs, light frustum, etc.).
class FDebugDraw {
public:
    bool Initialize(const std::string& shaderDirectory);
    void Shutdown();
    /// Reload line shader from disk if timestamps changed (or force).
    [[nodiscard]] EShaderReloadResult ReloadShader(bool force = false);

    void Clear();
    void AddLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color);
    /// Shaft + V-shaped head for a world-space direction vector.
    void AddArrow(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color,
                  float headLength = 0.28f, float headWidth = 0.14f);
    void AddAabb(const glm::vec3& worldMin, const glm::vec3& worldMax, const glm::vec3& color);
    /// RGB axes at a USceneComponent world location (editor / PIE debug).
    void AddAxes(const glm::vec3& origin, float size = 0.35f);
    /// Clip-space cube (±1) transformed by inverse(lightSpace) → world-space ortho frustum.
    void AddLightFrustum(const glm::mat4& lightSpace, const glm::vec3& color);

    void Flush(const glm::mat4& viewProjection) const;

    [[nodiscard]] bool IsValid() const { return shader_.Valid() && vao_ != 0; }
    [[nodiscard]] bool IsEmpty() const { return vertices_.empty(); }

private:
    struct FVertex {
        glm::vec3 position{};
        glm::vec3 color{};
    };

    FShader shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    std::vector<FVertex> vertices_;
};

