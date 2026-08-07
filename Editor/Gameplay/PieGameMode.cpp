#include <algorithm>
#include <iostream>
#include <leon/editor/PieGameMode.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>

#include <leon/animation/SkeletalAnimation.h>
#include <leon/core/Input.h>
#include <leon/core/InputActions.h>
#include <leon/core/Window.h>
#include <leon/Engine.h>
#include <leon/gameplay/Character.h>
#include <leon/gameplay/SkeletalMeshComponent.h>
#include <leon/gameplay/SpringArmComponent.h>
#include <leon/gameplay/VolumeHelpers.h>


namespace leon::editor {
namespace {

// Staged from Templates/ThirdPerson/Content/assets (not Engine). Missing → capsule-only PIE.
constexpr const char* kPieBotCharacter = "assets/characters/bot/Bot.lchar";

class PieAnimInstance final : public CharacterAnimInstance {
public:
    void NativeInitializeAnimation() override {
        SkeletalMeshComponent* mesh = GetOwningMeshComponent();
        if (mesh == nullptr) {
            return;
        }
        BlendSpace1D& blend = mesh->GetBlendSpace();
        blend.name = "Locomotion_BS_1D";
        blend.axisMin = 0.0f;
        blend.axisMax = 1.0f;
        blend.ClearSamples();
        if (AnimSequence* idle = mesh->FindSequence("BreathingIdle")) {
            blend.AddSample(idle, 0.0f);
        }
        if (AnimSequence* run = mesh->FindSequence("Running")) {
            blend.AddSample(run, 1.0f);
        }
        SetBlendSpace(&blend);
        AnimJumpClips clips{};
        if (AnimSequence* jump = mesh->FindSequence("JumpingUp")) {
            jump->bLooping = false;
            clips.jumpStart = jump;
        }
        if (AnimSequence* fall = mesh->FindSequence("FallingIdle")) {
            fall->bLooping = true;
            clips.fallLoop = fall;
        }
        if (AnimSequence* land = mesh->FindSequence("FallingToLanding")) {
            land->bLooping = false;
            clips.land = land;
        }
        SetJumpClips(clips);
        SetJumpPlayRates(4.0f, 1.15f, 4.5f);
        SetCrossfadeDuration(0.12f);
        SetLandToLocomotionCrossfade(0.18f);
        SetLocomotionBlendInterpSpeed(12.0f);
    }
};

class PieCharacter final : public Character {
public:
    PieCharacter() {
        GetMesh().SetAnimInstance<PieAnimInstance>();
        CapsuleShape capsule{};
        capsule.radius = 0.35f;
        capsule.height = 1.85f;
        SetCapsule(capsule);
        CharacterMovement movement{};
        movement.MaxWalkSpeed = 4.5f;
        movement.WalkBounds = 50.0f;
        movement.JumpZVelocity = 7.0f;
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
    [[nodiscard]] SpringArmComponent& SpringArm() { return springArm_; }

private:
    SpringArmComponent springArm_{};
};

class PiePlayerController final : public PlayerController {
public:
    [[nodiscard]] PieCharacter* GetPieCharacter() const {
        return dynamic_cast<PieCharacter*>(GetCharacter());
    }

    glm::vec3 TickInput(Engine& engine) override {
        PieCharacter* character = GetPieCharacter();
        if (character == nullptr) {
            mouseLookSampleValid_ = false;
            return {};
        }
        SpringArmComponent& boom = character->SpringArm();
        Window& inputWindow = engine.GetPlayInputWindow();
        double mouseX = 0.0;
        double mouseY = 0.0;
        inputWindow.GetCursorPos(mouseX, mouseY);
        const bool wantLook = !engine.IsCameraDragSuppressed() &&
                              (engine.IsCursorCaptured() || engine.IsPlayMouseLookActive() ||
                               inputWindow.IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT));
        if (wantLook) {
            if (mouseLookSampleValid_) {
                const float dx = static_cast<float>(mouseX - lastMouseX_);
                const float dy = static_cast<float>(mouseY - lastMouseY_);
                constexpr float kLookDegreesPerPixel = 0.25f;
                boom.AddYawInput(dx * kLookDegreesPerPixel);
                boom.AddPitchInput(dy * kLookDegreesPerPixel);
            }
            mouseLookSampleValid_ = true;
            lastMouseX_ = mouseX;
            lastMouseY_ = mouseY;
        } else {
            mouseLookSampleValid_ = false;
            lastMouseX_ = mouseX;
            lastMouseY_ = mouseY;
        }
        const float scrollY = engine.ConsumeScrollY();
        if (scrollY != 0.0f) {
            boom.AddArmLengthInput(-scrollY * 0.4f);
        }
        const MoveAxes2D axes = engine.GetInput().GetMoveAxes2D();
        const glm::vec3 move = boom.GetMoveDirectionXZ(axes);
        character->AddMovementInput(move);
        if (engine.GetInput().WasActionJustPressed(InputActions::Jump)) {
            character->Jump();
        }
        return move;
    }

    void UpdateCamera(Engine& engine, float deltaTime) override {
        PieCharacter* character = GetPieCharacter();
        if (character == nullptr) {
            return;
        }
        character->SpringArm().ApplyToCamera(engine.GetCamera(), character->GetActorLocation(),
                                             deltaTime);
    }

private:
    bool mouseLookSampleValid_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
};

} // namespace

void PieGameMode::OnEnter(Engine& engine, const std::string& /*levelPath*/) {
    if (player_ != nullptr) {
        player_->UnPossess();
    }
    player_ = std::make_unique<PiePlayerController>();
    auto* piePc = static_cast<PiePlayerController*>(player_.get());
    GetWorld().Clear();
    GetGameState().Reset();
    piePc->GetPlayerState().Reset();
    painTickAccum_ = 0.0f;

    RegisterBodiesFromLevel(engine.GetLevel());

    glm::vec3 spawnLoc{0.0f, 0.0f, 0.0f};
    float spawnYaw = 0.0f;
    (void)FindPlayerStart(engine.GetLevel(), spawnLoc, spawnYaw, 0);

    const float floorY = EstimateFloorY(engine.GetLevel());

    auto* character = GetWorld().SpawnActor<PieCharacter>();
    if (!character->GetMesh().LoadFromCooked(engine, kPieBotCharacter)) {
        std::cerr << "PieGameMode: continuing without Bot.lchar visual\n";
    }
    character->GetCharacterMovement().FloorY = floorY;
    SpringArmComponent& boom = character->SpringArm();
    boom.BoomYawDegrees += spawnYaw;
    boom.ClampPitch();
    spawnLoc.y = std::max(spawnLoc.y, floorY);
    character->Reset(spawnLoc, boom.GetLookFacingYawDegrees() +
                                   character->GetCharacterMovement().ModelYawOffsetDegrees);
    character->SyncTransformToLevel(engine.GetLevel());
    GetWorld().GetPhysicsScene().SyncFromLevel(engine.GetLevel());
    piePc->Possess(character);
    boom.SnapLagState(character->GetActorLocation());
    boom.ApplyToCamera(engine.GetCamera(), character->GetActorLocation(), 0.0f);

    GetGameState().HandleMatchHasStarted();
    engine.GetGameInstance().NotifyLevelOpened();
    engine.SetPlayMouseLookActive(true);
    engine.SetOrbitMouseEnabled(false);
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetSuppressCameraDrag(false);
    engine.SetCursorCaptured(true);

    engine.AddOnScreenDebugMessage(
        "PIE — WASD move, mouse look, Space jump, Esc stop (edit-time preview)", 6.0f,
        {0.4f, 0.85f, 1.0f});
}

void PieGameMode::OnExit(Engine& engine) {
    GetGameState().HandleMatchHasEnded();
    if (player_ != nullptr) {
        player_->UnPossess();
    }
    player_.reset();
    GetWorld().Clear();
    painTickAccum_ = 0.0f;
    engine.SetCursorCaptured(false);
    engine.SetPlayMouseLookActive(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetSuppressCameraDrag(true);
}

void PieGameMode::Tick(Engine& engine, float deltaTime) {
    GetGameState().Tick(deltaTime);
    auto* piePc = dynamic_cast<PiePlayerController*>(player_.get());
    if (piePc == nullptr) {
        return;
    }
    piePc->GetPlayerState().Tick(deltaTime);

    PieCharacter* character = piePc->GetPieCharacter();
    if (character == nullptr || character->IsPendingKillPending()) {
        return;
    }

    const glm::vec3 move = piePc->TickInput(engine);
    const float moveLen = glm::length(move);
    character->SetAnimBlendInput(character->IsFalling() ? 0.0f : std::clamp(moveLen, 0.0f, 1.0f));

    Character* painTargets[] = {character};
    TickPainCausingVolumes(engine.GetLevel().PainCausingVolumes(), painTargets, deltaTime,
                           painTickAccum_);

    WorldGameplayFrameParams frame{};
    frame.deltaTime = deltaTime;
    frame.level = &engine.GetLevel();
    frame.renderer = &engine.GetRenderer();
    GetWorld().TickGameplayFrame(frame);
    piePc->UpdateCamera(engine, deltaTime);
}

} // namespace leon::editor
