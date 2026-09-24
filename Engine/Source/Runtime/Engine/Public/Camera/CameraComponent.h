#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>


enum class ECameraMode : std::uint8_t {
    Orbit,    // Blender-style tumble around a target
    FreeLook, // Unreal-like flying / first-person: eye + look yaw/pitch
};

/// View camera: orbit (default) or free-look for ADefaultCameraActor.
class ENGINE_API UCameraComponent {
public:
    void SetPerspective(float InFovDegrees, float InAspect, float InNearPlane, float InFarPlane);
    /// Orthographic projection; `height` is the full vertical world extent visible.
    void SetOrthographic(float Height, float InAspect, float InNearPlane, float InFarPlane);

    /// Vertical FOV in degrees (rebuilds projection with last aspect/near/far).
    void SetFieldOfView(float InFovDegrees);
    [[nodiscard]] float FieldOfView() const { return FovDegrees; }
    [[nodiscard]] float GetAspect() const { return Aspect; }
    [[nodiscard]] float GetNearPlane() const { return NearPlane; }
    [[nodiscard]] float GetFarPlane() const { return FarPlane; }
    [[nodiscard]] bool IsOrthographic() const { return bOrthographic; }
    [[nodiscard]] float GetOrthoHeight() const { return OrthoHeight; }
    void SetOrthoHeight(float Height);

    void SetMode(ECameraMode InMode);
    [[nodiscard]] ECameraMode GetMode() const { return Mode; }

    /// Orbit: tumble around target. FreeLook: add yaw/pitch to look direction.
    void Orbit(float DeltaYawDegrees, float DeltaPitchDegrees);
    void AddLook(float DeltaYawDegrees, float DeltaPitchDegrees) {
        Orbit(DeltaYawDegrees, DeltaPitchDegrees);
    }

    /// Slide along camera right / world up (Orbit moves pivot; FreeLook moves eye).
    void Pan(float DeltaRight, float DeltaUp);

    void Zoom(float DeltaDistance);
    void SetDistance(float InDistance);
    void SetYawPitch(float InYawDegrees, float InPitchDegrees);

    [[nodiscard]] glm::mat4 ViewMatrix() const;
    [[nodiscard]] const glm::mat4& ProjectionMatrix() const { return Projection; }
    /// Eye position (orbit: derived from target+distance; FreeLook: explicit eye).
    [[nodiscard]] glm::vec3 GetCameraLocation() const;

    [[nodiscard]] float GetDistance() const { return Distance; }
    [[nodiscard]] float GetYawDegrees() const { return YawDegrees; }
    [[nodiscard]] float GetPitchDegrees() const { return PitchDegrees; }
    [[nodiscard]] const glm::vec3& GetTarget() const { return Target; }
    void SetTarget(const glm::vec3& InTarget);

    /// FreeLook eye (ignored in Orbit mode).
    void SetEyeLocation(const glm::vec3& InEye);
    [[nodiscard]] const glm::vec3& EyeLocation() const { return Eye; }

    /// Unit look / strafe vectors for the active mode.
    [[nodiscard]] glm::vec3 ForwardVector() const;
    [[nodiscard]] glm::vec3 RightVector() const;

private:
    void InvalidateCache();
    void UpdateCachedPosition() const;

    ECameraMode Mode = ECameraMode::Orbit;
    glm::mat4 Projection{1.0f};
    glm::vec3 Target{0.0f};
    glm::vec3 Eye{0.0f, 1.0f, 0.0f};
    float YawDegrees = 45.0f;
    float PitchDegrees = 25.0f;
    float Distance = 5.0f;
    float FovDegrees = 60.0f;
    float Aspect = 16.0f / 9.0f;
    float NearPlane = 0.1f;
    float FarPlane = 100.0f;
    bool bOrthographic = false;
    float OrthoHeight = 20.0f;

    mutable bool bCacheDirty = true;
    mutable glm::vec3 CachedPosition{0.0f};
};

