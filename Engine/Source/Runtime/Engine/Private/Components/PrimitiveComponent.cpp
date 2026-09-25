#include "Components/PrimitiveComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

UPrimitiveComponent::UPrimitiveComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadow = true;
	bGenerateOverlapEvents = true;
}

FCollisionShape UPrimitiveComponent::GetCollisionShape(float /*Inflation*/) const
{
	return FCollisionShape();
}

void UPrimitiveComponent::SubmitDraw(FSceneRenderer& /*Renderer*/) const
{
}

bool UPrimitiveComponent::ShouldRender() const
{
	const AActor* Owner = GetOwner();
	return IsVisible() && (Owner == nullptr || !Owner->IsHidden());
}

void UPrimitiveComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();
	if (UWorld* World = GetWorld())
	{
		World->AddPrimitive(this);
	}
}

void UPrimitiveComponent::DestroyRenderState_Concurrent()
{
	if (UWorld* World = GetWorld())
	{
		World->RemovePrimitive(this);
	}
	Super::DestroyRenderState_Concurrent();
}

void UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled::Type NewType)
{
	if (CollisionEnabled == NewType)
	{
		return;
	}
	CollisionEnabled = NewType;
	RecreatePhysicsState();
}

void UPrimitiveComponent::SetSimulatePhysics(bool bSimulate)
{
	if (bSimulatePhysics == bSimulate)
	{
		return;
	}
	bSimulatePhysics = bSimulate;
	RecreatePhysicsState();
}

void UPrimitiveComponent::SetEnableGravity(bool bGravityEnabled)
{
	if (bEnableGravity == bGravityEnabled)
	{
		return;
	}
	bEnableGravity = bGravityEnabled;
	RecreatePhysicsState();
}

void UPrimitiveComponent::CreatePhysicsState()
{
	Super::CreatePhysicsState();
	UWorld* World = GetWorld();
	if (World != nullptr && IsCollisionEnabled())
	{
		World->GetPhysicsScene().AddComponentBody(*this);
	}
}

void UPrimitiveComponent::DestroyPhysicsState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetPhysicsScene().RemoveComponentBody(*this);
	}
	Super::DestroyPhysicsState();
}
