#include "Components/SceneComponent.h"

#include "GameFramework/Actor.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace
{

	[[nodiscard]] FLegacyTransform DecomposeApprox(const glm::mat4& M)
	{
		FLegacyTransform T{};
		T.Position = glm::vec3(M[3]);
		T.Scale.x = glm::length(glm::vec3(M[0]));
		T.Scale.y = glm::length(glm::vec3(M[1]));
		T.Scale.z = glm::length(glm::vec3(M[2]));
		constexpr float Eps = 1.0e-6f;
		const glm::vec3 Col0 = T.Scale.x > Eps ? glm::vec3(M[0]) / T.Scale.x : glm::vec3(1.0f, 0.0f, 0.0f);
		const glm::vec3 Col1 = T.Scale.y > Eps ? glm::vec3(M[1]) / T.Scale.y : glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::vec3 Col2 = T.Scale.z > Eps ? glm::vec3(M[2]) / T.Scale.z : glm::vec3(0.0f, 0.0f, 1.0f);
		// XYZ Euler extraction (degrees) matching FLegacyTransform::ModelMatrix order Rx*Ry*Rz.
		T.RotationDegrees.y = std::atan2(-Col0.z, Col2.z) * (180.0f / 3.14159265358979323846f);
		T.RotationDegrees.x = std::asin(std::clamp(Col1.z, -1.0f, 1.0f)) * (180.0f / 3.14159265358979323846f);
		T.RotationDegrees.z = std::atan2(-Col1.x, Col1.y) * (180.0f / 3.14159265358979323846f);
		(void)Col2;
		return T;
	}

} // namespace

USceneComponent::~USceneComponent()
{
	// UActorComponent dtor also calls DestroyComponent; detach scene links first while owner may
	// still be valid (Actor::~ clears owner before member USceneComponent dtors).
	while (!Children.empty())
	{
		USceneComponent* Child = Children.back();
		Child->DetachFromParent(false);
	}
	DetachFromParent(false);
}

FLegacyTransform USceneComponent::GetRelativeTransform() const
{
	FLegacyTransform T{};
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
	Children.erase(std::remove(Children.begin(), Children.end(), Child), Children.end());
}

bool USceneComponent::AttachToComponent(USceneComponent* InParent, bool bKeepWorldTransform)
{
	if (InParent == nullptr || InParent == this || WouldCreateCycle(InParent))
	{
		return false;
	}

	glm::mat4 WorldBefore{};
	if (bKeepWorldTransform)
	{
		WorldBefore = GetComponentTransform();
	}

	DetachFromParent(false);
	Parent = InParent;
	Parent->Children.push_back(this);
	if (Owner == nullptr)
	{
		Owner = InParent->Owner;
	}

	if (bKeepWorldTransform)
	{
		const glm::mat4 ParentWorld = Parent->GetComponentTransform();
		const glm::mat4 ParentInv = glm::inverse(ParentWorld);
		const FLegacyTransform Relative = DecomposeApprox(ParentInv * WorldBefore);
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

	glm::mat4 WorldBefore{};
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
			RelativeRotation.y -= Owner->GetActorYaw();
		}
	}
}

glm::mat4 USceneComponent::GetComponentTransform() const
{
	const FLegacyTransform Relative = GetRelativeTransform();
	if (Parent != nullptr)
	{
		return Parent->GetComponentTransform() * Relative.ModelMatrix();
	}
	if (Owner != nullptr)
	{
		FLegacyTransform World{};
		World.Position = Owner->GetActorLocation() + RelativeLocation;
		World.RotationDegrees = RelativeRotation;
		World.RotationDegrees.y += Owner->GetActorYaw();
		World.Scale = RelativeScale;
		return World.ModelMatrix();
	}
	return Relative.ModelMatrix();
}

glm::vec3 USceneComponent::GetComponentLocation() const
{
	return glm::vec3(GetComponentTransform()[3]);
}

void USceneComponent::DestroyComponent()
{
	while (!Children.empty())
	{
		USceneComponent* Child = Children.back();
		Child->DetachFromParent(false);
	}
	DetachFromParent(false);
	UActorComponent::DestroyComponent();
}
