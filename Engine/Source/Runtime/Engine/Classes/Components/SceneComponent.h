#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Level/LegacyTransform.h"

class AActor;

/**
 * Unreal-like USceneComponent: UActorComponent + relative TRS + parent/child attach tree.
 * World transform: root uses owning Actor location/yaw + relative; children compose parent * relative
 * (GL-convention matrices, LegacyGLMath.h).
 */
class ENGINE_API USceneComponent : public UActorComponent
{
public:
	USceneComponent() = default;
	~USceneComponent() override;

	USceneComponent(const USceneComponent&) = delete;
	USceneComponent& operator=(const USceneComponent&) = delete;
	USceneComponent(USceneComponent&&) = delete;
	USceneComponent& operator=(USceneComponent&&) = delete;

	FVector RelativeLocation = FVector::ZeroVector;
	FVector RelativeRotation = FVector::ZeroVector; // XYZ Euler, degrees
	FVector RelativeScale = FVector::OneVector;

	/** Attaches under InParent. Returns false if the parent is null, this, or would create a cycle. */
	[[nodiscard]] bool AttachToComponent(USceneComponent* InParent, bool bKeepWorldTransform = false);
	void DetachFromParent(bool bKeepWorldTransform = false);

	[[nodiscard]] USceneComponent* GetAttachParent() const
	{
		return Parent;
	}
	[[nodiscard]] const TArray<USceneComponent*>& GetAttachChildren() const
	{
		return Children;
	}

	[[nodiscard]] FLegacyTransform GetRelativeTransform() const;
	/** Component-to-world matrix (Unreal GetComponentTransform; GL convention). */
	[[nodiscard]] FMatrix GetComponentTransform() const;
	[[nodiscard]] FVector GetComponentLocation() const;

	/** Detaches the attach tree, then unregisters from the owner. */
	void DestroyComponent() override;

private:
	void DetachChild(USceneComponent* Child);
	[[nodiscard]] bool WouldCreateCycle(const USceneComponent* CandidateParent) const;

	USceneComponent* Parent = nullptr;
	TArray<USceneComponent*> Children;
};
