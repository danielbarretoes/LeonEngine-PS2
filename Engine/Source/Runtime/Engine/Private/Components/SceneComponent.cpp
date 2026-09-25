#include "Components/SceneComponent.h"

#include "GameFramework/Actor.h"

const FAttachmentTransformRules FAttachmentTransformRules::KeepRelativeTransform(EAttachmentRule::KeepRelative, false);
const FAttachmentTransformRules FAttachmentTransformRules::KeepWorldTransform(EAttachmentRule::KeepWorld, false);
const FAttachmentTransformRules FAttachmentTransformRules::SnapToTargetNotIncludingScale(
	EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
const FAttachmentTransformRules FAttachmentTransformRules::SnapToTargetIncludingScale(
	EAttachmentRule::SnapToTarget, false);

const FDetachmentTransformRules FDetachmentTransformRules::KeepRelativeTransform(EDetachmentRule::KeepRelative, true);
const FDetachmentTransformRules FDetachmentTransformRules::KeepWorldTransform(EDetachmentRule::KeepWorld, true);

namespace
{

	/** The relative field a rule keeps: the current one, the one that keeps the world value, or the snapped one. */
	FVector PickVector(EAttachmentRule Rule, const FVector& Current, const FVector& KeepWorld, const FVector& Snap)
	{
		switch (Rule)
		{
			case EAttachmentRule::KeepWorld:
				return KeepWorld;
			case EAttachmentRule::SnapToTarget:
				return Snap;
			default:
				return Current;
		}
	}

	FRotator PickRotator(EAttachmentRule Rule, const FRotator& Current, const FRotator& KeepWorld, const FRotator& Snap)
	{
		switch (Rule)
		{
			case EAttachmentRule::KeepWorld:
				return KeepWorld;
			case EAttachmentRule::SnapToTarget:
				return Snap;
			default:
				return Current;
		}
	}

} // namespace

USceneComponent::USceneComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bVisible = true;
	bHiddenInGame = false;
}

FTransform USceneComponent::GetRelativeTransform() const
{
	return FTransform(RelativeRotation, RelativeLocation, RelativeScale3D);
}

void USceneComponent::SetRelativeTransform(const FTransform& NewTransform)
{
	RelativeLocation = NewTransform.GetLocation();
	RelativeRotation = NewTransform.Rotator();
	RelativeScale3D = NewTransform.GetScale3D();
}

bool USceneComponent::IsAttachedTo(const USceneComponent* InParent) const
{
	for (const USceneComponent* Walk = this; Walk != nullptr; Walk = Walk->AttachParent)
	{
		if (Walk == InParent)
		{
			return true;
		}
	}
	return false;
}

void USceneComponent::SetupAttachment(USceneComponent* InParent, FName InSocketName)
{
	if (InParent == this || (InParent != nullptr && InParent->IsAttachedTo(this)))
	{
		return;
	}
	AttachParent = InParent;
	AttachSocketName = InSocketName;
}

bool USceneComponent::AttachToComponent(
	USceneComponent* InParent, const FAttachmentTransformRules& AttachmentRules, FName InSocketName)
{
	if (InParent == nullptr || InParent == this || InParent->IsAttachedTo(this))
	{
		return false;
	}

	const bool bKeepsEverything = AttachmentRules.LocationRule == EAttachmentRule::KeepRelative &&
		AttachmentRules.RotationRule == EAttachmentRule::KeepRelative &&
		AttachmentRules.ScaleRule == EAttachmentRule::KeepRelative;
	const FTransform WorldBefore = bKeepsEverything ? FTransform::Identity : GetComponentTransform();
	if (AttachParent != nullptr)
	{
		AttachParent->AttachChildren.Remove(this);
	}
	AttachParent = InParent;
	AttachSocketName = InSocketName;
	InParent->AttachChildren.AddUnique(this);

	if (!bKeepsEverything)
	{
		const FTransform KeepWorld = WorldBefore.GetRelativeTransform(GetParentToWorld());
		RelativeLocation =
			PickVector(AttachmentRules.LocationRule, RelativeLocation, KeepWorld.GetLocation(), FVector::ZeroVector);
		RelativeRotation =
			PickRotator(AttachmentRules.RotationRule, RelativeRotation, KeepWorld.Rotator(), FRotator::ZeroRotator);
		RelativeScale3D =
			PickVector(AttachmentRules.ScaleRule, RelativeScale3D, KeepWorld.GetScale3D(), FVector::OneVector);
	}
	return true;
}

void USceneComponent::DetachFromComponent(const FDetachmentTransformRules& DetachmentRules)
{
	if (AttachParent == nullptr)
	{
		return;
	}

	const bool bKeepsWorld = DetachmentRules.LocationRule == EDetachmentRule::KeepWorld ||
		DetachmentRules.RotationRule == EDetachmentRule::KeepWorld ||
		DetachmentRules.ScaleRule == EDetachmentRule::KeepWorld;
	const FTransform WorldBefore = bKeepsWorld ? GetComponentTransform() : FTransform::Identity;
	AttachParent->AttachChildren.Remove(this);
	AttachParent = nullptr;
	AttachSocketName = NAME_None;

	// Without a parent the relative transform is the world transform.
	if (DetachmentRules.LocationRule == EDetachmentRule::KeepWorld)
	{
		RelativeLocation = WorldBefore.GetLocation();
	}
	if (DetachmentRules.RotationRule == EDetachmentRule::KeepWorld)
	{
		RelativeRotation = WorldBefore.Rotator();
	}
	if (DetachmentRules.ScaleRule == EDetachmentRule::KeepWorld)
	{
		RelativeScale3D = WorldBefore.GetScale3D();
	}
}

FTransform USceneComponent::GetSocketTransform(FName /*InSocketName*/) const
{
	return GetComponentTransform();
}

bool USceneComponent::DoesSocketExist(FName /*InSocketName*/) const
{
	return false;
}

FTransform USceneComponent::GetParentToWorld() const
{
	return AttachParent != nullptr ? AttachParent->GetSocketTransform(AttachSocketName) : FTransform::Identity;
}

FTransform USceneComponent::GetComponentTransform() const
{
	if (AttachParent != nullptr)
	{
		return GetRelativeTransform() * AttachParent->GetSocketTransform(AttachSocketName);
	}
	return GetRelativeTransform();
}

FVector USceneComponent::GetComponentLocation() const
{
	return AttachParent != nullptr ? GetComponentTransform().GetLocation() : RelativeLocation;
}

FRotator USceneComponent::GetComponentRotation() const
{
	return AttachParent != nullptr ? GetComponentTransform().Rotator() : RelativeRotation;
}

FQuat USceneComponent::GetComponentQuat() const
{
	return GetComponentTransform().GetRotation();
}

FVector USceneComponent::GetComponentScale() const
{
	return AttachParent != nullptr ? GetComponentTransform().GetScale3D() : RelativeScale3D;
}

FVector USceneComponent::GetForwardVector() const
{
	return GetComponentTransform().GetUnitAxis(EAxis::X);
}

FVector USceneComponent::GetRightVector() const
{
	return GetComponentTransform().GetUnitAxis(EAxis::Y);
}

FVector USceneComponent::GetUpVector() const
{
	return GetComponentTransform().GetUnitAxis(EAxis::Z);
}

void USceneComponent::SetWorldLocation(const FVector& NewLocation)
{
	RelativeLocation = AttachParent != nullptr ? GetParentToWorld().InverseTransformPosition(NewLocation) : NewLocation;
}

void USceneComponent::SetWorldRotation(const FRotator& NewRotation)
{
	if (AttachParent == nullptr)
	{
		RelativeRotation = NewRotation;
		return;
	}
	RelativeRotation = (GetParentToWorld().GetRotation().Inverse() * NewRotation.Quaternion()).Rotator();
}

void USceneComponent::SetWorldLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	SetWorldLocation(NewLocation);
	SetWorldRotation(NewRotation);
}

void USceneComponent::SetWorldTransform(const FTransform& NewTransform)
{
	SetRelativeTransform(
		AttachParent != nullptr ? NewTransform.GetRelativeTransform(GetParentToWorld()) : NewTransform);
}

void USceneComponent::SetVisibility(bool bNewVisibility)
{
	bVisible = bNewVisibility;
}

void USceneComponent::SetHiddenInGame(bool bNewHidden)
{
	bHiddenInGame = bNewHidden;
}

bool USceneComponent::IsVisible() const
{
	return bVisible && !bHiddenInGame;
}

void USceneComponent::OnRegister()
{
	// SetupAttachment only recorded the parent: link it now (UE).
	if (AttachParent != nullptr && !AttachParent->AttachChildren.Contains(this))
	{
		USceneComponent* Parent = AttachParent;
		AttachParent = nullptr;
		(void)AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, AttachSocketName);
	}
	Super::OnRegister();
}

void USceneComponent::DetachAllChildren()
{
	while (AttachChildren.Num() > 0)
	{
		USceneComponent* Child = AttachChildren.Last();
		if (Child == nullptr)
		{
			AttachChildren.Pop(false);
			continue;
		}
		Child->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
}

void USceneComponent::DestroyComponent(bool bPromoteChildren)
{
	DetachAllChildren();
	DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	Super::DestroyComponent(bPromoteChildren);
}

void USceneComponent::BeginDestroy()
{
	// The collector may free a parent and its children in one pass: unlink first so no side keeps a stale pointer.
	DetachAllChildren();
	DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	Super::BeginDestroy();
}
