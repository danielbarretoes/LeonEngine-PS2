#include <glm/geometric.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "Debug/DebugDraw.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DefaultGameMode.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "AI/Navigation/NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Level/LevelCatalog.h"
#include "Net/RootReplication.h"
#include "BodyInstance.h"
#include "PhysicsBackend.h"
#include "Physics/PhysScene.h"
#include "TriangleCollision.h"
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

class TestActor : public AActor {};
class TestPawn : public APawn {};
class TestController : public AController {};

} // namespace

TEST_CASE("World spawns ticks and destroys actors", "[gameplay][world]") {
    UWorld world;
    auto* actor = world.SpawnActor<TestActor>();
    REQUIRE(actor != nullptr);
    REQUIRE(world.ActorCount() == 1);
    REQUIRE(actor->GetWorld() == &world);

    actor->SetActorLocation({1.0f, 2.0f, 3.0f});
    REQUIRE_THAT(actor->GetActorLocation().y, WithinAbs(2.0f, 1.0e-5f));

    world.DestroyActor(actor);
    REQUIRE(actor->IsPendingKillPending());
    world.Tick(0.016f);
    REQUIRE(world.ActorCount() == 0);

    world.SpawnActor<TestActor>();
    world.Clear();
    REQUIRE(world.ActorCount() == 0);
}

TEST_CASE("World FindFirst finds derived type", "[gameplay][world]") {
    UWorld world;
    world.SpawnActor<TestActor>();
    auto* pawn = world.SpawnActor<TestPawn>();
    REQUIRE(world.FindFirst<TestPawn>() == pawn);
    REQUIRE(world.FindFirst<ACharacter>() == nullptr);
}

TEST_CASE("Controller Possess and UnPossess", "[gameplay][controller]") {
    UWorld world;
    auto* pawn = world.SpawnActor<TestPawn>();
    TestController controller;
    controller.Possess(pawn);
    REQUIRE(controller.HasPawn());
    REQUIRE(pawn->IsPossessed());
    REQUIRE(pawn->GetController() == &controller);

    controller.UnPossess();
    REQUIRE_FALSE(controller.HasPawn());
    REQUIRE_FALSE(pawn->IsPossessed());
}

TEST_CASE("Pawn Destroy UnPossesses controller", "[gameplay][pawn]") {
    UWorld world;
    auto* pawn = world.SpawnActor<TestPawn>();
    TestController controller;
    controller.Possess(pawn);
    pawn->Destroy();
    world.Tick(0.0f);
    REQUIRE_FALSE(controller.HasPawn());
}

TEST_CASE("GameState match timer and PlayerState score", "[gameplay][state]") {
    AGameStateBase gs;
    gs.HandleMatchHasStarted();
    gs.Tick(0.5f);
    REQUIRE(gs.HasMatchStarted());
    REQUIRE_THAT(gs.GetServerWorldTimeSeconds(), WithinAbs(0.5f, 1.0e-5f));
    gs.Reset();
    REQUIRE_THAT(gs.GetServerWorldTimeSeconds(), WithinAbs(0.0f, 1.0e-5f));
    REQUIRE_FALSE(gs.HasMatchStarted());

    APlayerState ps;
    ps.SetPlayerId(2);
    ps.SetPlayerName("P2");
    ps.AddScore(10.0f);
    REQUIRE(ps.GetPlayerId() == 2);
    REQUIRE(ps.GetPlayerName() == "P2");
    REQUIRE_THAT(ps.GetScore(), WithinAbs(10.0f, 1.0e-5f));
    ps.Reset();
    REQUIRE_THAT(ps.GetScore(), WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("GameInstance NotifyLevelOpened", "[gameplay][gameinstance]") {
    UGameInstance gi;
    REQUIRE(gi.LevelsOpened() == 0);
    gi.NotifyLevelOpened();
    gi.NotifyLevelOpened();
    REQUIRE(gi.LevelsOpened() == 2);
}

TEST_CASE("SpringArmComponent clamps pitch and arm length", "[gameplay][springarm]") {
    USpringArmComponent arm;
    arm.AddPitchInput(200.0f);
    REQUIRE(arm.BoomPitchDegrees <= arm.PitchMax);
    arm.AddPitchInput(-400.0f);
    REQUIRE(arm.BoomPitchDegrees >= arm.PitchMin);

    arm.AddArmLengthInput(100.0f);
    REQUIRE_THAT(arm.TargetArmLength, WithinAbs(arm.ArmLengthMax, 1.0e-5f));

    arm.SnapLagState({0.0f, 0.0f, 0.0f});
    UCameraComponent camera;
    arm.ApplyToCamera(camera, {1.0f, 0.0f, 0.0f}, 0.016f);
    REQUIRE(camera.Mode() == ECameraMode::Orbit);
    REQUIRE_THAT(camera.Target().y, WithinAbs(arm.SocketOffsetZ, 0.5f));
}

TEST_CASE("SpringArmComponent collision probe shortens arm", "[gameplay][springarm]") {
    FPhysScene scene;
    const std::size_t id = scene.AddBody({0, EBodyType::Static, 1.0f, true});
    scene.Bodies()[id].position = {2.0f, 1.0f, 0.0f};
    scene.Bodies()[id].halfExtents = {0.25f, 1.0f, 2.0f};

    USpringArmComponent arm;
    arm.bDoCollisionTest = true;
    arm.bEnableCameraLag = false;
    arm.bEnableCameraRotationLag = false;
    arm.ArmLengthLagSpeed = 1000.0f;
    arm.TargetArmLength = 6.0f;
    arm.ArmLengthMin = 0.5f;
    arm.BoomYawDegrees = 0.0f;
    arm.BoomPitchDegrees = 0.0f;
    arm.SocketOffsetZ = 1.0f;
    arm.SocketOffsetX = 0.0f;
    arm.ProbeSize = 0.15f;
    arm.CollisionProbeOffset = 0.05f;
    arm.SnapLagState({0.0f, 0.0f, 0.0f});

    UCameraComponent camera;
    arm.ApplyToCamera(camera, {0.0f, 0.0f, 0.0f}, 0.016f, &scene);
    REQUIRE(camera.Distance() < 3.0f);
    REQUIRE(camera.Distance() >= arm.ArmLengthMin);
}

TEST_CASE("AIController steers toward target and arrives", "[gameplay][ai]") {
    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({0.0f, 0.0f, 0.0f});

    AIController ai;
    ai.Possess(character);
    ai.SetArriveRadius(0.5f);
    ai.MoveToLocation({10.0f, 0.0f, 0.0f});

    const glm::vec3 wishFar = ai.TickAI(0.016f);
    REQUIRE_THAT(glm::length(wishFar), WithinAbs(1.0f, 1.0e-3f));
    REQUIRE(wishFar.x > 0.5f);

    character->Reset({10.0f, 0.0f, 0.0f});
    const glm::vec3 wishNear = ai.TickAI(0.016f);
    REQUIRE_THAT(glm::length(wishNear), WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("AIController MoveToActor tracks moving target", "[gameplay][ai]") {
    UWorld world;
    auto* hunter = world.SpawnActor<ACharacter>();
    auto* prey = world.SpawnActor<ACharacter>();
    hunter->Reset({0.0f, 0.0f, 0.0f});
    prey->Reset({8.0f, 0.0f, 0.0f});

    AIController ai;
    ai.Possess(hunter);
    ai.SetArriveRadius(0.4f);
    ai.MoveToActor(prey);

    const glm::vec3 wish = ai.TickAI(0.016f);
    REQUIRE(wish.x > 0.5f);
    REQUIRE(ai.MoveActor() == prey);

    prey->Reset({0.2f, 0.0f, 0.0f});
    const glm::vec3 wishArrived = ai.TickAI(0.016f);
    REQUIRE_THAT(glm::length(wishArrived), WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("AIController path follow does not shortcut through blocker", "[gameplay][ai][nav]") {
    FPhysScene physics;
    FBodyInstance wall{};
    wall.type = EBodyType::Static;
    wall.position = {0.0f, 1.0f, 0.0f};
    wall.halfExtents = {0.6f, 1.5f, 4.0f};
    physics.Bodies().push_back(wall);

    UNavigationSystem nav;
    nav.SetCellSize(0.5f);
    nav.SetAgentRadius(0.45f);
    nav.BuildFromPhysScene(physics, 0.0f, 12.0f);
    REQUIRE(nav.HasNavMesh());

    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->Reset({-5.0f, 0.0f, 0.0f});

    AIController ai;
    ai.Possess(character);
    ai.SetNavigationSystem(&nav);
    // CoopTp-like large goal arrive — must not skip detour waypoints through the wall.
    ai.SetArriveRadius(1.25f);
    ai.MoveToLocation({5.0f, 0.0f, 0.0f});
    REQUIRE(ai.IsFollowingPath());
    REQUIRE(ai.PathPoints().size() >= 3);

    const glm::vec3 wish = ai.TickAI(0.016f);
    REQUIRE(glm::length(wish) > 0.5f);
    // Detour is off the X axis (around the wall), not a pure +X charge through it.
    REQUIRE(std::abs(wish.z) > 0.35f);
}

TEST_CASE("NavigationSystem FindPath routes around static blocker", "[gameplay][nav]") {
    FPhysScene physics;

    // Floor plane-like slab (wide aspect) must NOT wipe the whole grid.
    FBodyInstance floor{};
    floor.type = EBodyType::Static;
    floor.position = {0.0f, 0.0f, 0.0f};
    floor.halfExtents = {20.0f, 0.5f, 20.0f};
    physics.Bodies().push_back(floor);

    FBodyInstance wall{};
    wall.type = EBodyType::Static;
    wall.position = {0.0f, 1.0f, 0.0f};
    wall.halfExtents = {0.6f, 1.5f, 5.0f};
    physics.Bodies().push_back(wall);

    UNavigationSystem nav;
    nav.SetCellSize(0.5f);
    nav.SetAgentRadius(0.35f);
    nav.BuildFromPhysScene(physics, 0.0f, 12.0f);
    REQUIRE(nav.HasNavMesh());
    REQUIRE(nav.WalkableCellCount() > 100);
    REQUIRE(nav.BlockerCount() == 1);

    std::vector<glm::vec3> path;
    REQUIRE(nav.FindPath({-6.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, path));
    REQUIRE(path.size() >= 3);

    bool detoured = false;
    for (const glm::vec3& p : path) {
        if (std::abs(p.z) > 1.25f) {
            detoured = true;
            break;
        }
    }
    REQUIRE(detoured);

    glm::vec3 projected{};
    REQUIRE(nav.ProjectPointToNavigation({-6.0f, 2.0f, 0.0f}, projected));
    REQUIRE_THAT(projected.y, WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("NavigationSystem blocks NavBlocker but keeps NavWalkable walkable", "[gameplay][nav]") {
    ULevel level;
    FPhysScene physics;

    UStaticMeshComponent plate{};
    plate.tag = NavTags::Blocker;
    plate.collisionEnabled = true;
    plate.editorClass = "Cube";
    plate.transform.Position = {0.0f, 0.12f, 0.0f};
    plate.transform.Scale = {1.8f, 0.2f, 1.8f};
    level.StaticMeshes().push_back(std::move(plate));

    FBodyInstance plateBody{};
    plateBody.type = EBodyType::Static;
    plateBody.levelMeshIndex = 0;
    plateBody.position = {0.0f, 0.12f, 0.0f};
    plateBody.halfExtents = {0.9f, 0.1f, 0.9f};
    physics.Bodies().push_back(plateBody);
    physics.TriangleMeshes().emplace_back();

    UStaticMeshComponent ramp{};
    ramp.tag = NavTags::Walkable;
    ramp.collisionEnabled = true;
    ramp.editorClass = "Cube";
    level.StaticMeshes().push_back(std::move(ramp));

    FBodyInstance rampBody{};
    rampBody.type = EBodyType::Static;
    rampBody.levelMeshIndex = 1;
    rampBody.position = {4.0f, 1.0f, 0.0f};
    rampBody.halfExtents = {2.5f, 1.0f, 1.2f};
    rampBody.collisionShape = ECollisionShape::TriangleMesh;
    physics.Bodies().push_back(rampBody);

    FTriangleMeshCollision tri{};
    // Two tris covering a 4x2 footprint around (4,0).
    tri.positions = {
        {2.0f, 0.5f, -1.0f}, {6.0f, 1.5f, -1.0f}, {6.0f, 1.5f, 1.0f}, {2.0f, 0.5f, 1.0f}};
    tri.indices = {0, 1, 2, 0, 2, 3};
    physics.TriangleMeshes().push_back(std::move(tri));

    UNavigationSystem nav;
    nav.SetCellSize(0.5f);
    nav.SetAgentRadius(0.35f);
    nav.BuildFromLevel(level, physics, 0.0f, 12.0f);
    REQUIRE(nav.HasNavMesh());
    REQUIRE(nav.BlockerCount() == 1);

    // Cell under plate center must be blocked.
    int pix = 0;
    int piz = 0;
    REQUIRE(nav.GetNavMesh().WorldToCell(0.0f, 0.0f, pix, piz));
    REQUIRE_FALSE(nav.GetNavMesh().IsWalkable(pix, piz));

    // Path across the plate must detour.
    std::vector<glm::vec3> path;
    REQUIRE(nav.FindPath({-3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, path));
    bool detouredPlate = false;
    for (const glm::vec3& p : path) {
        if (std::abs(p.z) > 0.8f) {
            detouredPlate = true;
            break;
        }
    }
    REQUIRE(detouredPlate);

    // Climbable ramp footprint stays walkable (CMC handles the slope).
    int rix = 0;
    int riz = 0;
    REQUIRE(nav.GetNavMesh().WorldToCell(4.0f, 0.0f, rix, riz));
    REQUIRE(nav.GetNavMesh().IsWalkable(rix, riz));
    REQUIRE(nav.FindPath({2.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, path));
}

TEST_CASE("NavigationSystem AppendDebugDraw fills overlay", "[gameplay][nav][debug]") {
    FPhysScene physics;
    FBodyInstance wall{};
    wall.type = EBodyType::Static;
    wall.position = {0.0f, 1.0f, 0.0f};
    wall.halfExtents = {0.5f, 1.0f, 0.5f};
    physics.Bodies().push_back(wall);

    UNavigationSystem nav;
    nav.SetCellSize(1.0f);
    nav.BuildFromPhysScene(physics, 0.0f, 4.0f);
    REQUIRE(nav.HasNavMesh());

    FDebugDraw draw;
    REQUIRE(draw.IsEmpty());
    nav.AppendDebugDraw(draw);
    REQUIRE_FALSE(draw.IsEmpty());
}

TEST_CASE("Character Reset Jump and PerformMovement", "[gameplay][character]") {
    ACharacter character;
    character.Reset({0.0f, 0.0f, 0.0f}, 45.0f);
    REQUIRE(character.IsMovingOnGround());
    REQUIRE_THAT(character.GetActorYaw(), WithinAbs(45.0f, 1.0e-5f));

    FPhysScene scene;
    character.Jump();
    character.PerformMovement(scene, 1.0f / 60.0f);
    REQUIRE_FALSE(character.IsMovingOnGround());
    REQUIRE(character.IsFalling());
    REQUIRE(character.GetActorLocation().y > 0.0f);

    character.AddMovementInput({1.0f, 0.0f, 0.0f});
    const float x0 = character.GetActorLocation().x;
    character.PerformMovement(scene, 1.0f / 60.0f);
    REQUIRE(character.GetActorLocation().x > x0);
}

TEST_CASE("DefaultGameMode Matches empty or Default id", "[gameplay][gamemode]") {
    ADefaultGameMode mode;
    FLevelEntry entry{};
    REQUIRE(mode.Matches(entry, ""));
    REQUIRE(mode.Matches(entry, "Default"));
    REQUIRE_FALSE(mode.Matches(entry, "Showcase"));
    REQUIRE(std::string(mode.Id()) == "Default");
}

TEST_CASE("Actor SyncTransformToLevel writes linked mesh", "[gameplay][actor][sync]") {
    ULevel level;
    UStaticMeshComponent mesh{};
    mesh.transform.Position = {0.0f, 0.0f, 0.0f};
    mesh.transform.RotationDegrees = {0.0f, 0.0f, 0.0f};
    level.AddStaticMesh(std::move(mesh));

    UWorld world;
    auto* actor = world.SpawnActor<TestActor>();
    actor->SetLevelMeshIndex(0);
    actor->SetActorLocationAndRotation({3.0f, 1.5f, -2.0f}, 90.0f);
    actor->SyncTransformToLevel(level);

    REQUIRE_THAT(level.StaticMeshes()[0].transform.Position.x, WithinAbs(3.0f, 1.0e-5f));
    REQUIRE_THAT(level.StaticMeshes()[0].transform.Position.y, WithinAbs(1.5f, 1.0e-5f));
    REQUIRE_THAT(level.StaticMeshes()[0].transform.Position.z, WithinAbs(-2.0f, 1.0e-5f));
    REQUIRE_THAT(level.StaticMeshes()[0].transform.RotationDegrees.y, WithinAbs(90.0f, 1.0e-5f));
}

TEST_CASE("World TickGameplayFrame syncs Character to Level mesh", "[gameplay][world][sync]") {
    ULevel level;
    UStaticMeshComponent mesh{};
    level.AddStaticMesh(std::move(mesh));

    UWorld world;
    auto* character = world.SpawnActor<ACharacter>();
    character->SetLevelMeshIndex(0);
    character->Reset({1.0f, 0.0f, 2.0f}, 45.0f);

    FWorldGameplayFrameParams frame{};
    frame.deltaTime = 1.0f / 60.0f;
    frame.level = &level;
    world.TickGameplayFrame(frame);

    REQUIRE_THAT(level.StaticMeshes()[0].transform.Position.x, WithinAbs(1.0f, 1.0e-4f));
    REQUIRE_THAT(level.StaticMeshes()[0].transform.Position.z, WithinAbs(2.0f, 1.0e-4f));
    REQUIRE_THAT(level.StaticMeshes()[0].transform.RotationDegrees.y, WithinAbs(45.0f, 1.0e-4f));
}

TEST_CASE("ActorComponent RegisterComponent and CreateDefaultSubobject tick",
          "[gameplay][actorcomponent]") {
    struct CountingComponent : UActorComponent {
        int ticks = 0;
        int begins = 0;
        void BeginPlay() override { ++begins; }
        void TickComponent(float) override { ++ticks; }
    };

    UWorld world;
    auto* actor = world.SpawnActor<TestActor>();
    REQUIRE(actor->GetComponents().size() >= 1); // root

    CountingComponent* heap = actor->CreateDefaultSubobject<CountingComponent>();
    REQUIRE(heap != nullptr);
    REQUIRE(heap->GetOwner() == actor);
    REQUIRE(heap->IsRegistered());
    heap->SetComponentTickEnabled(true);

    // BeginPlayComponents runs on spawn before Actor::BeginPlay.
    REQUIRE(heap->begins == 1);

    world.Tick(1.0f / 60.0f);
    REQUIRE(heap->ticks == 1);

    heap->DestroyComponent();
    REQUIRE_FALSE(heap->IsRegistered());
    world.Tick(1.0f / 60.0f);
    REQUIRE(heap->ticks == 1); // unregistered: no further ticks
}

TEST_CASE("SceneComponent attach hierarchy world transform", "[gameplay][scenecomponent]") {
    UWorld world;
    auto* actor = world.SpawnActor<TestActor>();
    actor->SetActorLocationAndRotation({10.0f, 0.0f, 0.0f}, 0.0f);

    USceneComponent child;
    child.SetOwner(actor);
    child.RelativeLocation = {2.0f, 0.0f, 0.0f};
    REQUIRE(child.AttachToComponent(&actor->GetRootComponent()));
    REQUIRE(child.GetAttachParent() == &actor->GetRootComponent());
    REQUIRE(actor->GetRootComponent().GetAttachChildren().size() == 1);

    const glm::vec3 loc = child.GetComponentLocation();
    REQUIRE_THAT(loc.x, WithinAbs(12.0f, 1.0e-4f));

    USceneComponent grandchild;
    grandchild.RelativeLocation = {1.0f, 0.0f, 0.0f};
    REQUIRE(grandchild.AttachToComponent(&child));
    REQUIRE_THAT(grandchild.GetComponentLocation().x, WithinAbs(13.0f, 1.0e-4f));

    REQUIRE_FALSE(child.AttachToComponent(&grandchild)); // cycle
    child.DestroyComponent();
    REQUIRE(child.GetAttachParent() == nullptr);
    REQUIRE(actor->GetRootComponent().GetAttachChildren().empty());
}

TEST_CASE("Character mesh attaches to root SceneComponent",
          "[gameplay][character][scenecomponent]") {
    ACharacter character;
    REQUIRE(character.GetMesh().GetAttachParent() == &character.GetRootComponent());
    REQUIRE(character.GetMesh().GetOwner() == &character);
    REQUIRE(character.GetMesh().IsRegistered());
    REQUIRE(character.GetRootComponent().IsRegistered());
    character.SetActorLocation({5.0f, 0.0f, 0.0f});
    character.GetMesh().RelativeLocation = {1.0f, 0.0f, 0.0f};
    REQUIRE_THAT(character.GetMesh().GetComponentLocation().x, WithinAbs(6.0f, 1.0e-4f));
}

TEST_CASE("PhysScene reports Arcade backend by default", "[physics][backend]") {
    FPhysScene scene;
    REQUIRE(scene.GetBackend() == EPhysicsBackend::Arcade);
    REQUIRE(std::string(PhysicsBackendName(scene.GetBackend())) == "Arcade");
}

TEST_CASE("RootReplication capture and apply Actor root", "[net][replication]") {
    UWorld world;
    auto* actor = world.SpawnActor<TestActor>();
    actor->SetActorLocationAndRotation({1.0f, 2.0f, 3.0f}, 45.0f);
    const Leon::Net::FPawnSnap snap = Leon::Net::CaptureActorRoot(0, *actor, 1.5f, 0.25f);
    REQUIRE_THAT(snap.x, WithinAbs(1.0f, 1.0e-5f));
    REQUIRE_THAT(snap.yaw, WithinAbs(45.0f, 1.0e-5f));

    auto* other = world.SpawnActor<TestActor>();
    Leon::Net::ApplyActorRoot(*other, snap);
    REQUIRE_THAT(other->GetActorLocation().z, WithinAbs(3.0f, 1.0e-5f));
    REQUIRE_THAT(other->GetActorYaw(), WithinAbs(45.0f, 1.0e-5f));
}
