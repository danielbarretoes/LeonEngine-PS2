#include "Components/VerticalBox.h"

#include "Components/VerticalBoxSlot.h"

UVerticalBox::UVerticalBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UClass* UVerticalBox::GetSlotClass() const
{
	return UVerticalBoxSlot::StaticClass();
}

UVerticalBoxSlot* UVerticalBox::AddChildToVerticalBox(UWidget* Content)
{
	return Cast<UVerticalBoxSlot>(AddChild(Content));
}

FVector2D UVerticalBox::ComputeDesiredSize() const
{
	FVector2D Desired = FVector2D::ZeroVector;
	for (const UPanelSlot* PanelSlot : Slots)
	{
		const UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(PanelSlot);
		if (BoxSlot->Content->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		const FVector2D Padded = BoxSlot->Content->GetDesiredSize() + BoxSlot->GetPadding().GetDesiredSize();
		Desired.X = FMath::Max(Desired.X, Padded.X);
		Desired.Y += Padded.Y;
	}
	return Desired;
}

void UVerticalBox::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	float Y = Position.Y;
	for (const UPanelSlot* PanelSlot : Slots)
	{
		const UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(PanelSlot);
		const UWidget* Child = BoxSlot->Content;
		if (Child->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}
		const FMargin& Padding = BoxSlot->GetPadding();
		const FVector2D Desired = Child->GetDesiredSize();
		const float Available = FMath::Max(0.0f, Size.X - Padding.Left - Padding.Right);
		float Width = Available;
		float X = Position.X + Padding.Left;
		switch (BoxSlot->GetHorizontalAlignment())
		{
			case HAlign_Fill:
				break;
			case HAlign_Left:
				Width = FMath::Min(Desired.X, Available);
				break;
			case HAlign_Center:
				Width = FMath::Min(Desired.X, Available);
				X += (Available - Width) * 0.5f;
				break;
			case HAlign_Right:
				Width = FMath::Min(Desired.X, Available);
				X += Available - Width;
				break;
		}
		Y += Padding.Top;
		Child->Paint(Ctx, FVector2D(X, Y), FVector2D(Width, Desired.Y));
		Y += Desired.Y + Padding.Bottom;
	}
}
