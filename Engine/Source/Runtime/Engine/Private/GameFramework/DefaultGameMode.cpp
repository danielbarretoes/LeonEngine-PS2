#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include "GameFramework/DefaultCameraActor.h"
#include "GameFramework/DefaultGameMode.h"

namespace {

void beginFreeLookFromOrbit(UCameraComponent& camera) {
    const glm::vec3 eye = camera.GetCameraLocation();
    const glm::vec3 target = camera.Target();
    glm::vec3 look = target - eye;
    const float lookLen = glm::length(look);
    if (lookLen > 1.0e-5f) {
        look /= lookLen;
    } else {
        look = glm::vec3{0.0f, 0.0f, -1.0f};
    }

    const float pitch = std::asin(std::clamp(look.y, -1.0f, 1.0f)) * (180.0f / glm::pi<float>());
    const float yaw = std::atan2(look.z, look.x) * (180.0f / glm::pi<float>());

    camera.SetMode(ECameraMode::FreeLook);
    camera.SetEyeLocation(eye);
    camera.SetYawPitch(yaw, pitch);
}

} // namespace

bool ADefaultGameMode::Matches(const FLevelEntry& /*entry*/, const std::string& gameModeId) const {
    return gameModeId.empty() || gameModeId == Id();
}

void ADefaultGameMode::OnEnter(UGameEngine& engine, const std::string& /*levelPath*/) {
    player_.UnPossess();
    GetWorld().Clear();
    GetGameState().Reset();
    player_.GetPlayerState().Reset();

    UCameraComponent& camera = engine.GetCamera();
    savedOrbit_.target = camera.Target();
    savedOrbit_.distance = camera.Distance();
    savedOrbit_.yawDegrees = camera.YawDegrees();
    savedOrbit_.pitchDegrees = camera.PitchDegrees();

    beginFreeLookFromOrbit(camera);

    auto* cameraActor = GetWorld().SpawnActor<ADefaultCameraActor>();
    cameraActor->SetActorLocation(camera.EyeLocation());
    cameraActor->SetActorYaw(camera.YawDegrees());
    player_.Possess(cameraActor);
    PostLogin(player_);

    GetGameState().HandleMatchHasStarted();
    engine.GetGameInstance().NotifyLevelOpened();
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetSuppressCameraDrag(false);
    engine.SetPlayMouseLookActive(true);
    // Runtime / New Window: capture. Editor Selected Viewport clears this and gates look by hover.
    engine.SetCursorCaptured(true);
    mouseLookSampleValid_ = false;

    engine.AddOnScreenDebugMessage("DefaultCameraActor — mouse look, WASD fly, Q/E up/down", 5.0f,
                                   {0.35f, 0.95f, 0.55f});
}

void ADefaultGameMode::OnExit(UGameEngine& engine) {
    GetGameState().HandleMatchHasEnded();
    Logout(player_);
    player_.UnPossess();
    GetWorld().Clear();
    mouseLookSampleValid_ = false;

    UCameraComponent& camera = engine.GetCamera();
    camera.SetMode(ECameraMode::Orbit);
    camera.SetTarget(savedOrbit_.target);
    camera.SetDistance(savedOrbit_.distance);
    camera.SetYawPitch(savedOrbit_.yawDegrees, savedOrbit_.pitchDegrees);

    engine.SetCursorCaptured(false);
    engine.SetKeyboardOrbitEnabled(true);
    engine.SetOrbitMouseEnabled(true);
}

void ADefaultGameMode::Tick(UGameEngine& engine, float deltaTime) {
    GetGameState().Tick(deltaTime);
    player_.GetPlayerState().Tick(deltaTime);

    ADefaultCameraActor* cameraActor = player_.GetDefaultCameraActor();
    if (cameraActor == nullptr || cameraActor->IsPendingKillPending()) {
        return;
    }

    UCameraComponent& camera = engine.GetCamera();
    double mouseX = 0.0;
    double mouseY = 0.0;
    engine.GetPlayInputWindow().GetCursorPos(mouseX, mouseY);
    // EditorApp does not call Engine::handleInput — apply look here for PIE + runtime.
    if (engine.IsCursorCaptured() || engine.IsPlayMouseLookActive()) {
        if (mouseLookSampleValid_) {
            const float dx = static_cast<float>(mouseX - lastMouseX_);
            const float dy = static_cast<float>(mouseY - lastMouseY_);
            constexpr float kLookDegreesPerPixel = 0.15f;
            camera.AddLook(dx * kLookDegreesPerPixel, -dy * kLookDegreesPerPixel);
        }
        mouseLookSampleValid_ = true;
        lastMouseX_ = mouseX;
        lastMouseY_ = mouseY;
    } else {
        mouseLookSampleValid_ = false;
    }

    const glm::vec3 wish = player_.TickInput(engine);
    glm::vec3 location = cameraActor->GetActorLocation();
    location += wish * cameraActor->MoveSpeed() * deltaTime;
    cameraActor->SetActorLocation(location);
    cameraActor->SetActorYaw(camera.YawDegrees());

    GetWorld().Tick(deltaTime);

    cameraActor = player_.GetDefaultCameraActor();
    if (cameraActor == nullptr) {
        return;
    }

    camera.SetEyeLocation(cameraActor->GetActorLocation());
}

