#include "Components/LightComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LightSceneProxy.h"
#include "SceneInterface.h"

ULightComponent::ULightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector ULightComponent::GetDirection() const
{
	const FVector Direction = GetComponentTransform().GetRotation().GetForwardVector();
	return Direction / Direction.Size();
}

FLightSceneProxy* ULightComponent::CreateSceneProxy() const
{
	return new FLightSceneProxy(this);
}

void ULightComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	// UE: a light affects the world while it is visible (bAffectsWorld, bVisible) and its owner is not hidden.
	if (World != nullptr && World->Scene != nullptr && IsVisible() && (Owner == nullptr || !Owner->IsHidden()))
	{
		World->Scene->AddLight(this);
	}
}

void ULightComponent::DestroyRenderState_Concurrent()
{
	UWorld* World = GetWorld();
	if (World != nullptr && World->Scene != nullptr)
	{
		World->Scene->RemoveLight(this);
	}
	SceneProxy = nullptr;
	Super::DestroyRenderState_Concurrent();
}

void ULightComponent::SendRenderTransform_Concurrent()
{
	UWorld* World = GetWorld();
	if (SceneProxy != nullptr && World != nullptr && World->Scene != nullptr)
	{
		World->Scene->UpdateLightTransform(this);
	}
}
