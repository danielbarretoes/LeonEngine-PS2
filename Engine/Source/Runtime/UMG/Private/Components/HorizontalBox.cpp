#include "Components/HorizontalBox.h"

#include "Components/HorizontalBoxSlot.h"

UHorizontalBox::UHorizontalBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UClass* UHorizontalBox::GetSlotClass() const
{
	return UHorizontalBoxSlot::StaticClass();
}

UHorizontalBoxSlot* UHorizontalBox::AddChildToHorizontalBox(UWidget* Content)
{
	return Cast<UHorizontalBoxSlot>(AddChild(Content));
}

FVector2D UHorizontalBox::ComputeDesiredSize() const
{
	FVector2D Desired = FVector2D::ZeroVector;
	for (const UPanelSlot* PanelSlot : Slots)
	{
		const UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot);
		if (BoxSlot->Content->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		const FVector2D Padded = BoxSlot->Content->GetDesiredSize() + BoxSlot->GetPadding().GetDesiredSize();
		Desired.X += Padded.X;
		Desired.Y = FMath::Max(Desired.Y, Padded.Y);
	}
	return Desired;
}

void UHorizontalBox::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	// The automatic slots' widths and the fill slots' total share, then what is left split by share.
	float AutomaticWidth = 0.0f;
	float FillShares = 0.0f;
	for (const UPanelSlot* PanelSlot : Slots)
	{
		const UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot);
		if (BoxSlot->Content->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		AutomaticWidth += BoxSlot->GetPadding().Left + BoxSlot->GetPadding().Right;
		if (BoxSlot->GetSize().SizeRule == ESlateSizeRule::Fill)
		{
			FillShares += FMath::Max(0.0f, BoxSlot->GetSize().Value);
		}
		else
		{
			AutomaticWidth += BoxSlot->Content->GetDesiredSize().X;
		}
	}
	const float FillWidth = FMath::Max(0.0f, Size.X - AutomaticWidth);
	float X = Position.X;
	for (const UPanelSlot* PanelSlot : Slots)
	{
		const UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot);
		const UWidget* Child = BoxSlot->Content;
		if (Child->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		const FMargin& Padding = BoxSlot->GetPadding();
		const FVector2D Desired = Child->GetDesiredSize();
		const bool bFill = BoxSlot->GetSize().SizeRule == ESlateSizeRule::Fill;
		const float SlotWidth = bFill
			? (FillShares > 0.0f ? FillWidth * FMath::Max(0.0f, BoxSlot->GetSize().Value) / FillShares : 0.0f)
			: Desired.X;
		X += Padding.Left;
		// Across the slot's width (a fill slot's child may be narrower) and across the box's height.
		float ChildX = X;
		float ChildWidth = SlotWidth;
		if (BoxSlot->GetHorizontalAlignment() != HAlign_Fill && Desired.X < SlotWidth)
		{
			ChildWidth = Desired.X;
			const float Spare = SlotWidth - Desired.X;
			ChildX += BoxSlot->GetHorizontalAlignment() == HAlign_Center ? Spare * 0.5f
				: BoxSlot->GetHorizontalAlignment() == HAlign_Right      ? Spare
																		 : 0.0f;
		}
		const float Available = FMath::Max(0.0f, Size.Y - Padding.Top - Padding.Bottom);
		float ChildY = Position.Y + Padding.Top;
		float ChildHeight = Available;
		if (BoxSlot->GetVerticalAlignment() != VAlign_Fill)
		{
			ChildHeight = FMath::Min(Desired.Y, Available);
			const float Spare = Available - ChildHeight;
			ChildY += BoxSlot->GetVerticalAlignment() == VAlign_Center ? Spare * 0.5f
				: BoxSlot->GetVerticalAlignment() == VAlign_Bottom     ? Spare
																	   : 0.0f;
		}
		Child->Paint(Ctx, FVector2D(ChildX, ChildY), FVector2D(ChildWidth, ChildHeight));
		X += SlotWidth + Padding.Right;
	}
}
