#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "SceneComponent.generated.h"

/**
 * A component with a transform and an attachment (UE: USceneComponent).
 *
 * - Relative*: the transform relative to the attach parent (or to the world without one), in UE units (cm, an
 *   FRotator, a Scale3D). They are public members, as before UE 4.24; the 4.27 accessors exist as well.
 * - The component-to-world transform is the relative transform, then the parent's socket transform
 *   (GetSocketTransform of AttachSocketName; the parent's component transform for NAME_None). Leon computes it on
 *   demand in GetComponentTransform instead of caching ComponentToWorld.
 * - An actor's root component has no parent: its relative transform is the actor transform.
 * - AttachToComponent links a child now; SetupAttachment (constructors) only records the parent, and the link is made
 *   when the component registers (UE).
 */
UCLASS()
class ENGINE_API USceneComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USceneComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Location relative to the parent, cm (UE: RelativeLocation). */
	UPROPERTY()
	FVector RelativeLocation = FVector::ZeroVector;

	/** Rotation relative to the parent (UE: RelativeRotation). */
	UPROPERTY()
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/** Scale relative to the parent (UE: RelativeScale3D). */
	UPROPERTY()
	FVector RelativeScale3D = FVector::OneVector;

	/** Drawn when true (UE: bVisible). */
	UPROPERTY()
	uint8 bVisible : 1;

	/** Hidden in game even when visible (UE: bHiddenInGame). */
	UPROPERTY()
	uint8 bHiddenInGame : 1;

	/** Whether the component may move at runtime (UE: Mobility); the level reader sets it from the `.llev` record. */
	EComponentMobility Mobility = EComponentMobility::Static;

	[[nodiscard]] FVector GetRelativeLocation() const
	{
		return RelativeLocation;
	}
	[[nodiscard]] FRotator GetRelativeRotation() const
	{
		return RelativeRotation;
	}
	[[nodiscard]] FVector GetRelativeScale3D() const
	{
		return RelativeScale3D;
	}
	void SetRelativeLocation(const FVector& NewLocation)
	{
		RelativeLocation = NewLocation;
	}
	void SetRelativeRotation(const FRotator& NewRotation)
	{
		RelativeRotation = NewRotation;
	}
	void SetRelativeScale3D(const FVector& NewScale3D)
	{
		RelativeScale3D = NewScale3D;
	}
	void SetRelativeLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
	{
		RelativeLocation = NewLocation;
		RelativeRotation = NewRotation;
	}
	/**
	 * The relative transform; its rotation is exactly the quaternion SetRelativeTransform last stored while
	 * RelativeRotation still holds the rotator that came with it (RelativeRotationCache).
	 */
	[[nodiscard]] FTransform GetRelativeTransform() const;
	void SetRelativeTransform(const FTransform& NewTransform);

	/** UE: SetMobility / Mobility. */
	void SetMobility(EComponentMobility NewMobility)
	{
		Mobility = NewMobility;
	}

	/**
	 * Records the parent to attach to when the component registers (UE: SetupAttachment). For constructors, before the
	 * component is registered.
	 */
	void SetupAttachment(USceneComponent* InParent, FName InSocketName = NAME_None);

	/**
	 * Attaches under InParent at InSocketName (UE: AttachToComponent). Returns false when the parent is null, this
	 * component, or one of its descendants (a cycle).
	 */
	bool AttachToComponent(
		USceneComponent* InParent, const FAttachmentTransformRules& AttachmentRules, FName InSocketName = NAME_None);
	/** Detaches from the parent (UE: DetachFromComponent). */
	void DetachFromComponent(const FDetachmentTransformRules& DetachmentRules);

	[[nodiscard]] USceneComponent* GetAttachParent() const
	{
		return AttachParent;
	}
	[[nodiscard]] FName GetAttachSocketName() const
	{
		return AttachSocketName;
	}
	[[nodiscard]] const TArray<USceneComponent*>& GetAttachChildren() const
	{
		return AttachChildren;
	}
	/** True when this component is InParent or attached below it (UE: IsAttachedTo). */
	[[nodiscard]] bool IsAttachedTo(const USceneComponent* InParent) const;

	/** The world transform of a socket of this component (UE: GetSocketTransform); the component's for NAME_None. */
	[[nodiscard]] virtual FTransform GetSocketTransform(FName InSocketName) const;
	/** True when the component has the socket (UE: DoesSocketExist); scene components have none. */
	[[nodiscard]] virtual bool DoesSocketExist(FName InSocketName) const;

	/** Component-to-world transform (UE: GetComponentTransform / GetComponentToWorld). */
	[[nodiscard]] FTransform GetComponentTransform() const;
	[[nodiscard]] FTransform GetComponentToWorld() const
	{
		return GetComponentTransform();
	}
	/** World location; the relative location itself without a parent. */
	[[nodiscard]] FVector GetComponentLocation() const;
	/** World rotation; the relative rotation itself without a parent (no quaternion round trip). */
	[[nodiscard]] FRotator GetComponentRotation() const;
	[[nodiscard]] FQuat GetComponentQuat() const;
	[[nodiscard]] FVector GetComponentScale() const;
	[[nodiscard]] FVector GetForwardVector() const;
	[[nodiscard]] FVector GetRightVector() const;
	[[nodiscard]] FVector GetUpVector() const;

	/** Sets the world location / rotation through the parent's transform (UE: SetWorldLocation / SetWorldRotation). */
	void SetWorldLocation(const FVector& NewLocation);
	void SetWorldRotation(const FRotator& NewRotation);
	void SetWorldLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation);
	void SetWorldTransform(const FTransform& NewTransform);

	/** UE: SetVisibility / IsVisible (visible and not hidden in game). */
	void SetVisibility(bool bNewVisibility);
	void SetHiddenInGame(bool bNewHidden);
	[[nodiscard]] bool IsVisible() const;

	/** Detaches the children and the component itself, then UActorComponent::DestroyComponent. */
	void DestroyComponent(bool bPromoteChildren = false) override;

	// UObject
	void BeginDestroy() override;

protected:
	void OnRegister() override;

private:
	/** The parent's world transform at this component's socket (the identity without a parent). */
	[[nodiscard]] FTransform GetParentToWorld() const;
	void DetachAllChildren();

	/** The component this one is attached to (UE: AttachParent). */
	UPROPERTY()
	USceneComponent* AttachParent = nullptr;

	/** The socket of AttachParent (UE: AttachSocketName). */
	UPROPERTY()
	FName AttachSocketName;

	/** The components attached to this one (UE: AttachChildren). */
	UPROPERTY(Transient)
	TArray<USceneComponent*> AttachChildren;

	/** Keeps a quaternion set through SetRelativeTransform exact (UE: RelativeRotationCache). */
	FRotationConversionCache RelativeRotationCache;
};
