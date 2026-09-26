#pragma once

#include "Components/PanelSlot.h"
#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"
#include "VerticalBoxSlot.generated.h"

/** A child's place in a vertical box: its padding and horizontal alignment (UE: UVerticalBoxSlot). */
UCLASS()
class UMG_API UVerticalBoxSlot : public UPanelSlot
{
	GENERATED_BODY()

public:
	UVerticalBoxSlot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetPadding(const FMargin& InPadding)
	{
		Padding = InPadding;
	}
	[[nodiscard]] const FMargin& GetPadding() const
	{
		return Padding;
	}
	void SetHorizontalAlignment(EHorizontalAlignment InAlignment)
	{
		HorizontalAlignment = InAlignment;
	}
	[[nodiscard]] EHorizontalAlignment GetHorizontalAlignment() const
	{
		return HorizontalAlignment;
	}

private:
	FMargin Padding;
	EHorizontalAlignment HorizontalAlignment = HAlign_Fill;
};
