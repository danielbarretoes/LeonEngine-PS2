#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PanelSlot.generated.h"

class UPanelWidget;
class UWidget;

/** Where a panel holds one child, and how it lays it out (UE: UPanelSlot, a UVisual there). */
UCLASS()
class UMG_API UPanelSlot : public UObject
{
	GENERATED_BODY()

public:
	UPanelSlot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The panel (UE: Parent). */
	UPROPERTY()
	UPanelWidget* Parent = nullptr;

	/** The child (UE: Content). */
	UPROPERTY()
	UWidget* Content = nullptr;
};
