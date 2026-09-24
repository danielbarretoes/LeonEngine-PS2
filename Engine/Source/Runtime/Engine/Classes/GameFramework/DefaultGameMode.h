#pragma once

#include <glm/vec3.hpp>

#include "Engine/GameEngine.h"
#include "GameFramework/DefaultPlayerController.h"
#include "GameFramework/GameModeBase.h"


/// Fallback GameMode when a level has no `gameMode` override (Unreal default GameMode).
/// Spawns and possesses `ADefaultCameraActor` as the default pawn (free-look fly).
class ADefaultGameMode final : public AGameModeBase {
public:
    [[nodiscard]] const char* Id() const override { return "Default"; }

    [[nodiscard]] bool Matches(const FLevelEntry& entry,
                               const std::string& gameModeId) const override;

    void OnEnter(UGameEngine& engine, const std::string& levelPath) override;
    void OnExit(UGameEngine& engine) override;
    void Tick(UGameEngine& engine, float deltaTime) override;

private:
    struct FOrbitSnapshot {
        glm::vec3 target{0.0f};
        float distance = 8.0f;
        float yawDegrees = 45.0f;
        float pitchDegrees = 25.0f;
    };

    ADefaultPlayerController player_{};
    FOrbitSnapshot savedOrbit_{};
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
    bool mouseLookSampleValid_ = false;
};

