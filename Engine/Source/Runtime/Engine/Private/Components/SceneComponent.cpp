#include "Components/SceneComponent.h"

#include "GameFramework/Actor.h"

namespace
{

	/** Column I of a GL-convention matrix (the image of axis I). */
	[[nodiscard]] FVector Column(const FMatrix& M, int32 I)
	{
		return FVector(M.M[I][0], M.M[I][1], M.M[I][2]);
	}

	[[nodiscard]] FLegacyTransform DecomposeApprox(const FMatrix& M)
	{
		FLegacyTransform T;
		T.Position = Column(M, 3);
		T.Scale.X = Column(M, 0).Size();
		T.Scale.Y = Column(M, 1).Size();
		T.Scale.Z = Column(M, 2).Size();
		constexpr float Eps = 1.0e-6f;
		constexpr float RadToDeg = 180.0f / 3.14159265358979323846f;
		const FVector Col0 = T.Scale.X > Eps ? Column(M, 0) / T.Scale.X : FVector(1.0f, 0.0f, 0.0f);
		const FVector Col1 = T.Scale.Y > Eps ? Column(M, 1) / T.Scale.Y : FVector(0.0f, 1.0f, 0.0f);
		const FVector Col2 = T.Scale.Z > Eps ? Column(M, 2) / T.Scale.Z : FVector(0.0f, 0.0f, 1.0f);
		// XYZ Euler extraction (degrees) matching FLegacyTransform::ModelMatrix order Rx*Ry*Rz.
		T.RotationDegrees.Y = FMath::Atan2(-Col0.Z, Col2.Z) * RadToDeg;
		T.RotationDegrees.X = FMath::Asin(FMath::Clamp(Col1.Z, -1.0f, 1.0f)) * RadToDeg;
		T.RotationDegrees.Z = FMath::Atan2(-Col1.X, Col1.Y) * RadToDeg;
		return T;
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

FLegacyTransform USceneComponent::GetRelativeTransform() const
{
	FLegacyTransform T;
	T.Position = RelativeLocation;
	T.RotationDegrees = RelativeRotation;
	T.Scale = RelativeScale;
	return T;
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

	FMatrix WorldBefore = FMatrix::Identity;
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
		const FMatrix ParentWorld = Parent->GetComponentTransform();
		const FMatrix ParentInv = ParentWorld.Inverse();
		const FLegacyTransform Relative = DecomposeApprox(WorldBefore * ParentInv);
		RelativeLocation = Relative.Position;
		RelativeRotation = Relative.RotationDegrees;
		RelativeScale = Relative.Scale;
	}
	return true;
}

void USceneComponent::DetachFromParent(bool bKeepWorldTransform)
{
	if (Parent == nullptr)
	{
		return;
	}

	FMatrix WorldBefore = FMatrix::Identity;
	if (bKeepWorldTransform)
	{
		WorldBefore = GetComponentTransform();
	}

	Parent->DetachChild(this);
	Parent = nullptr;

	if (bKeepWorldTransform)
	{
		const FLegacyTransform World = DecomposeApprox(WorldBefore);
		RelativeLocation = World.Position;
		RelativeRotation = World.RotationDegrees;
		RelativeScale = World.Scale;
		if (Owner != nullptr)
		{
			RelativeLocation -= Owner->GetActorLocation();
			RelativeRotation.Y -= Owner->GetActorYaw();
		}
	}
}

FMatrix USceneComponent::GetComponentTransform() const
{
	const FLegacyTransform Relative = GetRelativeTransform();
	if (Parent != nullptr)
	{
		return Relative.ModelMatrix() * Parent->GetComponentTransform();
	}
	if (Owner != nullptr)
	{
		FLegacyTransform World;
		World.Position = Owner->GetActorLocation() + RelativeLocation;
		World.RotationDegrees = RelativeRotation;
		World.RotationDegrees.Y += Owner->GetActorYaw();
		World.Scale = RelativeScale;
		return World.ModelMatrix();
	}
	return Relative.ModelMatrix();
}

FVector USceneComponent::GetComponentLocation() const
{
	const FMatrix World = GetComponentTransform();
	return FVector(World.M[3][0], World.M[3][1], World.M[3][2]);
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
