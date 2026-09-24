#pragma once

#include "GameFramework/Pawn.h"


/// Default possessed pawn for `ADefaultGameMode` (Unreal-like DefaultPawn / flying camera).
/// Free-look: LMB aims, WASD flies along look direction, Q/E world vertical.
class ADefaultCameraActor : public APawn {
public:
    [[nodiscard]] float MoveSpeed() const { return moveSpeed_; }
    void SetMoveSpeed(float speed) { moveSpeed_ = speed > 0.0f ? speed : 0.0f; }

    [[nodiscard]] float LookSensitivity() const { return lookSensitivity_; }
    void SetLookSensitivity(float degreesPerPixel) {
        lookSensitivity_ = degreesPerPixel > 0.0f ? degreesPerPixel : 0.0f;
    }

private:
    float moveSpeed_ = 8.0f;
    float lookSensitivity_ = 0.15f;
};

