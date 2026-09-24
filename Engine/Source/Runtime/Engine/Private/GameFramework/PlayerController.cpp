#include "Engine/GameEngine.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"


void APlayerController::Possess(ACharacter* Character) {
    AController::Possess(Character);
}

glm::vec3 APlayerController::TickInput(UGameEngine& /*engine*/) {
    return {};
}

void APlayerController::UpdateCamera(UGameEngine& /*engine*/, float /*deltaTime*/) {}

void APlayerController::LatchButtons(std::uint16_t PressedNow) {
    const std::uint16_t Masked = static_cast<std::uint16_t>(PressedNow & Leon::Net::InputButtonMask);
    PressedEdges = static_cast<std::uint16_t>(Masked & static_cast<std::uint16_t>(~PrevButtons));
    DownButtons = Masked;
    PrevButtons = Masked;
}

bool APlayerController::WasButtonPressed(Leon::Net::EInputButton Button) const {
    return (PressedEdges & static_cast<std::uint16_t>(Button)) != 0;
}

bool APlayerController::IsButtonDown(Leon::Net::EInputButton Button) const {
    return (DownButtons & static_cast<std::uint16_t>(Button)) != 0;
}

std::uint16_t APlayerController::ConsumeButtonPressedMask() {
    const std::uint16_t Edges = PressedEdges;
    PressedEdges = 0;
    return Edges;
}

