#include "GameFramework/GameModeBase.h"

#include "AI/Navigation/NavigationSystem.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

#include <algorithm>
#include <cmath>
#include <iostream>

void AGameModeBase::PostLogin(APlayerController& NewPlayer)
{
	GetGameState().AddPlayerState(&NewPlayer.GetPlayerState());
}

void AGameModeBase::Logout(APlayerController& Exiting)
{
	GetGameState().RemovePlayerState(&Exiting.GetPlayerState());
}

float AGameModeBase::EstimateFloorY(const ULevel& Level)
{
	const auto& Starts = Level.GetPlayerStarts();
	if (Starts.empty())
	{
		return 0.0f;
	}
	float Y = Starts.front().Transform.Position.y;
	for (const FPlayerStart& Start : Starts)
	{
		Y = std::min(Y, Start.Transform.Position.y);
	}
	return Y;
}

float AGameModeBase::EstimateWalkBounds(const ULevel& Level)
{
	float MaxExtent = 40.0f;
	for (const UStaticMeshComponent& Mesh : Level.GetStaticMeshes())
	{
		if (!Mesh.HasPhysicsBody() || Mesh.bSimulatePhysics)
		{
			continue;
		}
		const float Hx = std::abs(Mesh.Transform.Scale.x) * 0.5f;
		const float Hz = std::abs(Mesh.Transform.Scale.z) * 0.5f;
		MaxExtent = std::max(MaxExtent, std::max(Hx, Hz));
	}
	return std::clamp(MaxExtent - 1.0f, 20.0f, 120.0f);
}

// Flow: Match enter — bodies + nav bake
// 1. Physics backend
// 2. Estimate floor Y / walk bounds from level
// 3. RegisterBodiesFromLevel + SyncFromLevel
// 4. UNavigationSystem bake (cell 0.5, agent 0.45)
void AGameModeBase::PrepareMatchWorld(
	UGameEngine& Engine, float& OutFloorY, float& OutWalkBounds, EPhysicsBackend Backend)
{
	SetPhysicsBackend(Backend);
	OutFloorY = EstimateFloorY(Engine.GetLevel());
	OutWalkBounds = EstimateWalkBounds(Engine.GetLevel());
	RegisterBodiesFromLevel(Engine.GetLevel());
	GetWorld().GetPhysicsScene().SyncFromLevel(Engine.GetLevel());

	UNavigationSystem& Nav = GetWorld().GetNavigationSystem();
	Nav.SetCellSize(0.5f);
	Nav.SetAgentRadius(0.45f);
	Nav.BuildFromLevel(Engine.GetLevel(), GetWorld().GetPhysicsScene(), OutFloorY, OutWalkBounds);
	std::cout << "GameMode: NavMesh bake blockers=" << Nav.GetBlockerCount()
			  << " walkable=" << Nav.GetWalkableCellCount() << "/" << (Nav.GetNavMesh().Width * Nav.GetNavMesh().Depth)
			  << " cell=" << Nav.GetCellSize() << '\n';
}

void AGameModeBase::RebuildNavigation(UGameEngine& Engine, float FloorY, float WalkBounds)
{
	RegisterBodiesFromLevel(Engine.GetLevel());
	GetWorld().GetPhysicsScene().SyncFromLevel(Engine.GetLevel());
	UNavigationSystem& Nav = GetWorld().GetNavigationSystem();
	Nav.BuildFromLevel(Engine.GetLevel(), GetWorld().GetPhysicsScene(), FloorY, WalkBounds);
}

void AGameModeBase::SnapCharacterToFloor(ACharacter& Character, glm::vec3& InOutFeet, float FloorY) const
{
	const FPhysScene& Phys = GetWorld().GetPhysicsScene();
	const UCharacterMovementComponent& Move = Character.GetCharacterMovement();
	glm::vec3 Probe = InOutFeet;
	Probe.y = std::max(InOutFeet.y, FloorY);
	const float Support = Phys.QuerySupportY(
		Character.GetCapsule(), Probe, Move.FloorY, Move.MaxStepHeight, Move.Skin, Character.GetLevelMeshIndex());
	InOutFeet.y = std::max(Support, FloorY) + 0.02f;
}
