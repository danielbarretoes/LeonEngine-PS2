#pragma once

#include "GameFramework/Actor.h"


class AController;

/// Possessable Actor (Unreal-style Pawn). Character derives from this.
class APawn : public AActor {
public:
    [[nodiscard]] AController* GetController() const { return controller_; }
    [[nodiscard]] bool IsPossessed() const { return controller_ != nullptr; }

    /// UnPossess any Controller, then mark pending kill.
    void Destroy() override;
    /// Also UnPossess when removed via World::Clear.
    void EndPlay() override;

protected:
    APawn() = default;

private:
    friend class AController;

    void bindController(AController* controller) { controller_ = controller; }
    void detachController();

    AController* controller_ = nullptr;
};

