#include "Components/SceneComponent.h"

#include "GameFramework/Actor.h"

namespace
{

	/** Writes a relative transform back into the legacy Relative* fields. */
	void SetRelativeFields(USceneComponent& Component, const FTransform& Relative)
	{
		Component.RelativeLocation = FLegacyCoordinateConversion::ToLegacyPosition(Relative.GetLocation());
		Component.RelativeRotation = FLegacyCoordinateConversion::ToLegacyEulerXYZ(Relative.GetRotation());
		Component.RelativeScale3D = FLegacyCoordinateConversion::ToLegacyScale(Relative.GetScale3D());
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
	return FLegacyCoordinateConversion::ConvertTransform(RelativeLocation, RelativeRotation, RelativeScale3D);
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
		SetRelativeFields(*this, WorldBefore);
		if (Owner != nullptr)
		{
			RelativeLocation -= FLegacyCoordinateConversion::ToLegacyPosition(Owner->GetActorLocation());
			RelativeRotation.Y -= Owner->GetActorYaw();
		}
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
		// The owner's legacy yaw adds to the legacy Euler Y.
		FVector Euler = RelativeRotation;
		Euler.Y += Owner->GetActorYaw();
		const FVector Location =
			FLegacyCoordinateConversion::ToLegacyPosition(Owner->GetActorLocation()) + RelativeLocation;
		return FLegacyCoordinateConversion::ConvertTransform(Location, Euler, RelativeScale3D);
	}
	return GetRelativeTransform();
}

FVector USceneComponent::GetComponentLocation() const
{
	return GetComponentTransform().GetLocation();
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
