#pragma once

#include "AI/Navigation/NavigationSystem.h"
#include "GameFramework/Actor.h"
#include "Physics/PhysScene.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

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
	/// When true, FPhysScene::Step uses these values instead of the first Character's movement.
	bool bOverridePhysicsStep = false;
	float PhysicsDamping = 6.0f;
	float PhysicsWalkBounds = 18.0f;
	float PhysicsGravity = 24.0f;
	float PhysicsFloorY = 0.0f;
	float PhysicsSkin = 0.02f;
};

/// Owns spawned Actors + FPhysScene; ticks them and purges pending kills.
/// Distinct from `Level` (map/visual content ≈ ULevel).
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

	/// Unreal-like UNavigationSystem lite (grid NavMesh for AI pathfinding).
	[[nodiscard]] UNavigationSystem& GetNavigationSystem()
	{
		return Navigation;
	}
	[[nodiscard]] const UNavigationSystem& GetNavigationSystem() const
	{
		return Navigation;
	}

	/// Recreate FPhysScene with another backend (clears bodies). Call before RegisterBodiesFromLevel.
	void SetPhysicsBackend(EPhysicsBackend PhysicsBackend)
	{
		Physics = FPhysScene(PhysicsBackend);
	}

	template <typename T, typename... ArgsType>
	T* SpawnActor(ArgsType&&... Args)
	{
		static_assert(std::is_base_of_v<AActor, T>, "T must derive from Actor");
		auto Owned = std::make_unique<T>(std::forward<ArgsType>(Args)...);
		T* Raw = Owned.get();
		Raw->World = this;
		Raw->SetEditorId(++NextEditorId);
		if (bTicking)
		{
			// Defer push_back so Tick iterators stay valid.
			PendingSpawns.push_back(std::move(Owned));
		}
		else
		{
			Actors.push_back(std::move(Owned));
			Raw->BeginPlayComponents();
			Raw->BeginPlay();
		}
		return Raw;
	}

	/// Find live Actor by session-stable editor id (PIE / Outliner).
	[[nodiscard]] AActor* FindActorByEditorId(std::uint64_t EditorId) const
	{
		if (EditorId == 0)
		{
			return nullptr;
		}
		for (const auto& Actor : Actors)
		{
			if (Actor && !Actor->IsPendingKill() && Actor->GetEditorId() == EditorId)
			{
				return Actor.get();
			}
		}
		return nullptr;
	}

	void DestroyActor(AActor* Actor)
	{
		if (Actor != nullptr && Actor->World == this)
		{
			Actor->Destroy();
		}
	}

	/// Actor Tick only (UAnimInstance, etc.). Prefer `TickGameplayFrame` for Character worlds.
	void Tick(float InDeltaTime);

	/// Unreal-like frame: Character move → FPhysScene::Step → overlaps → Actor Tick → sync → draw.
	void TickGameplayFrame(const FWorldGameplayFrameParams& Params);

	/// Register StaticMeshComponents that have collision as FPhysScene bodies (clears first).
	void RegisterBodiesFromLevel(const ULevel& InLevel);

	void SubmitSkeletalDraws(FSceneRenderer& InRenderer) const;

	void Clear();

	[[nodiscard]] std::size_t ActorCount() const
	{
		return Actors.size();
	}

	template <typename T>
	[[nodiscard]] T* FindFirst() const
	{
		static_assert(std::is_base_of_v<AActor, T>, "T must derive from Actor");
		for (const auto& Actor : Actors)
		{
			if (Actor && !Actor->IsPendingKill())
			{
				if (T* Typed = dynamic_cast<T*>(Actor.get()))
				{
					return Typed;
				}
			}
		}
		return nullptr;
	}

	/// Visit every live Actor of type T.
	template <typename T, typename TFn>
	void ForEach(TFn&& Fn) const
	{
		static_assert(std::is_base_of_v<AActor, T>, "T must derive from Actor");
		for (const auto& Actor : Actors)
		{
			if (Actor && !Actor->IsPendingKill())
			{
				if (T* Typed = dynamic_cast<T*>(Actor.get()))
				{
					Fn(*Typed);
				}
			}
		}
	}

	/// Visit every live Actor (any type).
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
	/// Pairwise Character capsule depenetration (players / AI are not FPhysScene bodies).
	void ResolveCharacterOverlaps();

	FPhysScene Physics{};
	UNavigationSystem Navigation{};
	std::vector<std::unique_ptr<AActor>> Actors;
	std::vector<std::unique_ptr<AActor>> PendingSpawns;
	bool bTicking = false;
	std::uint64_t NextEditorId = 0;
};
