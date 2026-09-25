#include "GameFramework/GameModeBase.h"

#include "AI/Navigation/NavigationSystem.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "EngineLogs.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

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
	if (Starts.Num() == 0)
	{
		return 0.0f;
	}
	float Y = Starts[0].Transform.GetLocation().Y;
	for (const FPlayerStart& Start : Starts)
	{
		Y = FMath::Min(Y, Start.Transform.GetLocation().Y);
	}
	return Y;
}

float AGameModeBase::EstimateWalkBounds(const ULevel& Level)
{
	// All in cm: the half size of a basic cube is half its scale times BasicShapeSize.
	constexpr float MinExtent = 4000.0f;
	constexpr float EdgeMargin = 100.0f;
	constexpr float MinWalkBounds = 2000.0f;
	constexpr float MaxWalkBounds = 12000.0f;
	float MaxExtent = MinExtent;
	for (const UStaticMeshComponent& Mesh : Level.GetStaticMeshes())
	{
		if (!Mesh.HasPhysicsBody() || Mesh.bSimulatePhysics)
		{
			continue;
		}
		const FVector Scale = Mesh.Transform.GetScale3D();
		const float Hx = FMath::Abs(Scale.X) * 0.5f * BasicShapeSize;
		const float Hz = FMath::Abs(Scale.Z) * 0.5f * BasicShapeSize;
		MaxExtent = FMath::Max(MaxExtent, FMath::Max(Hx, Hz));
	}
	return FMath::Clamp(MaxExtent - EdgeMargin, MinWalkBounds, MaxWalkBounds);
}

// Flow: Match enter — bodies + nav bake
// 1. Physics backend
// 2. Estimate floor Y / walk bounds from level
// 3. RegisterBodiesFromLevel + SyncFromLevel
// 4. UNavigationSystem bake (cell 50 cm, agent 45 cm)
void AGameModeBase::PrepareMatchWorld(
	UGameEngine& Engine, float& OutFloorY, float& OutWalkBounds, EPhysicsBackend Backend)
{
	SetPhysicsBackend(Backend);
	OutFloorY = EstimateFloorY(Engine.GetLevel());
	OutWalkBounds = EstimateWalkBounds(Engine.GetLevel());
	RegisterBodiesFromLevel(Engine.GetLevel());
	GetWorld().GetPhysicsScene().SyncFromLevel(Engine.GetLevel());

	UNavigationSystem& Nav = GetWorld().GetNavigationSystem();
	Nav.SetCellSize(50.0f);
	Nav.SetAgentRadius(45.0f);
	Nav.BuildFromLevel(Engine.GetLevel(), GetWorld().GetPhysicsScene(), OutFloorY, OutWalkBounds);
	UE_LOG(LogPath, Log, "GameMode: NavMesh bake blockers=%d walkable=%d/%d cell=%g", Nav.GetBlockerCount(),
		Nav.GetWalkableCellCount(), Nav.GetNavMesh().Width * Nav.GetNavMesh().Depth,
		static_cast<double>(Nav.GetCellSize()));
}

void AGameModeBase::RebuildNavigation(UGameEngine& Engine, float FloorY, float WalkBounds)
{
	RegisterBodiesFromLevel(Engine.GetLevel());
	GetWorld().GetPhysicsScene().SyncFromLevel(Engine.GetLevel());
	UNavigationSystem& Nav = GetWorld().GetNavigationSystem();
	Nav.BuildFromLevel(Engine.GetLevel(), GetWorld().GetPhysicsScene(), FloorY, WalkBounds);
}

void AGameModeBase::SnapCharacterToFloor(ACharacter& Character, FVector& InOutFeet, float FloorY) const
{
	const FPhysScene& Phys = GetWorld().GetPhysicsScene();
	const UCharacterMovementComponent& Move = Character.GetCharacterMovement();
	FVector Probe = InOutFeet;
	Probe.Y = FMath::Max(InOutFeet.Y, FloorY);
	const float Support = Phys.QuerySupportY(
		Character.GetCapsule(), Probe, Move.FloorY, Move.MaxStepHeight, Move.Skin, Character.GetLevelMeshIndex());
	/** cm above the support */
	constexpr float SnapClearance = 2.0f;
	InOutFeet.Y = FMath::Max(Support, FloorY) + SnapClearance;
}
