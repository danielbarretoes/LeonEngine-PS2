#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "VisibilityCellVolume.generated.h"

/**
 * A cell of a map's cells and portals (Leon, Docs/PLANS/ps2-shipping.md N15; UE 4.27 has no portals, its
 * APrecomputedVisibilityVolume is the nearest): the box of a glTF `VIS_<Cell>` node, named CellName. The renderer's
 * scene gathers a level's cells and portals (AVisibilityPortal) into its FVisibilityCellGraph when they initialize or
 * go, assigns every primitive to the cells its bounds touch, and each frame draws only what the cells the view sees
 * through the portals hold (FScene, the GS scene renderer). A map without cells draws everything.
 *
 * Its box is the volume's brush bounds (axis aligned, as every Leon volume's: plan decision D16). It collides with
 * nothing.
 */
UCLASS()
class ENGINE_API AVisibilityCellVolume : public AVolume
{
	GENERATED_BODY()

public:
	AVisibilityCellVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The cell's name: the node's suffix (`VIS_Mid` is `Mid`), which the portals name. */
	UPROPERTY()
	FName CellName;

	/** Tells the scene its cells changed (FSceneInterface::UpdateVisibilityCells). */
	void PostInitializeComponents() override;
	void Destroyed() override;
};
