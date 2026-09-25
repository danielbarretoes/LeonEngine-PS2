#pragma once

#include "CollisionShape.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "PrimitiveComponent.generated.h"

class FSceneRenderer;

/**
 * A scene component with geometry: something drawn, collided with or both (UE: UPrimitiveComponent).
 *
 * Its render state is its entry in the world's primitive list: registering in a world adds it
 * (CreateRenderState_Concurrent → UWorld::AddPrimitive) and each gameplay frame the world asks every visible primitive
 * to SubmitDraw. P13 replaces the list with FScene::AddPrimitive and a FPrimitiveSceneProxy. Physics bodies still come
 * from the legacy level meshes (UWorld::RegisterBodiesFromLevel); P13 moves them to CreatePhysicsState.
 */
UCLASS(Abstract)
class ENGINE_API UPrimitiveComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPrimitiveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Casts a shadow (UE: CastShadow). */
	UPROPERTY()
	uint8 CastShadow : 1;

	/** Reports overlaps (UE: bGenerateOverlapEvents; kept for the UE shape, nothing queries overlaps yet). */
	UPROPERTY()
	uint8 bGenerateOverlapEvents : 1;

	/** The collision shape in world units, grown by Inflation (UE: GetCollisionShape); a line by default. */
	[[nodiscard]] virtual FCollisionShape GetCollisionShape(float Inflation = 0.0f) const;

	/** Draws the component (Leon until P13's scene proxies). Nothing by default. */
	virtual void SubmitDraw(FSceneRenderer& Renderer) const;

	/** True when the world should draw it: visible, and its owner is not hidden. */
	[[nodiscard]] bool ShouldRender() const;

protected:
	void CreateRenderState_Concurrent() override;
	void DestroyRenderState_Concurrent() override;
};
