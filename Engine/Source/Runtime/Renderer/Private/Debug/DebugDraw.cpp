#include <glad/glad.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include "Misc/Paths.h"
#include "Debug/DebugDraw.h"
#include "GlAttrib.h"
#include <utility>


bool DebugDraw::Initialize(const std::string& /*shaderDirectory*/) {
    const std::string vert = ResolveAssetPath("assets/Shaders/debug_line.vert");
    const std::string frag = ResolveAssetPath("assets/Shaders/debug_line.frag");
    if (!shader_.LoadFromFiles(vert, frag)) {
        std::cerr << "Failed to load debug line shaders\n";
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          GlAttribOffset(&Vertex::position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), GlAttribOffset(&Vertex::color));
    glBindVertexArray(0);
    return true;
}

EShaderReloadResult DebugDraw::ReloadShader(bool force) {
    return force ? shader_.ForceReloadFromDisk() : shader_.ReloadFromDiskIfChanged();
}

void DebugDraw::Shutdown() {
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    shader_.Destroy();
    vertices_.clear();
}

void DebugDraw::Clear() {
    vertices_.clear();
}

void DebugDraw::AddLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color) {
    vertices_.push_back(Vertex{.position = a, .color = color});
    vertices_.push_back(Vertex{.position = b, .color = color});
}

void DebugDraw::AddArrow(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color,
                         float headLength, float headWidth) {
    AddLine(from, to, color);

    const glm::vec3 shaft = to - from;
    const float len = glm::length(shaft);
    if (len < 1.0e-4f) {
        return;
    }
    const glm::vec3 dir = shaft / len;
    glm::vec3 side = glm::cross(dir, glm::vec3{0.0f, 1.0f, 0.0f});
    if (glm::dot(side, side) < 1.0e-6f) {
        side = glm::cross(dir, glm::vec3{1.0f, 0.0f, 0.0f});
    }
    side = glm::normalize(side) * headWidth;
    const glm::vec3 back = to - (dir * headLength);
    AddLine(to, back + side, color);
    AddLine(to, back - side, color);
}

void DebugDraw::AddAabb(const glm::vec3& worldMin, const glm::vec3& worldMax,
                        const glm::vec3& color) {
    const glm::vec3& mn = worldMin;
    const glm::vec3& mx = worldMax;
    const std::array<glm::vec3, 8> c = {{
        {mn.x, mn.y, mn.z},
        {mx.x, mn.y, mn.z},
        {mx.x, mx.y, mn.z},
        {mn.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z},
        {mx.x, mn.y, mx.z},
        {mx.x, mx.y, mx.z},
        {mn.x, mx.y, mx.z},
    }};

    const std::array<std::pair<int, int>, 12> edges = {{
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
    for (const auto& [i, j] : edges) {
        AddLine(c[static_cast<std::size_t>(i)], c[static_cast<std::size_t>(j)], color);
    }
}

void DebugDraw::AddAxes(const glm::vec3& origin, float size) {
    AddLine(origin, origin + glm::vec3{size, 0.0f, 0.0f}, {1.0f, 0.2f, 0.2f});
    AddLine(origin, origin + glm::vec3{0.0f, size, 0.0f}, {0.2f, 1.0f, 0.2f});
    AddLine(origin, origin + glm::vec3{0.0f, 0.0f, size}, {0.2f, 0.4f, 1.0f});
}

void DebugDraw::AddLightFrustum(const glm::mat4& lightSpace, const glm::vec3& color) {
    const glm::mat4 inv = glm::inverse(lightSpace);
    const std::array<glm::vec3, 8> ndc = {{
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
    }};

    std::array<glm::vec3, 8> world{};
    for (std::size_t i = 0; i < ndc.size(); ++i) {
        glm::vec4 p = inv * glm::vec4(ndc[i], 1.0f);
        if (std::abs(p.w) > 1e-6f) {
            p /= p.w;
        }
        world[i] = glm::vec3(p);
    }

    const std::array<std::pair<int, int>, 12> edges = {{
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
    for (const auto& [i, j] : edges) {
        AddLine(world[static_cast<std::size_t>(i)], world[static_cast<std::size_t>(j)], color);
    }
}

void DebugDraw::Flush(const glm::mat4& viewProjection) const {
    if (!IsValid() || vertices_.empty()) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                 vertices_.data(), GL_DYNAMIC_DRAW);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    shader_.Bind();
    shader_.SetMat4("uViewProjection", glm::value_ptr(viewProjection));
    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);

    glDepthFunc(GL_LESS);
}

