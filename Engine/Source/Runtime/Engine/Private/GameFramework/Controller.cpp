#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"


AController::~AController() {
    UnPossess();
}

void AController::Possess(APawn* pawn) {
    if (pawn_ == pawn) {
        return;
    }
    if (pawn != nullptr) {
        if (AController* previous = pawn->GetController(); previous != nullptr && previous != this) {
            previous->UnPossess();
        }
    }
    UnPossess();
    pawn_ = pawn;
    if (pawn_ != nullptr) {
        pawn_->bindController(this);
    }
}

void AController::UnPossess() {
    if (pawn_ == nullptr) {
        return;
    }
    pawn_->bindController(nullptr);
    pawn_ = nullptr;
}

ACharacter* AController::GetCharacter() const {
    return dynamic_cast<ACharacter*>(pawn_);
}

