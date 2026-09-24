#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"


void APawn::detachController() {
    if (controller_ == nullptr) {
        return;
    }
    // Controller::UnPossess clears pawn_ and calls bindController(nullptr).
    controller_->UnPossess();
}

void APawn::Destroy() {
    if (IsPendingKillPending()) {
        return;
    }
    detachController();
    AActor::Destroy();
}

void APawn::EndPlay() {
    detachController();
}

