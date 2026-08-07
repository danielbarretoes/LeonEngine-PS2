#pragma once

#include <leon/gameplay/Actor.h>

namespace leon {

class Controller;

/// Possessable Actor (Unreal-style Pawn). Character derives from this.
class Pawn : public Actor {
public:
    [[nodiscard]] Controller* GetController() const { return controller_; }
    [[nodiscard]] bool IsPossessed() const { return controller_ != nullptr; }

    /// UnPossess any Controller, then mark pending kill.
    void Destroy() override;
    /// Also UnPossess when removed via World::Clear.
    void EndPlay() override;

protected:
    Pawn() = default;

private:
    friend class Controller;

    void bindController(Controller* controller) { controller_ = controller; }
    void detachController();

    Controller* controller_ = nullptr;
};

} // namespace leon
