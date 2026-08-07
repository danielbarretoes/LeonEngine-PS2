#pragma once

#include <leon/Gameplay.h>

namespace game {

class ThirdPersonCharacter;

/// Project PlayerController: boom-relative move, jump, mouse look.
class ThirdPersonPlayerController final : public leon::PlayerController {
public:
    [[nodiscard]] ThirdPersonCharacter* GetThirdPersonCharacter() const;

    glm::vec3 TickInput(leon::Engine& engine) override;
    void UpdateCamera(leon::Engine& engine, float deltaTime) override;

private:
    bool mouseLookSampleValid_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
};

} // namespace game
