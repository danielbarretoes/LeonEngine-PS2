#include "Engine/World.h"

#include "BodyInstance.h"
#include "Engine/Level.h"
#include "GameFramework/Character.h"
#include "SceneRenderer.h"

#include <vector>

void UWorld::Tick(float InDeltaTime)
{
	bTicking = true;
	for (auto& Actor : Actors)
	{
		if (Actor && !Actor->IsPendingKillPending())
		{
			Actor->TickComponents(InDeltaTime);
			Actor->Tick(InDeltaTime);
		}
	}
	bTicking = false;
	FlushPendingSpawns();
	PurgePending();
}

void UWorld::RegisterBodiesFromLevel(const ULevel& InLevel)
{
	Physics.Clear();
	const auto& Meshes = InLevel.GetStaticMeshes();
	for (std::size_t I = 0; I < Meshes.size(); ++I)
	{
		const UStaticMeshComponent& Component = Meshes[I];
		if (!Component.HasPhysicsBody())
		{
			continue;
		}
		FBodyInstanceDesc Desc{};
		Desc.LevelMeshIndex = I;
		Desc.Type = Component.bSimulatePhysics ? EBodyType::Dynamic : EBodyType::Static;
		Desc.bEnableGravity = Component.bEnableGravity;
		Physics.AddBody(Desc);
	}
}

void UWorld::ResolveCharacterOverlaps()
{
	std::vector<ACharacter*> Characters;
	Characters.reserve(Actors.size());
	ForEach<ACharacter>([&](ACharacter& Character) { Characters.push_back(&Character); });
	if (Characters.size() < 2)
	{
		return;
	}

	// Flow: collect live Characters → iterate pairs → equal XZ depenetration (2–3 passes).
	constexpr int Iterations = 3;
	for (int Iter = 0; Iter < Iterations; ++Iter)
	{
		for (std::size_t I = 0; I < Characters.size(); ++I)
		{
			for (std::size_t J = I + 1; J < Characters.size(); ++J)
			{
				Characters[I]->ResolvePawnOverlap(*Characters[J]);
			}
		}
	}
}

void UWorld::TickGameplayFrame(const FWorldGameplayFrameParams& Params)
{
	ForEach<ACharacter>(
		[&](ACharacter& Character) { Character.TickCharacterMovement(Params.DeltaTime, Params.CollisionDebugDraw); });
	ResolveCharacterOverlaps();

	FPhysSceneStepParams Step{};
	Step.DeltaTime = Params.DeltaTime;
	if (Params.bOverridePhysicsStep)
	{
		Step.Damping = Params.PhysicsDamping;
		Step.WalkBounds = Params.PhysicsWalkBounds;
		Step.Gravity = Params.PhysicsGravity;
		Step.FloorY = Params.PhysicsFloorY;
		Step.Skin = Params.PhysicsSkin;
	}
	else if (ACharacter* Primary = FindFirst<ACharacter>())
	{
		const UCharacterMovementComponent& MoveCfg = Primary->GetCharacterMovement();
		Step.Damping = MoveCfg.PushDamping;
		Step.WalkBounds = MoveCfg.WalkBounds;
		Step.Gravity = MoveCfg.Gravity;
		Step.FloorY = MoveCfg.FloorY;
		Step.Skin = MoveCfg.Skin;
		Step.SkipLevelMeshIndex = Primary->GetLevelMeshIndex();
	}
	Physics.Step(Step);

	ForEach<ACharacter>([](ACharacter& Character) { Character.ResolveOverlaps(); });
	ResolveCharacterOverlaps();

	Tick(Params.DeltaTime);

	if (Params.Level != nullptr)
	{
		Physics.SyncToLevel(*Params.Level);
		ForEach<ACharacter>(
			[LocalLevel = Params.Level](ACharacter& Character) { Character.SyncTransformToLevel(*LocalLevel); });
	}

	if (Params.Renderer != nullptr)
	{
		SubmitSkeletalDraws(*Params.Renderer);
	}

	if (Params.CollisionDebugDraw != nullptr)
	{
		ForEach<ACharacter>(
			[&](ACharacter& Character)
			{
				Physics.AppendCollisionDebug(*Params.CollisionDebugDraw, Character.GetCapsule(),
					Character.GetActorLocation(), Character.GetLevelMeshIndex());
			});
	}

	if (Params.NavMeshDebugDraw != nullptr)
	{
		Navigation.AppendDebugDraw(*Params.NavMeshDebugDraw);
	}
}

void UWorld::SubmitSkeletalDraws(FSceneRenderer& InRenderer) const
{
	ForEach<ACharacter>([&](ACharacter& Character) { Character.SubmitMeshDraw(InRenderer); });
}

void UWorld::Clear()
{
	for (auto& Actor : PendingSpawns)
	{
		if (Actor)
		{
			Actor->World = nullptr;
		}
	}
	PendingSpawns.clear();
	for (auto& Actor : Actors)
	{
		if (Actor)
		{
			Actor->EndPlay();
			Actor->EndPlayComponents();
			Actor->World = nullptr;
		}
	}
	Actors.clear();
	Physics.Clear();
}

void UWorld::FlushPendingSpawns()
{
	for (auto& Owned : PendingSpawns)
	{
		if (!Owned)
		{
			continue;
		}
		AActor* Raw = Owned.get();
		Actors.push_back(std::move(Owned));
		Raw->BeginPlayComponents();
		Raw->BeginPlay();
	}
	PendingSpawns.clear();
}

void UWorld::PurgePending()
{
	for (auto It = Actors.begin(); It != Actors.end();)
	{
		if (!*It || (*It)->IsPendingKillPending())
		{
			if (*It)
			{
				(*It)->EndPlay();
				(*It)->EndPlayComponents();
				(*It)->World = nullptr;
			}
			It = Actors.erase(It);
		}
		else
		{
			++It;
		}
	}
}
