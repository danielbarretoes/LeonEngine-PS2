#include <leon/gameplay/Controller.h>
#include <leon/gameplay/Pawn.h>

namespace leon {

void Pawn::detachController() {
    if (controller_ == nullptr) {
        return;
    }
    // Controller::UnPossess clears pawn_ and calls bindController(nullptr).
    controller_->UnPossess();
}

void Pawn::Destroy() {
    if (IsPendingKillPending()) {
        return;
    }
    detachController();
    Actor::Destroy();
}

void Pawn::EndPlay() {
    detachController();
}

} // namespace leon
