#pragma once

#include "GameFramework/Pawn.h"


class ACharacter;

/// Drives a possessed Pawn (Unreal-style Controller).
class ENGINE_API AController {
public:
    virtual ~AController();

    AController(const AController&) = delete;
    AController& operator=(const AController&) = delete;
    AController(AController&&) = delete;
    AController& operator=(AController&&) = delete;

    void Possess(APawn* InPawn);
    void UnPossess();

    [[nodiscard]] APawn* GetPawn() const { return Pawn; }
    [[nodiscard]] bool HasPawn() const { return Pawn != nullptr; }
    [[nodiscard]] ACharacter* GetCharacter() const;

protected:
    AController() = default;

private:
    APawn* Pawn = nullptr;
};

