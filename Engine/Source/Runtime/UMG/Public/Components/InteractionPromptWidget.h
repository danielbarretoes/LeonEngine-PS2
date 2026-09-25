#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"
#include "InteractionPromptWidget.generated.h"

/** Centered outlined interact hint ("[F] Open Door [750]"). An empty Prompt skips the paint. */
UCLASS()
class UMG_API UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInteractionPromptWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FText Prompt;

	FLinearColor Color = FLinearColor(0.95f, 0.9f, 0.45f);
	float Scale = 2.4f;
	/** Vertical placement as a fraction of the viewport height (0 = top, 1 = bottom). */
	float NormalizedY = 0.62f;
	ETextJustify Justify = ETextJustify::Center;

	void NativePaint(FPaintContext& Ctx) override;
};
