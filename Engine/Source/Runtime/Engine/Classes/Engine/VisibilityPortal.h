#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisibilityPortal.generated.h"

/**
 * A portal between two cells of a map (Leon, Docs/PLANS/ps2-shipping.md N15; see AVisibilityCellVolume): the quad of a
 * glTF `PORTAL_<CellA>_<CellB>` node, an opening (a door, a window, a corridor's end) the view can see one cell from
 * the other through. Two ways: either cell sees the other through it.
 */
UCLASS()
class ENGINE_API AVisibilityPortal : public AActor
{
	GENERATED_BODY()

public:
	AVisibilityPortal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The cells it joins (their AVisibilityCellVolume::CellName). */
	UPROPERTY()
	FName CellA;

	UPROPERTY()
	FName CellB;

	/** The quad's four corners in the world, in order around it (cm). */
	UPROPERTY()
	TArray<FVector> Corners;

	/** Tells the scene its portals changed (FSceneInterface::UpdateVisibilityCells). */
	void PostInitializeComponents() override;
	void Destroyed() override;
};
