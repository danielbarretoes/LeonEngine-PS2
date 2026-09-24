#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"


Controller::~Controller() {
    UnPossess();
}

void Controller::Possess(Pawn* pawn) {
    if (pawn_ == pawn) {
        return;
    }
    if (pawn != nullptr) {
        if (Controller* previous = pawn->GetController(); previous != nullptr && previous != this) {
            previous->UnPossess();
        }
    }
    UnPossess();
    pawn_ = pawn;
    if (pawn_ != nullptr) {
        pawn_->bindController(this);
    }
}

void Controller::UnPossess() {
    if (pawn_ == nullptr) {
        return;
    }
    pawn_->bindController(nullptr);
    pawn_ = nullptr;
}

Character* Controller::GetCharacter() const {
    return dynamic_cast<Character*>(pawn_);
}

