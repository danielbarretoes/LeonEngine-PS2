#include "AI/Navigation/NavigationSystem.h"
#include "AIController.h"
#include "BodyInstance.h"
#include "Components/SceneComponent.h"
#include "Debug/DebugDraw.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DefaultGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Level/LevelCatalog.h"
#include "Net/RootReplication.h"
#include "Physics/PhysScene.h"
#include "PhysicsBackend.h"
#include "TriangleCollision.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/geometric.hpp>

#include <cmath>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{

	class ATestActor : public AActor
	{
	};
	class ATestPawn : public APawn
	{
	};
	class ATestController : public AController
	{
	};

} // namespace

TEST_CASE("World spawns ticks and destroys actors", "[gameplay][world]")
{
	UWorld World;
	auto* Actor = World.SpawnActor<ATestActor>();
	REQUIRE(Actor != nullptr);
	REQUIRE(World.ActorCount() == 1);
	REQUIRE(Actor->GetWorld() == &World);

	Actor->SetActorLocation({1.0f, 2.0f, 3.0f});
	REQUIRE_THAT(Actor->GetActorLocation().y, WithinAbs(2.0f, 1.0e-5f));

	World.DestroyActor(Actor);
	REQUIRE(Actor->IsPendingKillPending());
	World.Tick(0.016f);
	REQUIRE(World.ActorCount() == 0);

	World.SpawnActor<ATestActor>();
	World.Clear();
	REQUIRE(World.ActorCount() == 0);
}

TEST_CASE("World FindFirst finds derived type", "[gameplay][world]")
{
	UWorld World;
	World.SpawnActor<ATestActor>();
	auto* Pawn = World.SpawnActor<ATestPawn>();
	REQUIRE(World.FindFirst<ATestPawn>() == Pawn);
	REQUIRE(World.FindFirst<ACharacter>() == nullptr);
}

TEST_CASE("Controller Possess and UnPossess", "[gameplay][controller]")
{
	UWorld World;
	auto* Pawn = World.SpawnActor<ATestPawn>();
	ATestController Controller;
	Controller.Possess(Pawn);
	REQUIRE(Controller.HasPawn());
	REQUIRE(Pawn->IsPossessed());
	REQUIRE(Pawn->GetController() == &Controller);

	Controller.UnPossess();
	REQUIRE_FALSE(Controller.HasPawn());
	REQUIRE_FALSE(Pawn->IsPossessed());
}

TEST_CASE("Pawn Destroy UnPossesses controller", "[gameplay][pawn]")
{
	UWorld World;
	auto* Pawn = World.SpawnActor<ATestPawn>();
	ATestController Controller;
	Controller.Possess(Pawn);
	Pawn->Destroy();
	World.Tick(0.0f);
	REQUIRE_FALSE(Controller.HasPawn());
}

TEST_CASE("GameState match timer and PlayerState score", "[gameplay][state]")
{
	AGameStateBase Gs;
	Gs.HandleMatchHasStarted();
	Gs.Tick(0.5f);
	REQUIRE(Gs.HasMatchStarted());
	REQUIRE_THAT(Gs.GetServerWorldTimeSeconds(), WithinAbs(0.5f, 1.0e-5f));
	Gs.Reset();
	REQUIRE_THAT(Gs.GetServerWorldTimeSeconds(), WithinAbs(0.0f, 1.0e-5f));
	REQUIRE_FALSE(Gs.HasMatchStarted());

	APlayerState Ps;
	Ps.SetPlayerId(2);
	Ps.SetPlayerName("P2");
	Ps.AddScore(10.0f);
	REQUIRE(Ps.GetPlayerId() == 2);
	REQUIRE(Ps.GetPlayerName() == "P2");
	REQUIRE_THAT(Ps.GetScore(), WithinAbs(10.0f, 1.0e-5f));
	Ps.Reset();
	REQUIRE_THAT(Ps.GetScore(), WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("GameInstance NotifyLevelOpened", "[gameplay][gameinstance]")
{
	UGameInstance Gi;
	REQUIRE(Gi.GetLevelsOpened() == 0);
	Gi.NotifyLevelOpened();
	Gi.NotifyLevelOpened();
	REQUIRE(Gi.GetLevelsOpened() == 2);
}

TEST_CASE("SpringArmComponent clamps pitch and arm length", "[gameplay][springarm]")
{
	USpringArmComponent Arm;
	Arm.AddPitchInput(200.0f);
	REQUIRE(Arm.BoomPitchDegrees <= Arm.PitchMax);
	Arm.AddPitchInput(-400.0f);
	REQUIRE(Arm.BoomPitchDegrees >= Arm.PitchMin);

	Arm.AddArmLengthInput(100.0f);
	REQUIRE_THAT(Arm.TargetArmLength, WithinAbs(Arm.ArmLengthMax, 1.0e-5f));

	Arm.SnapLagState({0.0f, 0.0f, 0.0f});
	UCameraComponent Camera;
	Arm.ApplyToCamera(Camera, {1.0f, 0.0f, 0.0f}, 0.016f);
	REQUIRE(Camera.GetMode() == ECameraMode::Orbit);
	REQUIRE_THAT(Camera.GetTarget().y, WithinAbs(Arm.SocketOffsetZ, 0.5f));
}

TEST_CASE("SpringArmComponent collision probe shortens arm", "[gameplay][springarm]")
{
	FPhysScene Scene;
	const std::size_t Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = {2.0f, 1.0f, 0.0f};
	Scene.GetBodies()[Id].HalfExtents = {0.25f, 1.0f, 2.0f};

	USpringArmComponent Arm;
	Arm.bDoCollisionTest = true;
	Arm.bEnableCameraLag = false;
	Arm.bEnableCameraRotationLag = false;
	Arm.ArmLengthLagSpeed = 1000.0f;
	Arm.TargetArmLength = 6.0f;
	Arm.ArmLengthMin = 0.5f;
	Arm.BoomYawDegrees = 0.0f;
	Arm.BoomPitchDegrees = 0.0f;
	Arm.SocketOffsetZ = 1.0f;
	Arm.SocketOffsetX = 0.0f;
	Arm.ProbeSize = 0.15f;
	Arm.CollisionProbeOffset = 0.05f;
	Arm.SnapLagState({0.0f, 0.0f, 0.0f});

	UCameraComponent Camera;
	Arm.ApplyToCamera(Camera, {0.0f, 0.0f, 0.0f}, 0.016f, &Scene);
	REQUIRE(Camera.GetDistance() < 3.0f);
	REQUIRE(Camera.GetDistance() >= Arm.ArmLengthMin);
}

TEST_CASE("AIController steers toward target and arrives", "[gameplay][ai]")
{
	UWorld World;
	auto* Character = World.SpawnActor<ACharacter>();
	Character->Reset({0.0f, 0.0f, 0.0f});

	AAIController Ai;
	Ai.Possess(Character);
	Ai.SetArriveRadius(0.5f);
	Ai.MoveToLocation({10.0f, 0.0f, 0.0f});

	const glm::vec3 WishFar = Ai.TickAI(0.016f);
	REQUIRE_THAT(glm::length(WishFar), WithinAbs(1.0f, 1.0e-3f));
	REQUIRE(WishFar.x > 0.5f);

	Character->Reset({10.0f, 0.0f, 0.0f});
	const glm::vec3 WishNear = Ai.TickAI(0.016f);
	REQUIRE_THAT(glm::length(WishNear), WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("AIController MoveToActor tracks moving target", "[gameplay][ai]")
{
	UWorld World;
	auto* Hunter = World.SpawnActor<ACharacter>();
	auto* Prey = World.SpawnActor<ACharacter>();
	Hunter->Reset({0.0f, 0.0f, 0.0f});
	Prey->Reset({8.0f, 0.0f, 0.0f});

	AAIController Ai;
	Ai.Possess(Hunter);
	Ai.SetArriveRadius(0.4f);
	Ai.MoveToActor(Prey);

	const glm::vec3 Wish = Ai.TickAI(0.016f);
	REQUIRE(Wish.x > 0.5f);
	REQUIRE(Ai.GetMoveActor() == Prey);

	Prey->Reset({0.2f, 0.0f, 0.0f});
	const glm::vec3 WishArrived = Ai.TickAI(0.016f);
	REQUIRE_THAT(glm::length(WishArrived), WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("AIController path follow does not shortcut through blocker", "[gameplay][ai][nav]")
{
	FPhysScene Physics;
	FBodyInstance Wall{};
	Wall.Type = EBodyType::Static;
	Wall.Position = {0.0f, 1.0f, 0.0f};
	Wall.HalfExtents = {0.6f, 1.5f, 4.0f};
	Physics.GetBodies().push_back(Wall);

	UNavigationSystem Nav;
	Nav.SetCellSize(0.5f);
	Nav.SetAgentRadius(0.45f);
	Nav.BuildFromPhysScene(Physics, 0.0f, 12.0f);
	REQUIRE(Nav.HasNavMesh());

	UWorld World;
	auto* Character = World.SpawnActor<ACharacter>();
	Character->Reset({-5.0f, 0.0f, 0.0f});

	AAIController Ai;
	Ai.Possess(Character);
	Ai.SetNavigationSystem(&Nav);
	// CoopTp-like large goal arrive — must not skip detour waypoints through the wall.
	Ai.SetArriveRadius(1.25f);
	Ai.MoveToLocation({5.0f, 0.0f, 0.0f});
	REQUIRE(Ai.IsFollowingPath());
	REQUIRE(Ai.PathPoints().size() >= 3);

	const glm::vec3 Wish = Ai.TickAI(0.016f);
	REQUIRE(glm::length(Wish) > 0.5f);
	// Detour is off the X axis (around the wall), not a pure +X charge through it.
	REQUIRE(std::abs(Wish.z) > 0.35f);
}

TEST_CASE("NavigationSystem FindPath routes around static blocker", "[gameplay][nav]")
{
	FPhysScene Physics;

	// Floor plane-like slab (wide aspect) must NOT wipe the whole grid.
	FBodyInstance Floor{};
	Floor.Type = EBodyType::Static;
	Floor.Position = {0.0f, 0.0f, 0.0f};
	Floor.HalfExtents = {20.0f, 0.5f, 20.0f};
	Physics.GetBodies().push_back(Floor);

	FBodyInstance Wall{};
	Wall.Type = EBodyType::Static;
	Wall.Position = {0.0f, 1.0f, 0.0f};
	Wall.HalfExtents = {0.6f, 1.5f, 5.0f};
	Physics.GetBodies().push_back(Wall);

	UNavigationSystem Nav;
	Nav.SetCellSize(0.5f);
	Nav.SetAgentRadius(0.35f);
	Nav.BuildFromPhysScene(Physics, 0.0f, 12.0f);
	REQUIRE(Nav.HasNavMesh());
	REQUIRE(Nav.GetWalkableCellCount() > 100);
	REQUIRE(Nav.GetBlockerCount() == 1);

	std::vector<glm::vec3> Path;
	REQUIRE(Nav.FindPath({-6.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, Path));
	REQUIRE(Path.size() >= 3);

	bool bDetoured = false;
	for (const glm::vec3& P : Path)
	{
		if (std::abs(P.z) > 1.25f)
		{
			bDetoured = true;
			break;
		}
	}
	REQUIRE(bDetoured);

	glm::vec3 Projected{};
	REQUIRE(Nav.ProjectPointToNavigation({-6.0f, 2.0f, 0.0f}, Projected));
	REQUIRE_THAT(Projected.y, WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("NavigationSystem blocks NavBlocker but keeps NavWalkable walkable", "[gameplay][nav]")
{
	ULevel Level;
	FPhysScene Physics;

	UStaticMeshComponent Plate{};
	Plate.Tag = NavTags::Blocker;
	Plate.bCollisionEnabled = true;
	Plate.EditorClass = "Cube";
	Plate.Transform.Position = {0.0f, 0.12f, 0.0f};
	Plate.Transform.Scale = {1.8f, 0.2f, 1.8f};
	Level.GetStaticMeshes().push_back(std::move(Plate));

	FBodyInstance PlateBody{};
	PlateBody.Type = EBodyType::Static;
	PlateBody.LevelMeshIndex = 0;
	PlateBody.Position = {0.0f, 0.12f, 0.0f};
	PlateBody.HalfExtents = {0.9f, 0.1f, 0.9f};
	Physics.GetBodies().push_back(PlateBody);
	Physics.GetTriangleMeshes().emplace_back();

	UStaticMeshComponent Ramp{};
	Ramp.Tag = NavTags::Walkable;
	Ramp.bCollisionEnabled = true;
	Ramp.EditorClass = "Cube";
	Level.GetStaticMeshes().push_back(std::move(Ramp));

	FBodyInstance RampBody{};
	RampBody.Type = EBodyType::Static;
	RampBody.LevelMeshIndex = 1;
	RampBody.Position = {4.0f, 1.0f, 0.0f};
	RampBody.HalfExtents = {2.5f, 1.0f, 1.2f};
	RampBody.CollisionShape = ECollisionShape::TriangleMesh;
	Physics.GetBodies().push_back(RampBody);

	FTriangleMeshCollision Tri{};
	// Two tris covering a 4x2 footprint around (4,0).
	Tri.Positions = {{2.0f, 0.5f, -1.0f}, {6.0f, 1.5f, -1.0f}, {6.0f, 1.5f, 1.0f}, {2.0f, 0.5f, 1.0f}};
	Tri.Indices = {0, 1, 2, 0, 2, 3};
	Physics.GetTriangleMeshes().push_back(std::move(Tri));

	UNavigationSystem Nav;
	Nav.SetCellSize(0.5f);
	Nav.SetAgentRadius(0.35f);
	Nav.BuildFromLevel(Level, Physics, 0.0f, 12.0f);
	REQUIRE(Nav.HasNavMesh());
	REQUIRE(Nav.GetBlockerCount() == 1);

	// Cell under plate center must be blocked.
	int Pix = 0;
	int Piz = 0;
	REQUIRE(Nav.GetNavMesh().WorldToCell(0.0f, 0.0f, Pix, Piz));
	REQUIRE_FALSE(Nav.GetNavMesh().IsWalkable(Pix, Piz));

	// Path across the plate must detour.
	std::vector<glm::vec3> Path;
	REQUIRE(Nav.FindPath({-3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, Path));
	bool bDetouredPlate = false;
	for (const glm::vec3& P : Path)
	{
		if (std::abs(P.z) > 0.8f)
		{
			bDetouredPlate = true;
			break;
		}
	}
	REQUIRE(bDetouredPlate);

	// Climbable ramp footprint stays walkable (CMC handles the slope).
	int Rix = 0;
	int Riz = 0;
	REQUIRE(Nav.GetNavMesh().WorldToCell(4.0f, 0.0f, Rix, Riz));
	REQUIRE(Nav.GetNavMesh().IsWalkable(Rix, Riz));
	REQUIRE(Nav.FindPath({2.0f, 0.0f, 0.0f}, {6.0f, 0.0f, 0.0f}, Path));
}

TEST_CASE("NavigationSystem AppendDebugDraw fills overlay", "[gameplay][nav][debug]")
{
	FPhysScene Physics;
	FBodyInstance Wall{};
	Wall.Type = EBodyType::Static;
	Wall.Position = {0.0f, 1.0f, 0.0f};
	Wall.HalfExtents = {0.5f, 1.0f, 0.5f};
	Physics.GetBodies().push_back(Wall);

	UNavigationSystem Nav;
	Nav.SetCellSize(1.0f);
	Nav.BuildFromPhysScene(Physics, 0.0f, 4.0f);
	REQUIRE(Nav.HasNavMesh());

	FDebugDraw Draw;
	REQUIRE(Draw.IsEmpty());
	Nav.AppendDebugDraw(Draw);
	REQUIRE_FALSE(Draw.IsEmpty());
}

TEST_CASE("Character Reset Jump and PerformMovement", "[gameplay][character]")
{
	ACharacter Character;
	Character.Reset({0.0f, 0.0f, 0.0f}, 45.0f);
	REQUIRE(Character.IsMovingOnGround());
	REQUIRE_THAT(Character.GetActorYaw(), WithinAbs(45.0f, 1.0e-5f));

	FPhysScene Scene;
	Character.Jump();
	Character.PerformMovement(Scene, 1.0f / 60.0f);
	REQUIRE_FALSE(Character.IsMovingOnGround());
	REQUIRE(Character.IsFalling());
	REQUIRE(Character.GetActorLocation().y > 0.0f);

	Character.AddMovementInput({1.0f, 0.0f, 0.0f});
	const float X0 = Character.GetActorLocation().x;
	Character.PerformMovement(Scene, 1.0f / 60.0f);
	REQUIRE(Character.GetActorLocation().x > X0);
}

TEST_CASE("DefaultGameMode Matches empty or Default id", "[gameplay][gamemode]")
{
	ADefaultGameMode Mode;
	FLevelEntry Entry{};
	REQUIRE(Mode.Matches(Entry, ""));
	REQUIRE(Mode.Matches(Entry, "Default"));
	REQUIRE_FALSE(Mode.Matches(Entry, "Showcase"));
	REQUIRE(std::string(Mode.Id()) == "Default");
}

TEST_CASE("Actor SyncTransformToLevel writes linked mesh", "[gameplay][actor][sync]")
{
	ULevel Level;
	UStaticMeshComponent Mesh{};
	Mesh.Transform.Position = {0.0f, 0.0f, 0.0f};
	Mesh.Transform.RotationDegrees = {0.0f, 0.0f, 0.0f};
	Level.AddStaticMesh(std::move(Mesh));

	UWorld World;
	auto* Actor = World.SpawnActor<ATestActor>();
	Actor->SetLevelMeshIndex(0);
	Actor->SetActorLocationAndRotation({3.0f, 1.5f, -2.0f}, 90.0f);
	Actor->SyncTransformToLevel(Level);

	REQUIRE_THAT(Level.GetStaticMeshes()[0].Transform.Position.x, WithinAbs(3.0f, 1.0e-5f));
	REQUIRE_THAT(Level.GetStaticMeshes()[0].Transform.Position.y, WithinAbs(1.5f, 1.0e-5f));
	REQUIRE_THAT(Level.GetStaticMeshes()[0].Transform.Position.z, WithinAbs(-2.0f, 1.0e-5f));
	REQUIRE_THAT(Level.GetStaticMeshes()[0].Transform.RotationDegrees.y, WithinAbs(90.0f, 1.0e-5f));
}

TEST_CASE("World TickGameplayFrame syncs Character to Level mesh", "[gameplay][world][sync]")
{
	ULevel Level;
	UStaticMeshComponent Mesh{};
	Level.AddStaticMesh(std::move(Mesh));

	UWorld World;
	auto* Character = World.SpawnActor<ACharacter>();
	Character->SetLevelMeshIndex(0);
	Character->Reset({1.0f, 0.0f, 2.0f}, 45.0f);

	FWorldGameplayFrameParams Frame{};
	Frame.DeltaTime = 1.0f / 60.0f;
	Frame.Level = &Level;
	World.TickGameplayFrame(Frame);

	REQUIRE_THAT(Level.GetStaticMeshes()[0].Transform.Position.x, WithinAbs(1.0f, 1.0e-4f));
	REQUIRE_THAT(Level.GetStaticMeshes()[0].Transform.Position.z, WithinAbs(2.0f, 1.0e-4f));
	REQUIRE_THAT(Level.GetStaticMeshes()[0].Transform.RotationDegrees.y, WithinAbs(45.0f, 1.0e-4f));
}

TEST_CASE("ActorComponent RegisterComponent and CreateDefaultSubobject tick", "[gameplay][actorcomponent]")
{
	struct UCountingComponent : UActorComponent
	{
		int Ticks = 0;
		int Begins = 0;
		void BeginPlay() override
		{
			++Begins;
		}
		void TickComponent(float) override
		{
			++Ticks;
		}
	};

	UWorld World;
	auto* Actor = World.SpawnActor<ATestActor>();
	REQUIRE(Actor->GetComponents().size() >= 1); // root

	UCountingComponent* Heap = Actor->CreateDefaultSubobject<UCountingComponent>();
	REQUIRE(Heap != nullptr);
	REQUIRE(Heap->GetOwner() == Actor);
	REQUIRE(Heap->IsRegistered());
	Heap->SetComponentTickEnabled(true);

	// BeginPlayComponents runs on spawn before Actor::BeginPlay.
	REQUIRE(Heap->Begins == 1);

	World.Tick(1.0f / 60.0f);
	REQUIRE(Heap->Ticks == 1);

	Heap->DestroyComponent();
	REQUIRE_FALSE(Heap->IsRegistered());
	World.Tick(1.0f / 60.0f);
	REQUIRE(Heap->Ticks == 1); // unregistered: no further ticks
}

TEST_CASE("SceneComponent attach hierarchy world transform", "[gameplay][scenecomponent]")
{
	UWorld World;
	auto* Actor = World.SpawnActor<ATestActor>();
	Actor->SetActorLocationAndRotation({10.0f, 0.0f, 0.0f}, 0.0f);

	USceneComponent Child;
	Child.SetOwner(Actor);
	Child.RelativeLocation = {2.0f, 0.0f, 0.0f};
	REQUIRE(Child.AttachToComponent(&Actor->GetRootComponent()));
	REQUIRE(Child.GetAttachParent() == &Actor->GetRootComponent());
	REQUIRE(Actor->GetRootComponent().GetAttachChildren().size() == 1);

	const glm::vec3 Loc = Child.GetComponentLocation();
	REQUIRE_THAT(Loc.x, WithinAbs(12.0f, 1.0e-4f));

	USceneComponent Grandchild;
	Grandchild.RelativeLocation = {1.0f, 0.0f, 0.0f};
	REQUIRE(Grandchild.AttachToComponent(&Child));
	REQUIRE_THAT(Grandchild.GetComponentLocation().x, WithinAbs(13.0f, 1.0e-4f));

	REQUIRE_FALSE(Child.AttachToComponent(&Grandchild)); // cycle
	Child.DestroyComponent();
	REQUIRE(Child.GetAttachParent() == nullptr);
	REQUIRE(Actor->GetRootComponent().GetAttachChildren().empty());
}

TEST_CASE("Character mesh attaches to root SceneComponent", "[gameplay][character][scenecomponent]")
{
	ACharacter Character;
	REQUIRE(Character.GetMesh().GetAttachParent() == &Character.GetRootComponent());
	REQUIRE(Character.GetMesh().GetOwner() == &Character);
	REQUIRE(Character.GetMesh().IsRegistered());
	REQUIRE(Character.GetRootComponent().IsRegistered());
	Character.SetActorLocation({5.0f, 0.0f, 0.0f});
	Character.GetMesh().RelativeLocation = {1.0f, 0.0f, 0.0f};
	REQUIRE_THAT(Character.GetMesh().GetComponentLocation().x, WithinAbs(6.0f, 1.0e-4f));
}

TEST_CASE("PhysScene reports Arcade backend by default", "[physics][backend]")
{
	FPhysScene Scene;
	REQUIRE(Scene.GetBackend() == EPhysicsBackend::Arcade);
	REQUIRE(std::string(PhysicsBackendName(Scene.GetBackend())) == "Arcade");
}

TEST_CASE("RootReplication capture and apply Actor root", "[net][replication]")
{
	UWorld World;
	auto* Actor = World.SpawnActor<ATestActor>();
	Actor->SetActorLocationAndRotation({1.0f, 2.0f, 3.0f}, 45.0f);
	const Leon::Net::FPawnSnap Snap = Leon::Net::CaptureActorRoot(0, *Actor, 1.5f, 0.25f);
	REQUIRE_THAT(Snap.X, WithinAbs(1.0f, 1.0e-5f));
	REQUIRE_THAT(Snap.Yaw, WithinAbs(45.0f, 1.0e-5f));

	auto* Other = World.SpawnActor<ATestActor>();
	Leon::Net::ApplyActorRoot(*Other, Snap);
	REQUIRE_THAT(Other->GetActorLocation().z, WithinAbs(3.0f, 1.0e-5f));
	REQUIRE_THAT(Other->GetActorYaw(), WithinAbs(45.0f, 1.0e-5f));
}
