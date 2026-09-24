#pragma once

#include "GameFramework/Actor.h"


class AController;

/// Possessable Actor (Unreal-style Pawn). Character derives from this.
class APawn : public AActor {
public:
    [[nodiscard]] AController* GetController() const { return Controller; }
    [[nodiscard]] bool IsPossessed() const { return Controller != nullptr; }

    /// UnPossess any Controller, then mark pending kill.
    void Destroy() override;
    /// Also UnPossess when removed via World::Clear.
    void EndPlay() override;

protected:
    APawn() = default;

private:
    friend class AController;

    void BindController(AController* InController) { Controller = InController; }
    void DetachController();

    AController* Controller = nullptr;
};

