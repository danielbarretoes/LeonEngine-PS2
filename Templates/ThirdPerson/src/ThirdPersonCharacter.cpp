#include "ThirdPersonCharacter.h"

namespace game {

ThirdPersonCharacter::ThirdPersonCharacter() {
    GetMesh().SetAnimInstance<ThirdPersonAnimInstance>();

    leon::CapsuleShape capsule{};
    capsule.radius = 0.35f;
    capsule.height = 1.85f;
    SetCapsule(capsule);

    leon::CharacterMovement movement{};
    movement.MaxWalkSpeed = 4.5f;
    movement.WalkBounds = 50.0f;
    movement.ModelYawOffsetDegrees = 0.0f;
    movement.TurnSharpness = 16.0f;
    movement.JumpZVelocity = 7.0f;
    movement.Gravity = 24.0f;
    movement.FloorY = 0.0f;
    movement.MaxStepHeight = 0.35f;
    movement.PushStrength = 0.85f;
    SetCharacterMovement(movement);

    springArm_.SetOwner(this);
    (void)springArm_.AttachToComponent(&GetRootComponent());
    springArm_.TargetArmLength = 4.5f;
    springArm_.SocketOffsetZ = 1.15f;
    springArm_.BoomYawDegrees = 200.0f;
    springArm_.BoomPitchDegrees = 18.0f;
    springArm_.bEnableCameraLag = true;
    springArm_.CameraLagSpeed = 10.0f;
    springArm_.bEnableCameraRotationLag = true;
    springArm_.CameraRotationLagSpeed = 14.0f;
    springArm_.ArmLengthLagSpeed = 10.0f;
}

} // namespace game
