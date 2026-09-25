#include "GameFramework/GameModeBase.h"

#include "AI/Navigation/NavigationSystem.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "EngineLogs.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

AGameModeBase::AGameModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AGameStateBase::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	PlayerStateClass = APlayerState::StaticClass();
	DefaultPawnClass = APawn::StaticClass();
	HUDClass = AHUD::StaticClass();
}

void AGameModeBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	SpawnGameState();
}

void AGameModeBase::SpawnGameState()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (GameState != nullptr)
	{
		GameState->Destroy();
		GameState = nullptr;
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	GameState = World->SpawnActor<AGameStateBase>(
		GameStateClass != nullptr ? GameStateClass.Get() : AGameStateBase::StaticClass(), SpawnInfo);
	World->SetGameState(GameState);
	InitGameState();
}

void AGameModeBase::OnEnter(UGameEngine& /*Engine*/, const FString& /*LevelPath*/)
{
}

void AGameModeBase::OnExit(UGameEngine& /*Engine*/)
{
}

void AGameModeBase::Tick(UGameEngine& /*Engine*/, float DeltaTime)
{
	GetWorld()->Tick(DeltaTime);
}

void AGameModeBase::PostLogin(APlayerController& NewPlayer)
{
	GetGameState().AddPlayerState(NewPlayer.GetPlayerState<APlayerState>());
}

void AGameModeBase::Logout(APlayerController& Exiting)
{
	GetGameState().RemovePlayerState(Exiting.GetPlayerState<APlayerState>());
}

float AGameModeBase::EstimateFloorZ(const ULevel& Level)
{
	const auto& Starts = Level.GetPlayerStarts();
	if (Starts.Num() == 0)
	{
		return 0.0f;
	}
	float Z = Starts[0].Transform.GetLocation().Z;
	for (const FPlayerStart& Start : Starts)
	{
		Z = FMath::Min(Z, Start.Transform.GetLocation().Z);
	}
	return Z;
}

float AGameModeBase::EstimateWalkBounds(const ULevel& Level)
{
	// All in cm: the half size of a basic cube is half its scale times BasicShapeSize.
	constexpr float MinExtent = 4000.0f;
	constexpr float EdgeMargin = 100.0f;
	constexpr float MinWalkBounds = 2000.0f;
	constexpr float MaxWalkBounds = 12000.0f;
	float MaxExtent = MinExtent;
	for (const FLevelStaticMesh& Mesh : Level.GetStaticMeshes())
	{
		if (!Mesh.HasPhysicsBody() || Mesh.bSimulatePhysics)
		{
			continue;
		}
		const FVector Scale = Mesh.Transform.GetScale3D();
		const float Hx = FMath::Abs(Scale.X) * 0.5f * BasicShapeSize;
		const float Hy = FMath::Abs(Scale.Y) * 0.5f * BasicShapeSize;
		MaxExtent = FMath::Max(MaxExtent, FMath::Max(Hx, Hy));
	}
	return FMath::Clamp(MaxExtent - EdgeMargin, MinWalkBounds, MaxWalkBounds);
}

// Flow: Match enter — bodies + nav bake
// 1. Physics backend
// 2. Estimate floor Z / walk bounds from level
// 3. RegisterBodiesFromLevel + SyncFromLevel
// 4. UNavigationSystem bake (cell 50 cm, agent 45 cm)
void AGameModeBase::PrepareMatchWorld(
	UGameEngine& Engine, float& OutFloorZ, float& OutWalkBounds, EPhysicsBackend Backend)
{
	SetPhysicsBackend(Backend);
	OutFloorZ = EstimateFloorZ(Engine.GetLevel());
	OutWalkBounds = EstimateWalkBounds(Engine.GetLevel());
	RegisterBodiesFromLevel(Engine.GetLevel());
	GetWorld()->GetPhysicsScene().SyncFromLevel(Engine.GetLevel());

	UNavigationSystem& Nav = GetWorld()->GetNavigationSystem();
	Nav.SetCellSize(50.0f);
	Nav.SetAgentRadius(45.0f);
	Nav.BuildFromLevel(Engine.GetLevel(), GetWorld()->GetPhysicsScene(), OutFloorZ, OutWalkBounds);
	UE_LOG(LogPath, Log, "GameMode: NavMesh bake blockers=%d walkable=%d/%d cell=%g", Nav.GetBlockerCount(),
		Nav.GetWalkableCellCount(), Nav.GetNavMesh().Width * Nav.GetNavMesh().Depth,
		static_cast<double>(Nav.GetCellSize()));
}

void AGameModeBase::RebuildNavigation(UGameEngine& Engine, float FloorZ, float WalkBounds)
{
	RegisterBodiesFromLevel(Engine.GetLevel());
	GetWorld()->GetPhysicsScene().SyncFromLevel(Engine.GetLevel());
	UNavigationSystem& Nav = GetWorld()->GetNavigationSystem();
	Nav.BuildFromLevel(Engine.GetLevel(), GetWorld()->GetPhysicsScene(), FloorZ, WalkBounds);
}

void AGameModeBase::SnapCharacterToFloor(ACharacter& Character, FVector& InOutFeet, float FloorZ) const
{
	const FPhysScene& Phys = GetWorld()->GetPhysicsScene();
	const UCharacterMovementComponent& Move = Character.GetCharacterMovement();
	FVector Probe = InOutFeet;
	Probe.Z = FMath::Max(InOutFeet.Z, FloorZ);
	const float Support = Phys.QuerySupportZ(
		Character.GetCapsule(), Probe, Move.FloorZ, Move.MaxStepHeight, Move.Skin, Character.GetLevelMeshIndex());
	/** cm above the support */
	constexpr float SnapClearance = 2.0f;
	InOutFeet.Z = FMath::Max(Support, FloorZ) + SnapClearance;
}
