#pragma once

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Templates/UniquePtr.h"

class UWorld;

/**
 * Yaw in degrees that makes content converted from the legacy formats face an actor's forward. Legacy content faces the
 * legacy +Z, which becomes +Y; UE actors face +X. A component showing such content gets this relative yaw (as UE's
 * mannequin mesh does), and a level mesh driven by an actor gets the actor yaw plus this.
 */
inline constexpr float LegacyContentYaw = -90.0f;

/**
 * Unreal-style Actor (no A-prefix): owns a root USceneComponent and optional Level mesh link.
 *
 * ## Transforms
 * `Location` / `Rotation` (UE: yaw about Z, 0 = +X, 90 = +Y) are the gameplay pose written to Level meshes via
 * `SyncTransformToLevel`. The root `USceneComponent` may add `Relative*` offsets on top
 * (`GetComponentTransform`). Prefer setting Actor location/rotation for pawn movement; keep root
 * Relative near identity unless you intentionally offset the visual.
 *
 * ## Components
 * Root is always registered. Add member comps with `RegisterComponent`; heap extras with
 * `CreateDefaultSubobject<T>()`. See `UActorComponent` contract.
 *
 * ## Actor location vs Level mesh
 * `SetLevelMeshIndex` links this Actor to a Level UStaticMeshComponent for FPhysScene bodies.
 * `SyncTransformToLevel` writes Actor location/yaw into that mesh each gameplay frame
 * (`World::TickGameplayFrame`). Skeletal visuals use SceneComponents (`GetMesh`), not Level
 * meshes.
 */
class ENGINE_API AActor
{
public:
	virtual ~AActor();

	AActor(const AActor&) = delete;
	AActor& operator=(const AActor&) = delete;
	AActor(AActor&&) = delete;
	AActor& operator=(AActor&&) = delete;

	[[nodiscard]] UWorld* GetWorld() const
	{
		return World;
	}

	[[nodiscard]] USceneComponent& GetRootComponent()
	{
		return RootComponent;
	}
	[[nodiscard]] const USceneComponent& GetRootComponent() const
	{
		return RootComponent;
	}

	[[nodiscard]] const TArray<UActorComponent*>& GetComponents() const
	{
		return Components;
	}

	/** Register a component that lives on this Actor (member or already owned). Idempotent. */
	void RegisterComponent(UActorComponent* Component);

	/** Heap-owned component (Unreal CreateDefaultSubobject lite — no name table). */
	template <typename T, typename... ArgsType>
	T* CreateDefaultSubobject(ArgsType&&... Args)
	{
		static_assert(TIsDerivedFrom<T, UActorComponent>::Value, "T must derive from ActorComponent");
		TUniquePtr<T> Owned = MakeUnique<T>(Forward<ArgsType>(Args)...);
		T* Raw = Owned.Get();
		OwnedComponents.Add(MoveTemp(Owned));
		RegisterComponent(Raw);
		return Raw;
	}

	void SetLevelMeshIndex(SIZE_T Index)
	{
		LevelMeshIndex = Index;
	}
	[[nodiscard]] SIZE_T GetLevelMeshIndex() const
	{
		return LevelMeshIndex;
	}

	/** Spawn-order serial assigned by UWorld::SpawnActor (UE: UObjectBase::GetUniqueID). */
	void SetUniqueID(uint64 Id)
	{
		UniqueID = Id;
	}
	[[nodiscard]] uint64 GetUniqueID() const
	{
		return UniqueID;
	}

	[[nodiscard]] const FVector& GetActorLocation() const
	{
		return Location;
	}
	/** UE rotation: yaw about Z in degrees, 0 faces +X, 90 faces +Y. */
	[[nodiscard]] const FRotator& GetActorRotation() const
	{
		return Rotation;
	}

	void SetActorLocation(const FVector& InLocation)
	{
		Location = InLocation;
		// Keep root Relative* as identity offset so GetComponentTransform matches Actor pose.
		RootComponent.RelativeLocation = FVector::ZeroVector;
	}
	void SetActorRotation(const FRotator& InRotation)
	{
		Rotation = InRotation;
		RootComponent.RelativeRotation = FRotator::ZeroRotator;
	}
	void SetActorLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
	{
		SetActorLocation(NewLocation);
		SetActorRotation(NewRotation);
	}

	/** Unreal-like AActor::IsPendingKill. */
	[[nodiscard]] bool IsPendingKill() const
	{
		return bPendingKill;
	}
	[[nodiscard]] bool IsPendingKillPending() const
	{
		return IsPendingKill();
	}

	/** Mark for removal at end of World::Tick (or immediately via World::Clear). */
	virtual void Destroy()
	{
		if (!bPendingKill)
		{
			bPendingKill = true;
		}
	}

	virtual void BeginPlay()
	{
	}
	virtual void EndPlay()
	{
	}
	virtual void Tick(float /*deltaTime*/)
	{
	}

	void BeginPlayComponents();
	void EndPlayComponents();
	void TickComponents(float DeltaTime);
	[[nodiscard]] bool HasActorBegunPlay() const
	{
		return bHasBegunPlay;
	}

	/**
	 * Copy location + yaw into the linked Level UStaticMeshComponent (no-op if index is invalid). The mesh shows
	 * converted legacy content, so its yaw is the actor yaw plus LegacyContentYaw; its pitch and roll are kept.
	 */
	virtual void SyncTransformToLevel(ULevel& Level) const
	{
		TArray<UStaticMeshComponent>& Meshes = Level.GetStaticMeshes();
		if (LevelMeshIndex >= static_cast<SIZE_T>(Meshes.Num()))
		{
			return;
		}
		UStaticMeshComponent& Obj = Meshes[static_cast<int32>(LevelMeshIndex)];
		Obj.Transform.SetLocation(Location);
		FRotator MeshRotation = Obj.Transform.Rotator();
		MeshRotation.Yaw = Rotation.Yaw + LegacyContentYaw;
		Obj.Transform.SetRotation(MeshRotation.Quaternion());
	}

protected:
	AActor()
	{
		RegisterComponent(&RootComponent);
	}

	[[nodiscard]] FVector& MutableLocation()
	{
		return Location;
	}
	[[nodiscard]] FRotator& MutableRotation()
	{
		return Rotation;
	}

	void UnregisterComponent(UActorComponent* Component);

private:
	friend class UWorld;
	friend class UActorComponent;

	USceneComponent RootComponent{};
	TArray<UActorComponent*> Components;
	TArray<TUniquePtr<UActorComponent>> OwnedComponents;
	SIZE_T LevelMeshIndex = ULevel::Npos;
	uint64 UniqueID = 0;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	UWorld* World = nullptr;
	bool bPendingKill = false;
	bool bHasBegunPlay = false;
};
