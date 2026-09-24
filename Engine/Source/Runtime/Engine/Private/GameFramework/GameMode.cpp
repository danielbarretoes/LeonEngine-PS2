#include "GameFramework/GameMode.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include "Engine/GameEngine.h"
#include "GameFramework/Character.h"
#include "Engine/GameInstance.h"
#include "AI/Navigation/NavigationSystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Level.h"


void GameMode::PostLogin(PlayerController& newPlayer) {
    GetGameState().AddPlayerState(&newPlayer.GetPlayerState());
}

void GameMode::Logout(PlayerController& exiting) {
    GetGameState().RemovePlayerState(&exiting.GetPlayerState());
}

bool GameMode::ServerTravel(Engine& engine, std::string_view mapName,
                            std::string_view hintLevelPath) {
    if (!engine.GetGameInstance().ServerTravel(engine, mapName, hintLevelPath)) {
        return false;
    }
    if (!engine.GetLevel().Name().empty()) {
        GetGameState().SetMapName(engine.GetLevel().Name());
    } else {
        GetGameState().SetMapName(std::string(mapName));
    }
    return true;
}

bool GameMode::ClientTravel(Engine& engine, std::string_view mapName,
                            std::string_view hintLevelPath) {
    if (!engine.GetGameInstance().ClientTravel(engine, mapName, hintLevelPath)) {
        return false;
    }
    if (!engine.GetLevel().Name().empty()) {
        GetGameState().SetMapName(engine.GetLevel().Name());
    } else {
        GetGameState().SetMapName(std::string(mapName));
    }
    return true;
}

float GameMode::EstimateFloorY(const Level& level) {
    const auto& starts = level.PlayerStarts();
    if (starts.empty()) {
        return 0.0f;
    }
    float y = starts.front().transform.position.y;
    for (const PlayerStart& start : starts) {
        y = std::min(y, start.transform.position.y);
    }
    return y;
}

float GameMode::EstimateWalkBounds(const Level& level) {
    float maxExtent = 40.0f;
    for (const StaticMeshComponent& mesh : level.StaticMeshes()) {
        if (!mesh.HasPhysicsBody() || mesh.simulatePhysics) {
            continue;
        }
        const float hx = std::abs(mesh.transform.scale.x) * 0.5f;
        const float hz = std::abs(mesh.transform.scale.z) * 0.5f;
        maxExtent = std::max(maxExtent, std::max(hx, hz));
    }
    return std::clamp(maxExtent - 1.0f, 20.0f, 120.0f);
}

// Flow: Match enter — bodies + nav bake
// 1. Physics backend
// 2. Estimate floor Y / walk bounds from level
// 3. RegisterBodiesFromLevel + SyncFromLevel
// 4. NavigationSystem bake (cell 0.5, agent 0.45)
void GameMode::PrepareMatchWorld(Engine& engine, float& outFloorY, float& outWalkBounds,
                                 EPhysicsBackend backend) {
    SetPhysicsBackend(backend);
    outFloorY = EstimateFloorY(engine.GetLevel());
    outWalkBounds = EstimateWalkBounds(engine.GetLevel());
    RegisterBodiesFromLevel(engine.GetLevel());
    GetWorld().GetPhysicsScene().SyncFromLevel(engine.GetLevel());

    NavigationSystem& nav = GetWorld().GetNavigationSystem();
    nav.SetCellSize(0.5f);
    nav.SetAgentRadius(0.45f);
    nav.BuildFromLevel(engine.GetLevel(), GetWorld().GetPhysicsScene(), outFloorY, outWalkBounds);
    std::cout << "GameMode: NavMesh bake blockers=" << nav.BlockerCount()
              << " walkable=" << nav.WalkableCellCount() << "/"
              << (nav.GetNavMesh().width * nav.GetNavMesh().depth) << " cell=" << nav.CellSize()
              << '\n';
}

void GameMode::RebuildNavigation(Engine& engine, float floorY, float walkBounds) {
    RegisterBodiesFromLevel(engine.GetLevel());
    GetWorld().GetPhysicsScene().SyncFromLevel(engine.GetLevel());
    NavigationSystem& nav = GetWorld().GetNavigationSystem();
    nav.BuildFromLevel(engine.GetLevel(), GetWorld().GetPhysicsScene(), floorY, walkBounds);
}

void GameMode::SnapCharacterToFloor(Character& character, glm::vec3& inOutFeet,
                                    float floorY) const {
    const PhysScene& phys = GetWorld().GetPhysicsScene();
    const CharacterMovement& move = character.GetCharacterMovement();
    glm::vec3 probe = inOutFeet;
    probe.y = std::max(inOutFeet.y, floorY);
    const float support =
        phys.QuerySupportY(character.GetCapsule(), probe, move.FloorY, move.MaxStepHeight,
                           move.Skin, character.LevelMeshIndex());
    inOutFeet.y = std::max(support, floorY) + 0.02f;
}

