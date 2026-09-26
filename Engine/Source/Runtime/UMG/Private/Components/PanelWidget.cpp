#include "Components/PanelWidget.h"

#include "Components/PanelSlot.h"

UPanelWidget::UPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UClass* UPanelWidget::GetSlotClass() const
{
	return UPanelSlot::StaticClass();
}

UPanelSlot* UPanelWidget::AddChild(UWidget* Content)
{
	if (Content == nullptr || (!CanHaveMultipleChildren() && Slots.Num() > 0))
	{
		return nullptr;
	}
	Content->RemoveFromParent();
	UPanelSlot* NewSlot = NewObject<UPanelSlot>(this, GetSlotClass());
	NewSlot->Parent = this;
	NewSlot->Content = Content;
	Content->Slot = NewSlot;
	Slots.Add(NewSlot);
	return NewSlot;
}

bool UPanelWidget::RemoveChild(UWidget* Content)
{
	const int32 Index = GetChildIndex(Content);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	UPanelSlot* OldSlot = Slots[Index];
	Slots.RemoveAt(Index);
	Content->Slot = nullptr;
	OldSlot->Parent = nullptr;
	OldSlot->Content = nullptr;
	return true;
}

void UPanelWidget::ClearChildren()
{
	while (Slots.Num() > 0)
	{
		(void)RemoveChild(Slots.Last()->Content);
	}
}

UWidget* UPanelWidget::GetChildAt(int32 Index) const
{
	return Slots.IsValidIndex(Index) ? Slots[Index]->Content : nullptr;
}

int32 UPanelWidget::GetChildIndex(const UWidget* Content) const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Content != nullptr && Slots[Index]->Content == Content)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
