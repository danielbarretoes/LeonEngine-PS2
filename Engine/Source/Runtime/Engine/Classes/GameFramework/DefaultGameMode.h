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

    [[nodiscard]] bool Matches(const FLevelEntry& Entry,
                               const std::string& GameModeId) const override;

    void OnEnter(UGameEngine& Engine, const std::string& LevelPath) override;
    void OnExit(UGameEngine& Engine) override;
    void Tick(UGameEngine& Engine, float DeltaTime) override;

private:
    struct FOrbitSnapshot {
        glm::vec3 Target{0.0f};
        float Distance = 8.0f;
        float YawDegrees = 45.0f;
        float PitchDegrees = 25.0f;
    };

    ADefaultPlayerController Player{};
    FOrbitSnapshot SavedOrbit{};
    double LastMouseX = 0.0;
    double LastMouseY = 0.0;
    bool bMouseLookSampleValid = false;
};

