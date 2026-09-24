#pragma once

#include <glm/vec3.hpp>

#include "GameFramework/PlayerController.h"


class DefaultCameraActor;

/// PlayerController for `DefaultCameraActor`: fly along look (Move*) + world up (MoveUp).
class DefaultPlayerController final : public PlayerController {
public:
    [[nodiscard]] DefaultCameraActor* GetDefaultCameraActor() const;

    glm::vec3 TickInput(Engine& engine) override;
};

