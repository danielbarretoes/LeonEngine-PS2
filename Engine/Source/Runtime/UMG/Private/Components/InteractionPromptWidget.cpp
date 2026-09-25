#include "Components/InteractionPromptWidget.h"

UInteractionPromptWidget::UInteractionPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

namespace
{

	void DrawOutlinedText(FPaintContext& Ctx, const FString& Text, float X, float Y, const FLinearColor& Color,
		float Scale, ETextJustify Justify)
	{
		const FLinearColor Shadow(0.02f, 0.02f, 0.02f);
		Ctx.DrawText(Text, X + 2.0f, Y + 2.0f, Shadow, Scale, Justify);
		Ctx.DrawText(Text, X, Y, Color, Scale, Justify);
	}

} // namespace

void UInteractionPromptWidget::NativePaint(FPaintContext& Ctx)
{
	if (Prompt.IsEmpty())
	{
		return;
	}
	const float X = static_cast<float>(Ctx.GetWidth()) * 0.5f;
	const float Y = static_cast<float>(Ctx.GetHeight()) * NormalizedY;
	DrawOutlinedText(Ctx, Prompt.ToString(), X, Y, Color, Scale, Justify);
}
