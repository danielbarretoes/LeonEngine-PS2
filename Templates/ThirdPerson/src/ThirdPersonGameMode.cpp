#include "ThirdPersonGameMode.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <iostream>
#include <leon/Engine.h>

#include "ThirdPersonCharacter.h"

namespace game {

bool ThirdPersonGameMode::Matches(const leon::LevelEntry& /*entry*/,
                               const std::string& gameModeId) const {
    return gameModeId.empty() || gameModeId == Id() || gameModeId == "third-person-template";
}

void ThirdPersonGameMode::OnEnter(leon::Engine& engine, const std::string& /*levelPath*/) {
    player_.UnPossess();
    GetWorld().Clear();
    GetGameState().Reset();
    player_.GetPlayerState().Reset();

    RegisterBodiesFromLevel(engine.GetLevel());

    glm::vec3 location{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    (void)FindPlayerStart(engine.GetLevel(), location, yaw);

    auto* character = GetWorld().SpawnActor<ThirdPersonCharacter>();
    if (!character->GetMesh().LoadFromCooked(engine, kCharacterAsset)) {
        std::cerr << "ThirdPersonGameMode: continuing without character visual (" << kCharacterAsset
                  << ")\n";
    }

    leon::SpringArmComponent& boom = character->SpringArm();
    boom.BoomYawDegrees += yaw;
    boom.ClampPitch();

    location.y = std::max(location.y, character->GetCharacterMovement().FloorY);
    character->Reset(location, yaw + character->GetCharacterMovement().ModelYawOffsetDegrees);
    character->SyncTransformToLevel(engine.GetLevel());
    GetWorld().GetPhysicsScene().SyncFromLevel(engine.GetLevel());
    player_.Possess(character);

    boom.SnapLagState(character->GetActorLocation());
    boom.ApplyToCamera(engine.GetCamera(), character->GetActorLocation(), 0.0f);

    GetGameState().HandleMatchHasStarted();
    engine.GetGameInstance().NotifyLevelOpened();
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetSuppressCameraDrag(false);
    engine.SetPlayMouseLookActive(true);
    engine.SetCursorCaptured(true);

    engine.AddOnScreenDebugMessage(
        "Third Person — mouse look, scroll zoom, WASD move, Space jump", 4.0f,
        {0.4f, 0.85f, 1.0f});
}

void ThirdPersonGameMode::OnExit(leon::Engine& engine) {
    GetGameState().HandleMatchHasEnded();
    player_.UnPossess();
    GetWorld().Clear();
    engine.SetCursorCaptured(false);
    engine.SetPlayMouseLookActive(false);
    engine.SetKeyboardOrbitEnabled(true);
    engine.SetOrbitMouseEnabled(true);
}

void ThirdPersonGameMode::Tick(leon::Engine& engine, float deltaTime) {
    GetGameState().Tick(deltaTime);
    player_.GetPlayerState().Tick(deltaTime);

    ThirdPersonCharacter* character = player_.GetThirdPersonCharacter();
    if (character == nullptr || character->IsPendingKillPending()) {
        return;
    }

    const glm::vec3 move = player_.TickInput(engine);
    const float moveLen = glm::length(move);
    const float speedAlpha = character->IsFalling() ? 0.0f : std::clamp(moveLen, 0.0f, 1.0f);
    character->SetAnimBlendInput(speedAlpha);

    leon::WorldGameplayFrameParams frame{};
    frame.deltaTime = deltaTime;
    frame.level = &engine.GetLevel();
    frame.renderer = &engine.GetRenderer();
    frame.collisionDebugDraw =
        engine.IsCollisionDebugEnabled() ? &engine.GetRenderer().GetDebugOverlay() : nullptr;
    frame.navMeshDebugDraw =
        engine.IsNavMeshDebugEnabled() ? &engine.GetRenderer().GetDebugOverlay() : nullptr;
    GetWorld().TickGameplayFrame(frame);

    player_.UpdateCamera(engine, deltaTime);
}

} // namespace game
