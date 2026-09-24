#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <leon/core/Camera.h>

namespace leon {
namespace {

[[nodiscard]] glm::vec3 freeLookForward(float yawDegrees, float pitchDegrees) {
    const float yawRad = glm::radians(yawDegrees);
    const float pitchRad = glm::radians(pitchDegrees);
    return glm::normalize(glm::vec3{
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad),
    });
}

/// Stable up for lookAt when looking nearly straight up/down (ortho Top).
[[nodiscard]] glm::vec3 freeLookWorldUp(float yawDegrees, float pitchDegrees) {
    if (pitchDegrees < -80.0f) {
        const float yawRad = glm::radians(yawDegrees);
        return glm::normalize(glm::vec3{std::cos(yawRad), 0.0f, std::sin(yawRad)});
    }
    if (pitchDegrees > 80.0f) {
        const float yawRad = glm::radians(yawDegrees);
        return glm::normalize(glm::vec3{-std::cos(yawRad), 0.0f, -std::sin(yawRad)});
    }
    return glm::vec3{0.0f, 1.0f, 0.0f};
}

} // namespace

void Camera::SetPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane) {
    fovDegrees_ = std::clamp(fovDegrees, 20.0f, 120.0f);
    aspect_ = aspect > 1.0e-4f ? aspect : (16.0f / 9.0f);
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
    orthographic_ = false;
    projection_ = glm::perspective(glm::radians(fovDegrees_), aspect_, nearPlane_, farPlane_);
}

void Camera::SetOrthographic(float height, float aspect, float nearPlane, float farPlane) {
    orthoHeight_ = std::clamp(height, 0.5f, 500.0f);
    aspect_ = aspect > 1.0e-4f ? aspect : (16.0f / 9.0f);
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
    orthographic_ = true;
    const float halfH = orthoHeight_ * 0.5f;
    const float halfW = halfH * aspect_;
    projection_ = glm::ortho(-halfW, halfW, -halfH, halfH, nearPlane_, farPlane_);
}

void Camera::SetOrthoHeight(float height) {
    if (orthographic_) {
        SetOrthographic(height, aspect_, nearPlane_, farPlane_);
    } else {
        orthoHeight_ = std::clamp(height, 0.5f, 500.0f);
    }
}

void Camera::SetFieldOfView(float fovDegrees) {
    SetPerspective(fovDegrees, aspect_, nearPlane_, farPlane_);
}

void Camera::SetMode(ECameraMode mode) {
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    invalidateCache();
}

void Camera::Orbit(float deltaYawDegrees, float deltaPitchDegrees) {
    yawDegrees_ += deltaYawDegrees;
    pitchDegrees_ = std::clamp(pitchDegrees_ + deltaPitchDegrees, -89.0f, 89.0f);
    invalidateCache();
}

void Camera::Pan(float deltaRight, float deltaUp) {
    const glm::vec3 right = RightVector();
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    const glm::vec3 delta = right * deltaRight + up * deltaUp;
    if (mode_ == ECameraMode::FreeLook) {
        eye_ += delta;
    } else {
        target_ += delta;
    }
    invalidateCache();
}

void Camera::Zoom(float deltaDistance) {
    if (mode_ != ECameraMode::Orbit) {
        return;
    }
    SetDistance(distance_ - deltaDistance);
}

void Camera::SetDistance(float distance) {
    distance_ = std::clamp(distance, 0.5f, 80.0f);
    invalidateCache();
}

void Camera::SetYawPitch(float yawDegrees, float pitchDegrees) {
    yawDegrees_ = yawDegrees;
    pitchDegrees_ = std::clamp(pitchDegrees, -89.0f, 89.0f);
    invalidateCache();
}

void Camera::SetTarget(const glm::vec3& target) {
    target_ = target;
    invalidateCache();
}

void Camera::SetEyeLocation(const glm::vec3& eye) {
    eye_ = eye;
    invalidateCache();
}

void Camera::invalidateCache() {
    cacheDirty_ = true;
}

void Camera::updateCachedPosition() const {
    if (!cacheDirty_) {
        return;
    }

    if (mode_ == ECameraMode::FreeLook) {
        cachedPosition_ = eye_;
        cacheDirty_ = false;
        return;
    }

    const float yawRad = glm::radians(yawDegrees_);
    const float pitchRad = glm::radians(pitchDegrees_);

    cachedPosition_ = target_ + glm::vec3{
                                    distance_ * std::cos(pitchRad) * std::cos(yawRad),
                                    distance_ * std::sin(pitchRad),
                                    distance_ * std::cos(pitchRad) * std::sin(yawRad),
                                };
    cacheDirty_ = false;
}

glm::vec3 Camera::GetCameraLocation() const {
    updateCachedPosition();
    return cachedPosition_;
}

glm::vec3 Camera::ForwardVector() const {
    if (mode_ == ECameraMode::FreeLook) {
        return freeLookForward(yawDegrees_, pitchDegrees_);
    }
    updateCachedPosition();
    const glm::vec3 toTarget = target_ - cachedPosition_;
    const float len = glm::length(toTarget);
    if (len < 1.0e-5f) {
        return glm::vec3{0.0f, 0.0f, -1.0f};
    }
    return toTarget / len;
}

glm::vec3 Camera::RightVector() const {
    const glm::vec3 forward = ForwardVector();
    const glm::vec3 up = (mode_ == ECameraMode::FreeLook)
                             ? freeLookWorldUp(yawDegrees_, pitchDegrees_)
                             : glm::vec3{0.0f, 1.0f, 0.0f};
    glm::vec3 right = glm::cross(forward, up);
    const float len = glm::length(right);
    if (len < 1.0e-5f) {
        right = glm::cross(forward, glm::vec3{0.0f, 0.0f, 1.0f});
        const float len2 = glm::length(right);
        if (len2 < 1.0e-5f) {
            return glm::vec3{1.0f, 0.0f, 0.0f};
        }
        return right / len2;
    }
    return right / len;
}

glm::mat4 Camera::ViewMatrix() const {
    updateCachedPosition();
    if (mode_ == ECameraMode::FreeLook) {
        const glm::vec3 forward = freeLookForward(yawDegrees_, pitchDegrees_);
        const glm::vec3 up = freeLookWorldUp(yawDegrees_, pitchDegrees_);
        return glm::lookAt(cachedPosition_, cachedPosition_ + forward, up);
    }
    return glm::lookAt(cachedPosition_, target_, glm::vec3{0.0f, 1.0f, 0.0f});
}

} // namespace leon
