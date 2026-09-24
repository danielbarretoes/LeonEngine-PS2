#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Physics/PhysScene.h"

using Catch::Matchers::WithinAbs;

namespace {

void AddFloorBox(FPhysScene& scene, const glm::vec3& center, const glm::vec3& halfExtents) {
    const std::size_t id = scene.AddBody({0, EBodyType::Static, 1.0f, true});
    FBodyInstance& body = scene.Bodies()[id];
    body.position = center;
    body.halfExtents = halfExtents;
}

} // namespace

TEST_CASE("IsWalkable uses WalkableFloorZ", "[gameplay][character][floor]") {
    ACharacter character;
    character.GetCharacterMovement().WalkableFloorZ = 0.71f;

    FHitResult flat{};
    flat.bBlockingHit = true;
    flat.ImpactNormal = {0.0f, 1.0f, 0.0f};
    REQUIRE(character.IsWalkable(flat));

    FHitResult steep{};
    steep.bBlockingHit = true;
    steep.ImpactNormal = {0.0f, 0.5f, 0.0f}; // ~60° — steeper than default UE walkable
    REQUIRE_FALSE(character.IsWalkable(steep));

    character.GetCharacterMovement().WalkableFloorZ = 0.5f;
    REQUIRE(character.IsWalkable(steep));
}

TEST_CASE("FindFloor hits infinite floor plane", "[gameplay][character][floor]") {
    FPhysScene scene;
    ACharacter character;
    character.Reset({0.0f, 1.0f, 0.0f}, 0.0f);
    character.GetCharacterMovement().FloorY = 0.0f;

    FFindFloorResult floor{};
    character.FindFloor(scene, floor, 2.0f, nullptr);
    REQUIRE(floor.bBlockingHit);
    REQUIRE(floor.bWalkableFloor);
    REQUIRE(floor.Hit.bFloorPlane);
    REQUIRE_THAT(floor.Hit.ImpactPoint.y, WithinAbs(0.0f, 1.0e-3f));
    REQUIRE_THAT(floor.FloorDist, WithinAbs(1.0f, 1.0e-3f));
}

TEST_CASE("FindFloor hits static AABB top", "[gameplay][character][floor]") {
    FPhysScene scene;
    // Box top at y = 2
    AddFloorBox(scene, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f});

    ACharacter character;
    character.Reset({0.0f, 2.5f, 0.0f}, 0.0f);
    character.GetCharacterMovement().FloorY = -100.0f; // prefer box over far plane

    FFindFloorResult floor{};
    character.FindFloor(scene, floor, 1.0f, nullptr);
    REQUIRE(floor.bBlockingHit);
    REQUIRE(floor.bWalkableFloor);
    REQUIRE_FALSE(floor.Hit.bFloorPlane);
    REQUIRE_THAT(floor.Hit.ImpactPoint.y, WithinAbs(2.0f, 1.0e-2f));
}

TEST_CASE("Character lands on floor plane after fall", "[gameplay][character][movement]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    REQUIRE(character != nullptr);
    character->Reset({0.0f, 2.0f, 0.0f}, 0.0f);
    character->GetCharacterMovement().FloorY = 0.0f;
    character->GetCharacterMovement().Gravity = 24.0f;
    character->ApplyReplicatedState({0.0f, 2.0f, 0.0f}, 0.0f, 0.0f, false);
    REQUIRE(character->IsFalling());

    FPhysScene& scene = world.GetPhysicsScene();
    for (int i = 0; i < 180; ++i) {
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(character->IsMovingOnGround());
    REQUIRE_THAT(character->GetActorLocation().y, WithinAbs(0.0f, 0.05f));
    REQUIRE_THAT(character->GetVelocityZ(), WithinAbs(0.0f, 0.1f));
    REQUIRE(character->GetCurrentFloor().bWalkableFloor);
}

TEST_CASE("Character jump leaves ground then lands", "[gameplay][character][movement]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({0.0f, 0.0f, 0.0f}, 0.0f);
    character->GetCharacterMovement().FloorY = 0.0f;
    character->GetCharacterMovement().JumpZVelocity = 7.0f;
    character->GetCharacterMovement().Gravity = 24.0f;

    FPhysScene& scene = world.GetPhysicsScene();
    // Settle on floor.
    for (int i = 0; i < 10; ++i) {
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }
    REQUIRE(character->IsMovingOnGround());

    character->Jump();
    character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    REQUIRE(character->IsFalling());
    REQUIRE(character->GetVelocityZ() > 0.0f);

    bool landed = false;
    for (int i = 0; i < 180; ++i) {
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
        if (character->IsMovingOnGround()) {
            landed = true;
            break;
        }
    }
    REQUIRE(landed);
    REQUIRE_THAT(character->GetActorLocation().y, WithinAbs(0.0f, 0.05f));
}

TEST_CASE("Character does not walk through static wall", "[gameplay][character][movement]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({-2.0f, 0.0f, 0.0f}, 0.0f);
    character->GetCharacterMovement().FloorY = 0.0f;
    character->GetCharacterMovement().MaxWalkSpeed = 6.0f;

    FPhysScene& scene = world.GetPhysicsScene();
    // Tall wall at x=0
    AddFloorBox(scene, {0.0f, 1.0f, 0.0f}, {0.25f, 1.0f, 2.0f});

    for (int i = 0; i < 120; ++i) {
        character->AddMovementInput({1.0f, 0.0f, 0.0f});
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }

    // Capsule radius 0.35 + wall halfX 0.25 → feet.x should stay left of ~-0.6
    REQUIRE(character->GetActorLocation().x < -0.5f);
}

TEST_CASE("Character slides along wall with diagonal wish", "[gameplay][character][movement][sweep]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({-1.5f, 0.0f, 0.0f}, 0.0f);
    character->GetCharacterMovement().FloorY = 0.0f;
    character->GetCharacterMovement().MaxWalkSpeed = 5.0f;

    FPhysScene& scene = world.GetPhysicsScene();
    AddFloorBox(scene, {0.0f, 1.0f, 0.0f}, {0.25f, 1.0f, 4.0f}); // wall in YZ plane at x=0

    const float z0 = character->GetActorLocation().z;
    for (int i = 0; i < 90; ++i) {
        character->AddMovementInput({1.0f, 0.0f, 1.0f});
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(character->GetActorLocation().x < -0.5f);
    REQUIRE(character->GetActorLocation().z > z0 + 0.5f); // slid forward in Z
}

TEST_CASE("Character sweep does not tunnel thin wall at high speed",
          "[gameplay][character][movement][sweep]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({-1.0f, 0.0f, 0.0f}, 0.0f);
    character->GetCharacterMovement().FloorY = 0.0f;
    character->GetCharacterMovement().MaxWalkSpeed = 40.0f; // >> normal

    FPhysScene& scene = world.GetPhysicsScene();
    AddFloorBox(scene, {0.0f, 1.0f, 0.0f}, {0.1f, 1.0f, 2.0f});

    for (int i = 0; i < 30; ++i) {
        character->AddMovementInput({1.0f, 0.0f, 0.0f});
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(character->GetActorLocation().x < 0.0f);
}

TEST_CASE("Character steps up onto short ledge", "[gameplay][character][movement][stepup]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({-1.5f, 0.0f, 0.0f}, 0.0f);
    character->GetCharacterMovement().FloorY = 0.0f;
    character->GetCharacterMovement().MaxWalkSpeed = 5.0f;
    character->GetCharacterMovement().MaxStepHeight = 0.35f;

    FPhysScene& scene = world.GetPhysicsScene();
    // Top at y=0.30 (< MaxStepHeight). Long/wide so we stay on the ledge after stepping up.
    AddFloorBox(scene, {8.0f, 0.15f, 0.0f}, {8.0f, 0.15f, 4.0f});

    for (int i = 0; i < 120; ++i) {
        character->AddMovementInput({1.0f, 0.0f, 0.0f});
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(character->GetActorLocation().x > 0.2f);
    REQUIRE(character->GetActorLocation().y > 0.2f);
    REQUIRE(character->IsMovingOnGround());
}

TEST_CASE("Character does not step up tall wall", "[gameplay][character][movement][stepup]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({-1.5f, 0.0f, 0.0f}, 0.0f);
    character->GetCharacterMovement().FloorY = 0.0f;
    character->GetCharacterMovement().MaxWalkSpeed = 5.0f;
    character->GetCharacterMovement().MaxStepHeight = 0.35f;

    FPhysScene& scene = world.GetPhysicsScene();
    // Top at y=1.0 (> MaxStepHeight)
    AddFloorBox(scene, {0.5f, 0.5f, 0.0f}, {0.5f, 0.5f, 4.0f});

    for (int i = 0; i < 120; ++i) {
        character->AddMovementInput({1.0f, 0.0f, 0.0f});
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(character->GetActorLocation().x < 0.0f);
    REQUIRE(character->GetActorLocation().y < 0.15f);
}

TEST_CASE("Character MovementMode Walking Jump Falling Land",
          "[gameplay][character][movement][mode]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({0.0f, 0.0f, 0.0f}, 0.0f);
    character->GetCharacterMovement().FloorY = 0.0f;
    REQUIRE(character->GetMovementMode() == EMovementMode::Walking);

    FPhysScene& scene = world.GetPhysicsScene();
    character->Jump();
    character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    REQUIRE(character->GetMovementMode() == EMovementMode::Falling);
    REQUIRE(character->IsFalling());

    bool landed = false;
    for (int i = 0; i < 180; ++i) {
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
        if (character->GetMovementMode() == EMovementMode::Walking) {
            landed = true;
            break;
        }
    }
    REQUIRE(landed);
    REQUIRE(character->IsMovingOnGround());
}

TEST_CASE("Character walks off ledge enters Falling", "[gameplay][character][movement][mode]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({0.0f, 1.0f, 0.0f}, 0.0f);
    character->GetCharacterMovement().FloorY = -100.0f; // no infinite floor under gap
    character->GetCharacterMovement().MaxWalkSpeed = 6.0f;
    character->GetCharacterMovement().Gravity = 24.0f;

    FPhysScene& scene = world.GetPhysicsScene();
    // Platform top at y=1, ends at x=0.5
    AddFloorBox(scene, {0.0f, 0.5f, 0.0f}, {0.5f, 0.5f, 0.5f});

    // Settle on platform.
    for (int i = 0; i < 20; ++i) {
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }
    REQUIRE(character->IsMovingOnGround());
    REQUIRE_THAT(character->GetActorLocation().y, WithinAbs(1.0f, 0.05f));

    for (int i = 0; i < 90; ++i) {
        character->AddMovementInput({1.0f, 0.0f, 0.0f});
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
        if (character->IsFalling()) {
            break;
        }
    }
    REQUIRE(character->IsFalling());
    REQUIRE(character->GetMovementMode() == EMovementMode::Falling);
}

TEST_CASE("Character walk shove moves Dynamic crate without overlap",
          "[gameplay][character][movement][push]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    REQUIRE(character != nullptr);
    character->GetCharacterMovement().FloorY = 0.0f;
    character->GetCharacterMovement().MaxWalkSpeed = 6.0f;
    character->GetCharacterMovement().PushStrength = 0.85f;
    character->Reset({0.0f, 0.0f, 0.0f}, 0.0f);

    FPhysScene& scene = world.GetPhysicsScene();
    const std::size_t id = scene.AddBody({3, EBodyType::Dynamic, 1.0f, true});
    FBodyInstance& crate = scene.Bodies()[id];
    // Capsule radius ~0.35; place crate so walking +X contacts the west face.
    crate.position = {1.2f, 0.45f, 0.0f};
    crate.halfExtents = {0.4f, 0.45f, 0.4f};
    crate.mass = 1.0f;
    const float x0 = crate.position.x;

    for (int i = 0; i < 45; ++i) {
        character->AddMovementInput({1.0f, 0.0f, 0.0f});
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
        FPhysSceneStepParams step{};
        step.deltaTime = 1.0f / 60.0f;
        step.floorY = 0.0f;
        step.gravity = 24.0f;
        scene.Step(step);
    }

    REQUIRE(crate.position.x > x0 + 0.15f);
}

TEST_CASE("World separates overlapping Character capsules", "[gameplay][character][pawn]") {
    UWorld world;
    auto* a = world.SpawnActor<ACharacter>();
    auto* b = world.SpawnActor<ACharacter>();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    a->GetCharacterMovement().FloorY = 0.0f;
    b->GetCharacterMovement().FloorY = 0.0f;
    a->Reset({0.0f, 0.0f, 0.0f}, 0.0f);
    b->Reset({0.1f, 0.0f, 0.0f}, 0.0f);

    FWorldGameplayFrameParams frame{};
    frame.deltaTime = 1.0f / 60.0f;
    world.TickGameplayFrame(frame);

    const float dx = a->GetActorLocation().x - b->GetActorLocation().x;
    const float dz = a->GetActorLocation().z - b->GetActorLocation().z;
    const float dist = std::sqrt((dx * dx) + (dz * dz));
    const float minDist = a->GetCapsule().radius + b->GetCapsule().radius;
    REQUIRE(dist + 1.0e-3f >= minDist);
}

TEST_CASE("ResolvePawnOverlap ignores vertically separated capsules",
          "[gameplay][character][pawn]") {
    ACharacter a;
    ACharacter b;
    a.Reset({0.0f, 0.0f, 0.0f}, 0.0f);
    b.Reset({0.05f, 3.0f, 0.0f}, 0.0f);
    a.ResolvePawnOverlap(b);
    REQUIRE_THAT(a.GetActorLocation().x, WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_THAT(b.GetActorLocation().x, WithinAbs(0.05f, 1.0e-5f));
}

TEST_CASE("Character walks up walkable slope ramp", "[gameplay][character][movement][slope]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->GetCharacterMovement().FloorY = -100.0f;
    character->GetCharacterMovement().MaxWalkSpeed = 5.0f;
    character->GetCharacterMovement().WalkableFloorZ = 0.71f; // ~44°

    FPhysScene& scene = world.GetPhysicsScene();
    // 30° ramp (cos30≈0.866 walkable). FPlane through origin; y ≈ x * tan30.
    scene.AddSlopeRamp({0.0f, 0.0f, 0.0f}, {8.0f, 8.0f, 2.0f}, 30.0f);

    const float x0 = -1.5f;
    const float y0 = x0 * 0.57735027f; // tan(30°)
    character->Reset({x0, y0 + 0.05f, 0.0f}, 0.0f);
    for (int i = 0; i < 15; ++i) {
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }
    REQUIRE(character->IsMovingOnGround());
    const float yStart = character->GetActorLocation().y;

    for (int i = 0; i < 60; ++i) {
        character->AddMovementInput({1.0f, 0.0f, 0.0f});
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(character->IsMovingOnGround());
    REQUIRE(character->GetActorLocation().x > x0 + 0.8f);
    REQUIRE(character->GetActorLocation().y > yStart + 0.35f);
    REQUIRE(character->GetCurrentFloor().bWalkableFloor);
    REQUIRE(character->GetCurrentFloor().Hit.ImpactNormal.y >= 0.71f);
}

TEST_CASE("Character cannot stand on steep slope ramp", "[gameplay][character][movement][slope]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->GetCharacterMovement().FloorY = -100.0f;
    character->GetCharacterMovement().Gravity = 24.0f;
    character->GetCharacterMovement().WalkableFloorZ = 0.71f;

    FPhysScene& scene = world.GetPhysicsScene();
    // 60° ramp (cos60=0.5 < WalkableFloorZ)
    scene.AddSlopeRamp({0.0f, 0.0f, 0.0f}, {4.0f, 4.0f, 2.0f}, 60.0f);

    character->ApplyReplicatedState({0.0f, 2.0f, 0.0f}, 0.0f, 0.0f, false);
    for (int i = 0; i < 120; ++i) {
        character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
    }

    REQUIRE(character->IsFalling());
    REQUIRE_FALSE(character->GetCurrentFloor().bWalkableFloor);
    // Must rest on / above the steep surface, not tunnel below.
    REQUIRE(character->GetActorLocation().y > -0.1f);
}

TEST_CASE("Character AirControl scales horizontal move while Falling",
          "[gameplay][character][movement][air]") {
    auto runAirMove = [](float AirControl) -> float {
        UWorld world;
        auto* character = world.SpawnActor<ACharacter>();
        character->Reset({0.0f, 4.0f, 0.0f}, 0.0f);
        character->GetCharacterMovement().FloorY = 0.0f;
        character->GetCharacterMovement().MaxWalkSpeed = 6.0f;
        character->GetCharacterMovement().Gravity = 24.0f;
        character->GetCharacterMovement().AirControl = AirControl;
        // Start airborne high enough that 30 frames stay Falling.
        character->ApplyReplicatedState({0.0f, 4.0f, 0.0f}, 0.0f, 0.0f, false);

        FPhysScene& scene = world.GetPhysicsScene();
        REQUIRE(character->IsFalling());

        const float x0 = character->GetActorLocation().x;
        for (int i = 0; i < 30; ++i) {
            REQUIRE(character->IsFalling());
            character->AddMovementInput({1.0f, 0.0f, 0.0f});
            character->PerformMovement(scene, 1.0f / 60.0f, nullptr);
        }
        return character->GetActorLocation().x - x0;
    };

    const float dxZero = runAirMove(0.0f);
    const float dxFull = runAirMove(1.0f);
    REQUIRE_THAT(dxZero, WithinAbs(0.0f, 0.05f));
    REQUIRE(dxFull > 1.0f);
    REQUIRE(dxFull > dxZero + 0.5f);
}
