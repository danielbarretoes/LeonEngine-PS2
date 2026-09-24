#pragma once

#include <glm/vec3.hpp>

#include "GameFramework/PlayerController.h"


class ADefaultCameraActor;

/// APlayerController for `ADefaultCameraActor`: fly along look (Move*) + world up (MoveUp).
class ENGINE_API ADefaultPlayerController final : public APlayerController {
public:
    [[nodiscard]] ADefaultCameraActor* GetDefaultCameraActor() const;

    glm::vec3 TickInput(UGameEngine& Engine) override;
};

