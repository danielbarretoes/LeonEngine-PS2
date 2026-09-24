#include "Engine/GameEngine.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"


void APlayerController::Possess(ACharacter* character) {
    AController::Possess(character);
}

glm::vec3 APlayerController::TickInput(UGameEngine& /*engine*/) {
    return {};
}

void APlayerController::UpdateCamera(UGameEngine& /*engine*/, float /*deltaTime*/) {}

void APlayerController::LatchButtons(std::uint16_t pressedNow) {
    const std::uint16_t masked = static_cast<std::uint16_t>(pressedNow & Leon::Net::InputButtonMask);
    pressedEdges_ = static_cast<std::uint16_t>(masked & static_cast<std::uint16_t>(~prevButtons_));
    downButtons_ = masked;
    prevButtons_ = masked;
}

bool APlayerController::WasButtonPressed(Leon::Net::EInputButton button) const {
    return (pressedEdges_ & static_cast<std::uint16_t>(button)) != 0;
}

bool APlayerController::IsButtonDown(Leon::Net::EInputButton button) const {
    return (downButtons_ & static_cast<std::uint16_t>(button)) != 0;
}

std::uint16_t APlayerController::ConsumeButtonPressedMask() {
    const std::uint16_t edges = pressedEdges_;
    pressedEdges_ = 0;
    return edges;
}

