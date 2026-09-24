#pragma once

#include "GameFramework/Pawn.h"


class ACharacter;

/// Drives a possessed Pawn (Unreal-style Controller).
class AController {
public:
    virtual ~AController();

    AController(const AController&) = delete;
    AController& operator=(const AController&) = delete;
    AController(AController&&) = delete;
    AController& operator=(AController&&) = delete;

    void Possess(APawn* pawn);
    void UnPossess();

    [[nodiscard]] APawn* GetPawn() const { return pawn_; }
    [[nodiscard]] bool HasPawn() const { return pawn_ != nullptr; }
    [[nodiscard]] ACharacter* GetCharacter() const;

protected:
    AController() = default;

private:
    APawn* pawn_ = nullptr;
};

