#include "Engine/GameEngine.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"


void PlayerController::Possess(Character* character) {
    Controller::Possess(character);
}

glm::vec3 PlayerController::TickInput(Engine& /*engine*/) {
    return {};
}

void PlayerController::UpdateCamera(Engine& /*engine*/, float /*deltaTime*/) {}

void PlayerController::LatchButtons(std::uint16_t pressedNow) {
    const std::uint16_t masked = static_cast<std::uint16_t>(pressedNow & Leon::Net::kInputButtonMask);
    pressedEdges_ = static_cast<std::uint16_t>(masked & static_cast<std::uint16_t>(~prevButtons_));
    downButtons_ = masked;
    prevButtons_ = masked;
}

bool PlayerController::WasButtonPressed(Leon::Net::EInputButton button) const {
    return (pressedEdges_ & static_cast<std::uint16_t>(button)) != 0;
}

bool PlayerController::IsButtonDown(Leon::Net::EInputButton button) const {
    return (downButtons_ & static_cast<std::uint16_t>(button)) != 0;
}

std::uint16_t PlayerController::ConsumeButtonPressedMask() {
    const std::uint16_t edges = pressedEdges_;
    pressedEdges_ = 0;
    return edges;
}

