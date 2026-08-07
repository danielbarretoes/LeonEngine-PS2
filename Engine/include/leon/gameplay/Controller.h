#pragma once

#include <leon/gameplay/Pawn.h>

namespace leon {

class Character;

/// Drives a possessed Pawn (Unreal-style Controller).
class Controller {
public:
    virtual ~Controller();

    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;
    Controller(Controller&&) = delete;
    Controller& operator=(Controller&&) = delete;

    void Possess(Pawn* pawn);
    void UnPossess();

    [[nodiscard]] Pawn* GetPawn() const { return pawn_; }
    [[nodiscard]] bool HasPawn() const { return pawn_ != nullptr; }
    [[nodiscard]] Character* GetCharacter() const;

protected:
    Controller() = default;

private:
    Pawn* pawn_ = nullptr;
};

} // namespace leon
