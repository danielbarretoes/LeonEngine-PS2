#pragma once

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "PanelWidget.generated.h"

class UPanelSlot;

/**
 * A widget holding children, each in a slot of the panel's slot class (UE: UPanelWidget). AddChild moves a child out
 * of any other panel first.
 */
UCLASS(Abstract)
class UMG_API UPanelWidget : public UWidget
{
	GENERATED_BODY()

public:
	UPanelWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Adds Content in a new slot; null for no content or a full single-child panel (UE: AddChild). */
	UPanelSlot* AddChild(UWidget* Content);
	/** Removes Content; false when it is not a child (UE: RemoveChild). */
	bool RemoveChild(UWidget* Content);
	/** Removes every child (UE: ClearChildren). */
	void ClearChildren();

	[[nodiscard]] int32 GetChildrenCount() const
	{
		return Slots.Num();
	}
	/** The child at Index, or null (UE: GetChildAt). */
	[[nodiscard]] UWidget* GetChildAt(int32 Index) const;
	/** The child's index, INDEX_NONE when it is not one (UE: GetChildIndex). */
	[[nodiscard]] int32 GetChildIndex(const UWidget* Content) const;
	[[nodiscard]] bool HasChild(const UWidget* Content) const
	{
		return GetChildIndex(Content) != INDEX_NONE;
	}
	[[nodiscard]] const TArray<UPanelSlot*>& GetSlots() const
	{
		return Slots;
	}

protected:
	/** The class of the panel's slots (UE: GetSlotClass). */
	[[nodiscard]] virtual UClass* GetSlotClass() const;
	/** Whether the panel holds more than one child (UE: bCanHaveMultipleChildren). */
	[[nodiscard]] virtual bool CanHaveMultipleChildren() const
	{
		return true;
	}

	/** UE: Slots. */
	UPROPERTY()
	TArray<UPanelSlot*> Slots;
};
