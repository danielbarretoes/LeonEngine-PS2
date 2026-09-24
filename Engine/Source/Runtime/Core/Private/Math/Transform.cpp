#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <cmath>
#include "Math/Transform.h"

namespace {

constexpr glm::vec3 AxisX{1.0f, 0.0f, 0.0f};
constexpr glm::vec3 AxisY{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 AxisZ{0.0f, 0.0f, 1.0f};

float SanitizeScaleComponent(float V) {
    constexpr float Min = 1e-4f;
    if (std::abs(V) < Min) {
        return (V < 0.0f) ? -Min : Min;
    }
    return V;
}

} // namespace

glm::mat4 FTransform::ModelMatrix() const {
    const glm::vec3 SafeScale{SanitizeScaleComponent(Scale.x), SanitizeScaleComponent(Scale.y),
                              SanitizeScaleComponent(Scale.z)};
    glm::mat4 Model(1.0f);
    Model = glm::translate(Model, Position);
    Model = glm::rotate(Model, glm::radians(RotationDegrees[0]), AxisX);
    Model = glm::rotate(Model, glm::radians(RotationDegrees[1]), AxisY);
    Model = glm::rotate(Model, glm::radians(RotationDegrees[2]), AxisZ);
    Model = glm::scale(Model, SafeScale);
    return Model;
}

glm::mat3 FTransform::NormalMatrix() const {
    const glm::mat3 M(ModelMatrix());
    const float Det = glm::determinant(M);
    if (std::abs(Det) < 1e-12f) {
        return glm::mat3(1.0f);
    }
    return glm::transpose(glm::inverse(M));
}

