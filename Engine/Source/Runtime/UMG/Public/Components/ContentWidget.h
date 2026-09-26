#pragma once

#include "Components/PanelWidget.h"
#include "CoreMinimal.h"
#include "ContentWidget.generated.h"

/** A panel with a single child (UE: UContentWidget). */
UCLASS(Abstract)
class UMG_API UContentWidget : public UPanelWidget
{
	GENERATED_BODY()

public:
	UContentWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The child, or null (UE: GetContent). */
	[[nodiscard]] UWidget* GetContent() const
	{
		return GetChildAt(0);
	}
	/** Replaces the child (UE: SetContent). */
	UPanelSlot* SetContent(UWidget* Content);

protected:
	bool CanHaveMultipleChildren() const override
	{
		return false;
	}
};
