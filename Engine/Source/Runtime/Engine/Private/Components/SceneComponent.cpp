#include "Components/SceneComponent.h"

#include "GameFramework/Actor.h"

namespace
{

	/** Writes a relative transform into the Relative* fields. */
	void SetRelativeFields(USceneComponent& Component, const FTransform& Relative)
	{
		Component.RelativeLocation = Relative.GetLocation();
		Component.RelativeRotation = Relative.Rotator();
		Component.RelativeScale3D = Relative.GetScale3D();
	}

	/** The owning actor's pose (UE: GetActorTransform). */
	FTransform ActorTransform(const AActor& Owner)
	{
		return FTransform(Owner.GetActorRotation(), Owner.GetActorLocation());
	}

} // namespace

USceneComponent::~USceneComponent()
{
	// UActorComponent dtor also calls DestroyComponent; detach scene links first while owner may
	// still be valid (Actor::~ clears owner before member USceneComponent dtors).
	while (Children.Num() > 0)
	{
		USceneComponent* Child = Children.Last();
		Child->DetachFromParent(false);
	}
	DetachFromParent(false);
}

FTransform USceneComponent::GetRelativeTransform() const
{
	return FTransform(RelativeRotation, RelativeLocation, RelativeScale3D);
}

bool USceneComponent::WouldCreateCycle(const USceneComponent* CandidateParent) const
{
	for (const USceneComponent* Walk = CandidateParent; Walk != nullptr; Walk = Walk->Parent)
	{
		if (Walk == this)
		{
			return true;
		}
	}
	return false;
}

void USceneComponent::DetachChild(USceneComponent* Child)
{
	Children.Remove(Child);
}

bool USceneComponent::AttachToComponent(USceneComponent* InParent, bool bKeepWorldTransform)
{
	if (InParent == nullptr || InParent == this || WouldCreateCycle(InParent))
	{
		return false;
	}

	FTransform WorldBefore;
	if (bKeepWorldTransform)
	{
		WorldBefore = GetComponentTransform();
	}

	DetachFromParent(false);
	Parent = InParent;
	Parent->Children.Add(this);
	if (Owner == nullptr)
	{
		Owner = InParent->Owner;
	}

	if (bKeepWorldTransform)
	{
		SetRelativeFields(*this, WorldBefore.GetRelativeTransform(Parent->GetComponentTransform()));
	}
	return true;
}

void USceneComponent::DetachFromParent(bool bKeepWorldTransform)
{
	if (Parent == nullptr)
	{
		return;
	}

	FTransform WorldBefore;
	if (bKeepWorldTransform)
	{
		WorldBefore = GetComponentTransform();
	}

	Parent->DetachChild(this);
	Parent = nullptr;

	if (bKeepWorldTransform)
	{
		// Without a parent the component sits on its owner's pose.
		SetRelativeFields(
			*this, Owner != nullptr ? WorldBefore.GetRelativeTransform(ActorTransform(*Owner)) : WorldBefore);
	}
}

FTransform USceneComponent::GetComponentTransform() const
{
	if (Parent != nullptr)
	{
		return GetRelativeTransform() * Parent->GetComponentTransform();
	}
	if (Owner != nullptr)
	{
		return GetRelativeTransform() * ActorTransform(*Owner);
	}
	return GetRelativeTransform();
}

FVector USceneComponent::GetComponentLocation() const
{
	return GetComponentTransform().GetLocation();
}

FRotator USceneComponent::GetComponentRotation() const
{
	return GetComponentTransform().Rotator();
}

void USceneComponent::DestroyComponent()
{
	while (Children.Num() > 0)
	{
		USceneComponent* Child = Children.Last();
		Child->DetachFromParent(false);
	}
	DetachFromParent(false);
	UActorComponent::DestroyComponent();
}
