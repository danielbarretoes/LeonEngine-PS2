#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"


AController::~AController() {
    UnPossess();
}

void AController::Possess(APawn* InPawn) {
    if (Pawn == InPawn) {
        return;
    }
    if (InPawn != nullptr) {
        if (AController* Previous = InPawn->GetController(); Previous != nullptr && Previous != this) {
            Previous->UnPossess();
        }
    }
    UnPossess();
    Pawn = InPawn;
    if (Pawn != nullptr) {
        Pawn->BindController(this);
    }
}

void AController::UnPossess() {
    if (Pawn == nullptr) {
        return;
    }
    Pawn->BindController(nullptr);
    Pawn = nullptr;
}

ACharacter* AController::GetCharacter() const {
    return dynamic_cast<ACharacter*>(Pawn);
}

