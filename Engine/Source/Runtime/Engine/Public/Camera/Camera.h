#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>


enum class ECameraMode : std::uint8_t {
    Orbit,    // Blender-style tumble around a target
    FreeLook, // Unreal-like flying / first-person: eye + look yaw/pitch
};

/// View camera: orbit (default) or free-look for DefaultCameraActor.
class Camera {
public:
    void SetPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane);
    /// Orthographic projection; `height` is the full vertical world extent visible.
    void SetOrthographic(float height, float aspect, float nearPlane, float farPlane);

    /// Vertical FOV in degrees (rebuilds projection with last aspect/near/far).
    void SetFieldOfView(float fovDegrees);
    [[nodiscard]] float FieldOfView() const { return fovDegrees_; }
    [[nodiscard]] float Aspect() const { return aspect_; }
    [[nodiscard]] float NearPlane() const { return nearPlane_; }
    [[nodiscard]] float FarPlane() const { return farPlane_; }
    [[nodiscard]] bool IsOrthographic() const { return orthographic_; }
    [[nodiscard]] float OrthoHeight() const { return orthoHeight_; }
    void SetOrthoHeight(float height);

    void SetMode(ECameraMode mode);
    [[nodiscard]] ECameraMode Mode() const { return mode_; }

    /// Orbit: tumble around target. FreeLook: add yaw/pitch to look direction.
    void Orbit(float deltaYawDegrees, float deltaPitchDegrees);
    void AddLook(float deltaYawDegrees, float deltaPitchDegrees) {
        Orbit(deltaYawDegrees, deltaPitchDegrees);
    }

    /// Slide along camera right / world up (Orbit moves pivot; FreeLook moves eye).
    void Pan(float deltaRight, float deltaUp);

    void Zoom(float deltaDistance);
    void SetDistance(float distance);
    void SetYawPitch(float yawDegrees, float pitchDegrees);

    [[nodiscard]] glm::mat4 ViewMatrix() const;
    [[nodiscard]] const glm::mat4& ProjectionMatrix() const { return projection_; }
    /// Eye position (orbit: derived from target+distance; FreeLook: explicit eye).
    [[nodiscard]] glm::vec3 GetCameraLocation() const;

    [[nodiscard]] float Distance() const { return distance_; }
    [[nodiscard]] float YawDegrees() const { return yawDegrees_; }
    [[nodiscard]] float PitchDegrees() const { return pitchDegrees_; }
    [[nodiscard]] const glm::vec3& Target() const { return target_; }
    void SetTarget(const glm::vec3& target);

    /// FreeLook eye (ignored in Orbit mode).
    void SetEyeLocation(const glm::vec3& eye);
    [[nodiscard]] const glm::vec3& EyeLocation() const { return eye_; }

    /// Unit look / strafe vectors for the active mode.
    [[nodiscard]] glm::vec3 ForwardVector() const;
    [[nodiscard]] glm::vec3 RightVector() const;

private:
    void invalidateCache();
    void updateCachedPosition() const;

    ECameraMode mode_ = ECameraMode::Orbit;
    glm::mat4 projection_{1.0f};
    glm::vec3 target_{0.0f};
    glm::vec3 eye_{0.0f, 1.0f, 0.0f};
    float yawDegrees_ = 45.0f;
    float pitchDegrees_ = 25.0f;
    float distance_ = 5.0f;
    float fovDegrees_ = 60.0f;
    float aspect_ = 16.0f / 9.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 100.0f;
    bool orthographic_ = false;
    float orthoHeight_ = 20.0f;

    mutable bool cacheDirty_ = true;
    mutable glm::vec3 cachedPosition_{0.0f};
};

