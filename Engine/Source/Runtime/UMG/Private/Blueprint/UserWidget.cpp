#include "Blueprint/UserWidget.h"

#include "Blueprint/WidgetTree.h"

UUserWidget::UUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UUserWidget::Initialize()
{
	if (WidgetTree != nullptr)
	{
		return false;
	}
	WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	NativeOnInitialized();
	return true;
}

void UUserWidget::NativePaint(FPaintContext& Ctx)
{
	Paint(
		Ctx, FVector2D::ZeroVector, FVector2D(static_cast<float>(Ctx.GetWidth()), static_cast<float>(Ctx.GetHeight())));
}

FVector2D UUserWidget::ComputeDesiredSize() const
{
	return WidgetTree != nullptr && WidgetTree->RootWidget != nullptr ? WidgetTree->RootWidget->GetDesiredSize()
																	  : FVector2D::ZeroVector;
}

void UUserWidget::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	if (WidgetTree != nullptr && WidgetTree->RootWidget != nullptr)
	{
		WidgetTree->RootWidget->Paint(Ctx, Position, Size);
	}
}
