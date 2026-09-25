#pragma once

#include "AI/Navigation/NavigationSystem.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Physics/PhysScene.h"

class ACharacter;
class FDebugDraw;
class ULevel;
class FSceneRenderer;

struct ENGINE_API FWorldGameplayFrameParams
{
	float DeltaTime = 0.0f;
	ULevel* Level = nullptr;
	FSceneRenderer* Renderer = nullptr;
	FDebugDraw* CollisionDebugDraw = nullptr;
	FDebugDraw* NavMeshDebugDraw = nullptr;
	/** When true, FPhysScene::Step uses these values instead of the first Character's movement. */
	bool bOverridePhysicsStep = false;
	float PhysicsDamping = 6.0f;
	/** cm */
	float PhysicsWalkBounds = 1800.0f;
	/** cm/s^2 */
	float PhysicsGravity = 2400.0f;
	/** cm */
	float PhysicsFloorZ = 0.0f;
	/** cm */
	float PhysicsSkin = 2.0f;
};

/**
 * Owns spawned Actors + FPhysScene; ticks them and purges pending kills.
 * Distinct from Level (map/visual content ≈ ULevel).
 */
class ENGINE_API UWorld
{
public:
	explicit UWorld(EPhysicsBackend PhysicsBackend = DefaultPhysicsBackend())
		: Physics(PhysicsBackend)
	{
	}
	~UWorld()
	{
		Clear();
	}

	UWorld(const UWorld&) = delete;
	UWorld& operator=(const UWorld&) = delete;
	UWorld(UWorld&&) = delete;
	UWorld& operator=(UWorld&&) = delete;

	[[nodiscard]] FPhysScene& GetPhysicsScene()
	{
		return Physics;
	}
	[[nodiscard]] const FPhysScene& GetPhysicsScene() const
	{
		return Physics;
	}

	/** Unreal-like UNavigationSystem lite (grid NavMesh for AI pathfinding). */
	[[nodiscard]] UNavigationSystem& GetNavigationSystem()
	{
		return Navigation;
	}
	[[nodiscard]] const UNavigationSystem& GetNavigationSystem() const
	{
		return Navigation;
	}

	/** Recreate FPhysScene with another backend (clears bodies). Call before RegisterBodiesFromLevel. */
	void SetPhysicsBackend(EPhysicsBackend PhysicsBackend)
	{
		Physics = FPhysScene(PhysicsBackend);
	}

	template <typename T, typename... ArgsType>
	T* SpawnActor(ArgsType&&... Args)
	{
		static_assert(TIsDerivedFrom<T, AActor>::Value, "T must derive from Actor");
		auto Owned = MakeUnique<T>(Forward<ArgsType>(Args)...);
		T* Raw = Owned.Get();
		Raw->World = this;
		Raw->SetUniqueID(++NextUniqueID);
		if (bTicking)
		{
			// Defer push_back so Tick iterators stay valid.
			PendingSpawns.Add(MoveTemp(Owned));
		}
		else
		{
			Actors.Add(MoveTemp(Owned));
			Raw->BeginPlayComponents();
			Raw->BeginPlay();
		}
		return Raw;
	}

	void DestroyActor(AActor* Actor)
	{
		if (Actor != nullptr && Actor->World == this)
		{
			Actor->Destroy();
		}
	}

	/** Actor Tick only (UAnimInstance, etc.). Prefer TickGameplayFrame for Character worlds. */
	void Tick(float InDeltaTime);

	/** Unreal-like frame: Character move → FPhysScene::Step → overlaps → Actor Tick → sync → draw. */
	void TickGameplayFrame(const FWorldGameplayFrameParams& Params);

	/** Register StaticMeshComponents that have collision as FPhysScene bodies (clears first). */
	void RegisterBodiesFromLevel(const ULevel& InLevel);

	void SubmitSkeletalDraws(FSceneRenderer& InRenderer) const;

	void Clear();

	[[nodiscard]] SIZE_T ActorCount() const
	{
		return Actors.Num();
	}

	template <typename T>
	[[nodiscard]] T* FindFirst() const
	{
		static_assert(TIsDerivedFrom<T, AActor>::Value, "T must derive from Actor");
		for (const auto& Actor : Actors)
		{
			if (Actor && !Actor->IsPendingKill())
			{
				if (T* Typed = dynamic_cast<T*>(Actor.Get()))
				{
					return Typed;
				}
			}
		}
		return nullptr;
	}

	/** Visit every live Actor of type T. */
	template <typename T, typename TFn>
	void ForEach(TFn&& Fn) const
	{
		static_assert(TIsDerivedFrom<T, AActor>::Value, "T must derive from Actor");
		for (const auto& Actor : Actors)
		{
			if (Actor && !Actor->IsPendingKill())
			{
				if (T* Typed = dynamic_cast<T*>(Actor.Get()))
				{
					Fn(*Typed);
				}
			}
		}
	}

	/** Visit every live Actor (any type). */
	template <typename TFn>
	void ForEachActor(TFn&& Fn) const
	{
		for (const auto& Actor : Actors)
		{
			if (Actor && !Actor->IsPendingKill())
			{
				Fn(*Actor);
			}
		}
	}

private:
	void FlushPendingSpawns();
	void PurgePending();
	/** Pairwise Character capsule depenetration (players / AI are not FPhysScene bodies). */
	void ResolveCharacterOverlaps();

	FPhysScene Physics{};
	UNavigationSystem Navigation{};
	TArray<TUniquePtr<AActor>> Actors;
	TArray<TUniquePtr<AActor>> PendingSpawns;
	bool bTicking = false;
	uint64 NextUniqueID = 0;
};
