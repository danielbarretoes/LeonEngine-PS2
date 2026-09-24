#pragma once

#include <glm/vec3.hpp>

#include "Engine/GameEngine.h"
#include "GameFramework/DefaultPlayerController.h"
#include "GameFramework/GameMode.h"


/// Fallback GameMode when a level has no `gameMode` override (Unreal default GameMode).
/// Spawns and possesses `DefaultCameraActor` as the default pawn (free-look fly).
class DefaultGameMode final : public GameMode {
public:
    [[nodiscard]] const char* Id() const override { return "Default"; }

    [[nodiscard]] bool Matches(const LevelEntry& entry,
                               const std::string& gameModeId) const override;

    void OnEnter(Engine& engine, const std::string& levelPath) override;
    void OnExit(Engine& engine) override;
    void Tick(Engine& engine, float deltaTime) override;

private:
    struct OrbitSnapshot {
        glm::vec3 target{0.0f};
        float distance = 8.0f;
        float yawDegrees = 45.0f;
        float pitchDegrees = 25.0f;
    };

    DefaultPlayerController player_{};
    OrbitSnapshot savedOrbit_{};
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
    bool mouseLookSampleValid_ = false;
};

