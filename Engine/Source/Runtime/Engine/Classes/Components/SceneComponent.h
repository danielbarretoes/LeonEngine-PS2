#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

class AActor;

/**
 * Unreal-like USceneComponent: UActorComponent + relative TRS + parent/child attach tree.
 * World transform: the relative transform, then the parent's (a component without a parent uses its owning Actor's
 * location and rotation). Relative* are UE values: world units (cm), an FRotator and a Scale3D.
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
	FRotator RelativeRotation = FRotator::ZeroRotator;
	FVector RelativeScale3D = FVector::OneVector;

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

	[[nodiscard]] FTransform GetRelativeTransform() const;
	/** Component-to-world transform (UE: GetComponentTransform). */
	[[nodiscard]] FTransform GetComponentTransform() const;
	[[nodiscard]] FVector GetComponentLocation() const;
	[[nodiscard]] FRotator GetComponentRotation() const;

	/** Detaches the attach tree, then unregisters from the owner. */
	void DestroyComponent() override;

private:
	void DetachChild(USceneComponent* Child);
	[[nodiscard]] bool WouldCreateCycle(const USceneComponent* CandidateParent) const;

	USceneComponent* Parent = nullptr;
	TArray<USceneComponent*> Children;
};
