#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <cmath>
#include "Math/Transform.h"

namespace leon {
namespace {

constexpr glm::vec3 kAxisX{1.0f, 0.0f, 0.0f};
constexpr glm::vec3 kAxisY{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 kAxisZ{0.0f, 0.0f, 1.0f};

float sanitizeScaleComponent(float v) {
    constexpr float kMin = 1e-4f;
    if (std::abs(v) < kMin) {
        return (v < 0.0f) ? -kMin : kMin;
    }
    return v;
}

} // namespace

glm::mat4 Transform::modelMatrix() const {
    const glm::vec3 safeScale{sanitizeScaleComponent(scale.x), sanitizeScaleComponent(scale.y),
                              sanitizeScaleComponent(scale.z)};
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotationDegrees[0]), kAxisX);
    model = glm::rotate(model, glm::radians(rotationDegrees[1]), kAxisY);
    model = glm::rotate(model, glm::radians(rotationDegrees[2]), kAxisZ);
    model = glm::scale(model, safeScale);
    return model;
}

glm::mat3 Transform::normalMatrix() const {
    const glm::mat3 m(modelMatrix());
    const float det = glm::determinant(m);
    if (std::abs(det) < 1e-12f) {
        return glm::mat3(1.0f);
    }
    return glm::transpose(glm::inverse(m));
}

} // namespace leon
