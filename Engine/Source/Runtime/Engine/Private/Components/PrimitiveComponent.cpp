#include "Components/PrimitiveComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SceneInterface.h"

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

FPrimitiveSceneProxy* UPrimitiveComponent::CreateSceneProxy()
{
	return nullptr;
}

bool UPrimitiveComponent::ShouldRender() const
{
	const AActor* Owner = GetOwner();
	return IsVisible() && (Owner == nullptr || !Owner->IsHidden());
}

void UPrimitiveComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();
	UWorld* World = GetWorld();
	if (World != nullptr && World->Scene != nullptr)
	{
		World->Scene->AddPrimitive(this);
	}
}

void UPrimitiveComponent::DestroyRenderState_Concurrent()
{
	UWorld* World = GetWorld();
	if (World != nullptr && World->Scene != nullptr)
	{
		World->Scene->RemovePrimitive(this);
	}
	SceneProxy = nullptr;
	Super::DestroyRenderState_Concurrent();
}

void UPrimitiveComponent::SendRenderTransform_Concurrent()
{
	UWorld* World = GetWorld();
	if (SceneProxy != nullptr && World != nullptr && World->Scene != nullptr)
	{
		World->Scene->UpdatePrimitiveTransform(this);
	}
}

void UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled NewType)
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
