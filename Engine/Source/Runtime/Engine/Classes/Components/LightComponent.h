#pragma once

#include "Components/LightComponentBase.h"
#include "CoreMinimal.h"
#include "LightComponent.generated.h"

class FLightSceneProxy;

/**
 * A light that illuminates the world (UE: ULightComponent). It shines along its forward axis (+X): GetDirection is the
 * unit X axis of its world transform. Its render state is a FLightSceneProxy in the world's scene
 * (FSceneInterface::AddLight), while it is visible and its owner is not hidden.
 */
UCLASS(Abstract)
class ENGINE_API ULightComponent : public ULightComponentBase
{
	GENERATED_BODY()

public:
	ULightComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The direction the light travels (UE: GetDirection). */
	[[nodiscard]] FVector GetDirection() const;

	/** The renderer's snapshot of the light (UE: CreateSceneProxy), owned by the scene. */
	[[nodiscard]] virtual FLightSceneProxy* CreateSceneProxy() const;

	/** Sends the world transform to the proxy (FSceneInterface::UpdateLightTransform). */
	void SendRenderTransform_Concurrent() override;

	/** The proxy the scene made from the light, while it is in a scene (UE: SceneProxy). */
	FLightSceneProxy* SceneProxy = nullptr;

protected:
	void CreateRenderState_Concurrent() override;
	void DestroyRenderState_Concurrent() override;
};
