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
