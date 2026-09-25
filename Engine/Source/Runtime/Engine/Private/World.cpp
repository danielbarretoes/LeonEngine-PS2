#include "Engine/World.h"

#include "BodyInstance.h"
#include "Engine/Level.h"
#include "GameFramework/Character.h"
#include "SceneRenderer.h"

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
	for (int32 I = 0; I < Meshes.Num(); ++I)
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
	TArray<ACharacter*> Characters;
	Characters.Reserve(Actors.Num());
	ForEach<ACharacter>([&](ACharacter& Character) { Characters.Add(&Character); });
	if (Characters.Num() < 2)
	{
		return;
	}

	// Flow: collect live Characters → iterate pairs → equal XY depenetration (2–3 passes).
	constexpr int Iterations = 3;
	for (int Iter = 0; Iter < Iterations; ++Iter)
	{
		for (int32 I = 0; I < Characters.Num(); ++I)
		{
			for (int32 J = I + 1; J < Characters.Num(); ++J)
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
		Step.FloorZ = Params.PhysicsFloorZ;
		Step.Skin = Params.PhysicsSkin;
	}
	else if (ACharacter* Primary = FindFirst<ACharacter>())
	{
		const UCharacterMovementComponent& MoveCfg = Primary->GetCharacterMovement();
		Step.Damping = MoveCfg.PushDamping;
		Step.WalkBounds = MoveCfg.WalkBounds;
		Step.Gravity = MoveCfg.Gravity;
		Step.FloorZ = MoveCfg.FloorZ;
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
	PendingSpawns.Empty();
	for (auto& Actor : Actors)
	{
		if (Actor)
		{
			Actor->EndPlay();
			Actor->EndPlayComponents();
			Actor->World = nullptr;
		}
	}
	Actors.Empty();
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
		AActor* Raw = Owned.Get();
		Actors.Add(MoveTemp(Owned));
		Raw->BeginPlayComponents();
		Raw->BeginPlay();
	}
	PendingSpawns.Empty();
}

void UWorld::PurgePending()
{
	for (int32 Index = 0; Index < Actors.Num();)
	{
		TUniquePtr<AActor>& Actor = Actors[Index];
		if (!Actor || Actor->IsPendingKillPending())
		{
			if (Actor)
			{
				Actor->EndPlay();
				Actor->EndPlayComponents();
				Actor->World = nullptr;
			}
			Actors.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}
}
