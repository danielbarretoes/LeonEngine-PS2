#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Physics/PhysScene.h"

using Catch::Matchers::WithinAbs;

namespace {

void AddFloorBox(FPhysScene& Scene, const glm::vec3& Center, const glm::vec3& HalfExtents) {
    const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
    FBodyInstance& Body = Scene.GetBodies()[Id];
    Body.Position = Center;
    Body.HalfExtents = HalfExtents;
}

} // namespace

TEST_CASE("IsWalkable uses WalkableFloorZ", "[gameplay][character][floor]") {
    ACharacter Character;
    Character.GetCharacterMovement().WalkableFloorZ = 0.71f;

    FHitResult Flat{};
    Flat.bBlockingHit = true;
    Flat.ImpactNormal = {0.0f, 1.0f, 0.0f};
    REQUIRE(Character.IsWalkable(Flat));

    FHitResult Steep{};
    Steep.bBlockingHit = true;
    Steep.ImpactNormal = {0.0f, 0.5f, 0.0f}; // ~60° — steeper than default UE walkable
    REQUIRE_FALSE(Character.IsWalkable(Steep));

    Character.GetCharacterMovement().WalkableFloorZ = 0.5f;
    REQUIRE(Character.IsWalkable(Steep));
}

TEST_CASE("FindFloor hits infinite floor plane", "[gameplay][character][floor]") {
    FPhysScene Scene;
    ACharacter Character;
    Character.Reset({0.0f, 1.0f, 0.0f}, 0.0f);
    Character.GetCharacterMovement().FloorY = 0.0f;

    FFindFloorResult Floor{};
    Character.FindFloor(Scene, Floor, 2.0f, nullptr);
    REQUIRE(Floor.bBlockingHit);
    REQUIRE(Floor.bWalkableFloor);
    REQUIRE(Floor.Hit.bFloorPlane);
    REQUIRE_THAT(Floor.Hit.ImpactPoint.y, WithinAbs(0.0f, 1.0e-3f));
    REQUIRE_THAT(Floor.FloorDist, WithinAbs(1.0f, 1.0e-3f));
}

TEST_CASE("FindFloor hits static AABB top", "[gameplay][character][floor]") {
    FPhysScene Scene;
    // Box top at y = 2
    AddFloorBox(Scene, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f});

    ACharacter Character;
    Character.Reset({0.0f, 2.5f, 0.0f}, 0.0f);
    Character.GetCharacterMovement().FloorY = -100.0f; // prefer box over far plane

    FFindFloorResult Floor{};
    Character.FindFloor(Scene, Floor, 1.0f, nullptr);
    REQUIRE(Floor.bBlockingHit);
    REQUIRE(Floor.bWalkableFloor);
    REQUIRE_FALSE(Floor.Hit.bFloorPlane);
    REQUIRE_THAT(Floor.Hit.ImpactPoint.y, WithinAbs(2.0f, 1.0e-2f));
}

TEST_CASE("Character lands on floor plane after fall", "[gameplay][character][movement]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    REQUIRE(Character != nullptr);
    Character->Reset({0.0f, 2.0f, 0.0f}, 0.0f);
    Character->GetCharacterMovement().FloorY = 0.0f;
    Character->GetCharacterMovement().Gravity = 24.0f;
    Character->ApplyReplicatedState({0.0f, 2.0f, 0.0f}, 0.0f, 0.0f, false);
    REQUIRE(Character->IsFalling());

    FPhysScene& Scene = World.GetPhysicsScene();
    for (int I = 0; I < 180; ++I) {
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(Character->IsMovingOnGround());
    REQUIRE_THAT(Character->GetActorLocation().y, WithinAbs(0.0f, 0.05f));
    REQUIRE_THAT(Character->GetVelocityZ(), WithinAbs(0.0f, 0.1f));
    REQUIRE(Character->GetCurrentFloor().bWalkableFloor);
}

TEST_CASE("Character jump leaves ground then lands", "[gameplay][character][movement]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->Reset({0.0f, 0.0f, 0.0f}, 0.0f);
    Character->GetCharacterMovement().FloorY = 0.0f;
    Character->GetCharacterMovement().JumpZVelocity = 7.0f;
    Character->GetCharacterMovement().Gravity = 24.0f;

    FPhysScene& Scene = World.GetPhysicsScene();
    // Settle on floor.
    for (int I = 0; I < 10; ++I) {
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }
    REQUIRE(Character->IsMovingOnGround());

    Character->Jump();
    Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    REQUIRE(Character->IsFalling());
    REQUIRE(Character->GetVelocityZ() > 0.0f);

    bool bLanded = false;
    for (int I = 0; I < 180; ++I) {
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
        if (Character->IsMovingOnGround()) {
            bLanded = true;
            break;
        }
    }
    REQUIRE(bLanded);
    REQUIRE_THAT(Character->GetActorLocation().y, WithinAbs(0.0f, 0.05f));
}

TEST_CASE("Character does not walk through static wall", "[gameplay][character][movement]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->Reset({-2.0f, 0.0f, 0.0f}, 0.0f);
    Character->GetCharacterMovement().FloorY = 0.0f;
    Character->GetCharacterMovement().MaxWalkSpeed = 6.0f;

    FPhysScene& Scene = World.GetPhysicsScene();
    // Tall wall at x=0
    AddFloorBox(Scene, {0.0f, 1.0f, 0.0f}, {0.25f, 1.0f, 2.0f});

    for (int I = 0; I < 120; ++I) {
        Character->AddMovementInput({1.0f, 0.0f, 0.0f});
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }

    // Capsule radius 0.35 + wall halfX 0.25 → feet.x should stay left of ~-0.6
    REQUIRE(Character->GetActorLocation().x < -0.5f);
}

TEST_CASE("Character slides along wall with diagonal wish", "[gameplay][character][movement][sweep]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->Reset({-1.5f, 0.0f, 0.0f}, 0.0f);
    Character->GetCharacterMovement().FloorY = 0.0f;
    Character->GetCharacterMovement().MaxWalkSpeed = 5.0f;

    FPhysScene& Scene = World.GetPhysicsScene();
    AddFloorBox(Scene, {0.0f, 1.0f, 0.0f}, {0.25f, 1.0f, 4.0f}); // wall in YZ plane at x=0

    const float Z0 = Character->GetActorLocation().z;
    for (int I = 0; I < 90; ++I) {
        Character->AddMovementInput({1.0f, 0.0f, 1.0f});
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(Character->GetActorLocation().x < -0.5f);
    REQUIRE(Character->GetActorLocation().z > Z0 + 0.5f); // slid forward in Z
}

TEST_CASE("Character sweep does not tunnel thin wall at high speed",
          "[gameplay][character][movement][sweep]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->Reset({-1.0f, 0.0f, 0.0f}, 0.0f);
    Character->GetCharacterMovement().FloorY = 0.0f;
    Character->GetCharacterMovement().MaxWalkSpeed = 40.0f; // >> normal

    FPhysScene& Scene = World.GetPhysicsScene();
    AddFloorBox(Scene, {0.0f, 1.0f, 0.0f}, {0.1f, 1.0f, 2.0f});

    for (int I = 0; I < 30; ++I) {
        Character->AddMovementInput({1.0f, 0.0f, 0.0f});
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(Character->GetActorLocation().x < 0.0f);
}

TEST_CASE("Character steps up onto short ledge", "[gameplay][character][movement][stepup]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->Reset({-1.5f, 0.0f, 0.0f}, 0.0f);
    Character->GetCharacterMovement().FloorY = 0.0f;
    Character->GetCharacterMovement().MaxWalkSpeed = 5.0f;
    Character->GetCharacterMovement().MaxStepHeight = 0.35f;

    FPhysScene& Scene = World.GetPhysicsScene();
    // Top at y=0.30 (< MaxStepHeight). Long/wide so we stay on the ledge after stepping up.
    AddFloorBox(Scene, {8.0f, 0.15f, 0.0f}, {8.0f, 0.15f, 4.0f});

    for (int I = 0; I < 120; ++I) {
        Character->AddMovementInput({1.0f, 0.0f, 0.0f});
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(Character->GetActorLocation().x > 0.2f);
    REQUIRE(Character->GetActorLocation().y > 0.2f);
    REQUIRE(Character->IsMovingOnGround());
}

TEST_CASE("Character does not step up tall wall", "[gameplay][character][movement][stepup]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->Reset({-1.5f, 0.0f, 0.0f}, 0.0f);
    Character->GetCharacterMovement().FloorY = 0.0f;
    Character->GetCharacterMovement().MaxWalkSpeed = 5.0f;
    Character->GetCharacterMovement().MaxStepHeight = 0.35f;

    FPhysScene& Scene = World.GetPhysicsScene();
    // Top at y=1.0 (> MaxStepHeight)
    AddFloorBox(Scene, {0.5f, 0.5f, 0.0f}, {0.5f, 0.5f, 4.0f});

    for (int I = 0; I < 120; ++I) {
        Character->AddMovementInput({1.0f, 0.0f, 0.0f});
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(Character->GetActorLocation().x < 0.0f);
    REQUIRE(Character->GetActorLocation().y < 0.15f);
}

TEST_CASE("Character MovementMode Walking Jump Falling Land",
          "[gameplay][character][movement][mode]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->Reset({0.0f, 0.0f, 0.0f}, 0.0f);
    Character->GetCharacterMovement().FloorY = 0.0f;
    REQUIRE(Character->GetMovementMode() == EMovementMode::Walking);

    FPhysScene& Scene = World.GetPhysicsScene();
    Character->Jump();
    Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    REQUIRE(Character->GetMovementMode() == EMovementMode::Falling);
    REQUIRE(Character->IsFalling());

    bool bLanded = false;
    for (int I = 0; I < 180; ++I) {
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
        if (Character->GetMovementMode() == EMovementMode::Walking) {
            bLanded = true;
            break;
        }
    }
    REQUIRE(bLanded);
    REQUIRE(Character->IsMovingOnGround());
}

TEST_CASE("Character walks off ledge enters Falling", "[gameplay][character][movement][mode]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->Reset({0.0f, 1.0f, 0.0f}, 0.0f);
    Character->GetCharacterMovement().FloorY = -100.0f; // no infinite floor under gap
    Character->GetCharacterMovement().MaxWalkSpeed = 6.0f;
    Character->GetCharacterMovement().Gravity = 24.0f;

    FPhysScene& Scene = World.GetPhysicsScene();
    // Platform top at y=1, ends at x=0.5
    AddFloorBox(Scene, {0.0f, 0.5f, 0.0f}, {0.5f, 0.5f, 0.5f});

    // Settle on platform.
    for (int I = 0; I < 20; ++I) {
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }
    REQUIRE(Character->IsMovingOnGround());
    REQUIRE_THAT(Character->GetActorLocation().y, WithinAbs(1.0f, 0.05f));

    for (int I = 0; I < 90; ++I) {
        Character->AddMovementInput({1.0f, 0.0f, 0.0f});
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
        if (Character->IsFalling()) {
            break;
        }
    }
    REQUIRE(Character->IsFalling());
    REQUIRE(Character->GetMovementMode() == EMovementMode::Falling);
}

TEST_CASE("Character walk shove moves Dynamic crate without overlap",
          "[gameplay][character][movement][push]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    REQUIRE(Character != nullptr);
    Character->GetCharacterMovement().FloorY = 0.0f;
    Character->GetCharacterMovement().MaxWalkSpeed = 6.0f;
    Character->GetCharacterMovement().PushStrength = 0.85f;
    Character->Reset({0.0f, 0.0f, 0.0f}, 0.0f);

    FPhysScene& Scene = World.GetPhysicsScene();
    const std::size_t Id = Scene.AddBody({3, EBodyType::Dynamic, 1.0f, true});
    FBodyInstance& Crate = Scene.GetBodies()[Id];
    // Capsule radius ~0.35; place crate so walking +X contacts the west face.
    Crate.Position = {1.2f, 0.45f, 0.0f};
    Crate.HalfExtents = {0.4f, 0.45f, 0.4f};
    Crate.Mass = 1.0f;
    const float X0 = Crate.Position.x;

    for (int I = 0; I < 45; ++I) {
        Character->AddMovementInput({1.0f, 0.0f, 0.0f});
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
        FPhysSceneStepParams Step{};
        Step.DeltaTime = 1.0f / 60.0f;
        Step.FloorY = 0.0f;
        Step.Gravity = 24.0f;
        Scene.Step(Step);
    }

    REQUIRE(Crate.Position.x > X0 + 0.15f);
}

TEST_CASE("World separates overlapping Character capsules", "[gameplay][character][pawn]") {
    UWorld World;
    auto* A = World.SpawnActor<ACharacter>();
    auto* B = World.SpawnActor<ACharacter>();
    REQUIRE(A != nullptr);
    REQUIRE(B != nullptr);
    A->GetCharacterMovement().FloorY = 0.0f;
    B->GetCharacterMovement().FloorY = 0.0f;
    A->Reset({0.0f, 0.0f, 0.0f}, 0.0f);
    B->Reset({0.1f, 0.0f, 0.0f}, 0.0f);

    FWorldGameplayFrameParams Frame{};
    Frame.DeltaTime = 1.0f / 60.0f;
    World.TickGameplayFrame(Frame);

    const float Dx = A->GetActorLocation().x - B->GetActorLocation().x;
    const float Dz = A->GetActorLocation().z - B->GetActorLocation().z;
    const float Dist = std::sqrt((Dx * Dx) + (Dz * Dz));
    const float MinDist = A->GetCapsule().Radius + B->GetCapsule().Radius;
    REQUIRE(Dist + 1.0e-3f >= MinDist);
}

TEST_CASE("ResolvePawnOverlap ignores vertically separated capsules",
          "[gameplay][character][pawn]") {
    ACharacter A;
    ACharacter B;
    A.Reset({0.0f, 0.0f, 0.0f}, 0.0f);
    B.Reset({0.05f, 3.0f, 0.0f}, 0.0f);
    A.ResolvePawnOverlap(B);
    REQUIRE_THAT(A.GetActorLocation().x, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(B.GetActorLocation().x, WithinAbs(0.05f, 1.0e-5f));
}

TEST_CASE("Character walks up walkable slope ramp", "[gameplay][character][movement][slope]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->GetCharacterMovement().FloorY = -100.0f;
    Character->GetCharacterMovement().MaxWalkSpeed = 5.0f;
    Character->GetCharacterMovement().WalkableFloorZ = 0.71f; // ~44°

    FPhysScene& Scene = World.GetPhysicsScene();
    // 30° ramp (cos30≈0.866 walkable). FPlane through origin; y ≈ x * tan30.
    Scene.AddSlopeRamp({0.0f, 0.0f, 0.0f}, {8.0f, 8.0f, 2.0f}, 30.0f);

    const float X0 = -1.5f;
    const float Y0 = X0 * 0.57735027f; // tan(30°)
    Character->Reset({X0, Y0 + 0.05f, 0.0f}, 0.0f);
    for (int I = 0; I < 15; ++I) {
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }
    REQUIRE(Character->IsMovingOnGround());
    const float YStart = Character->GetActorLocation().y;

    for (int I = 0; I < 60; ++I) {
        Character->AddMovementInput({1.0f, 0.0f, 0.0f});
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(Character->IsMovingOnGround());
    REQUIRE(Character->GetActorLocation().x > X0 + 0.8f);
    REQUIRE(Character->GetActorLocation().y > YStart + 0.35f);
    REQUIRE(Character->GetCurrentFloor().bWalkableFloor);
    REQUIRE(Character->GetCurrentFloor().Hit.ImpactNormal.y >= 0.71f);
}

TEST_CASE("Character cannot stand on steep slope ramp", "[gameplay][character][movement][slope]") {
    UWorld World;
    auto* Character = World.SpawnActor<ACharacter>();
    Character->GetCharacterMovement().FloorY = -100.0f;
    Character->GetCharacterMovement().Gravity = 24.0f;
    Character->GetCharacterMovement().WalkableFloorZ = 0.71f;

    FPhysScene& Scene = World.GetPhysicsScene();
    // 60° ramp (cos60=0.5 < WalkableFloorZ)
    Scene.AddSlopeRamp({0.0f, 0.0f, 0.0f}, {4.0f, 4.0f, 2.0f}, 60.0f);

    Character->ApplyReplicatedState({0.0f, 2.0f, 0.0f}, 0.0f, 0.0f, false);
    for (int I = 0; I < 120; ++I) {
        Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(Character->IsFalling());
    REQUIRE_FALSE(Character->GetCurrentFloor().bWalkableFloor);
    // Must rest on / above the steep surface, not tunnel below.
    REQUIRE(Character->GetActorLocation().y > -0.1f);
}

TEST_CASE("Character AirControl scales horizontal move while Falling",
          "[gameplay][character][movement][air]") {
    auto RunAirMove = [](float AirControl) -> float {
        UWorld World;
        auto* Character = World.SpawnActor<ACharacter>();
        Character->Reset({0.0f, 4.0f, 0.0f}, 0.0f);
        Character->GetCharacterMovement().FloorY = 0.0f;
        Character->GetCharacterMovement().MaxWalkSpeed = 6.0f;
        Character->GetCharacterMovement().Gravity = 24.0f;
        Character->GetCharacterMovement().AirControl = AirControl;
        // Start airborne high enough that 30 frames stay Falling.
        Character->ApplyReplicatedState({0.0f, 4.0f, 0.0f}, 0.0f, 0.0f, false);

        FPhysScene& Scene = World.GetPhysicsScene();
        REQUIRE(Character->IsFalling());

        const float X0 = Character->GetActorLocation().x;
        for (int I = 0; I < 30; ++I) {
            REQUIRE(Character->IsFalling());
            Character->AddMovementInput({1.0f, 0.0f, 0.0f});
            Character->PerformMovement(Scene, 1.0f / 60.0f, nullptr);
        }
        return Character->GetActorLocation().x - X0;
    };

    const float DxZero = RunAirMove(0.0f);
    const float DxFull = RunAirMove(1.0f);
    REQUIRE_THAT(DxZero, WithinAbs(0.0f, 0.05f));
    REQUIRE(DxFull > 1.0f);
    REQUIRE(DxFull > DxZero + 0.5f);
}
