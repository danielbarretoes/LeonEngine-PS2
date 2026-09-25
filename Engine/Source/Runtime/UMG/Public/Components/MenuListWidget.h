#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"
#include "MenuListWidget.generated.h"

class FGenericWindow;

/**
 * UE-like vertical text menu (UMG ListView lite): arrows / Enter / click.
 * Add through AHUD::AddWidget; call TickInput each frame from the game mode.
 */
UCLASS()
class UMG_API UMenuListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMenuListWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	struct FItem
	{
		/** Reported by TickInput when activated; NAME_None makes a status row. */
		FName Id;
		FText Label;
	};

	void SetTitle(const FText& InTitle)
	{
		Title = InTitle;
	}
	void SetHint(const FText& InHint)
	{
		Hint = InHint;
	}
	void SetItems(TArray<FItem> InItems);
	void SetColor(const FLinearColor& InColor)
	{
		Color = InColor;
	}

	[[nodiscard]] int32 SelectedIndex() const
	{
		return Selected;
	}
	void SetSelectedIndex(int32 Index);

	/** Seeds the edges as pressed + a short activate lockout (safe after travel). */
	void ResetEdges();

	/** The id of the item activated this frame (NAME_None if none). */
	[[nodiscard]] FName TickInput(FGenericWindow& Window, bool bCursorCaptured, float DeltaTime);

	void NativePaint(FPaintContext& Ctx) override;

private:
	[[nodiscard]] FString BuildPaintText() const;
	[[nodiscard]] static int32 CountLines(const FString& Text);
	void CacheLayout(int32 ViewportW, int32 InViewportH);

	FText Title = FText::FromString("Menu");
	FText Hint;
	TArray<FItem> Items;
	int32 Selected = 0;
	FLinearColor Color = FLinearColor(1.0f, 0.82f, 0.35f);

	bool bUpWasDown = false;
	bool bDownWasDown = false;
	bool bEnterWasDown = false;
	bool bMouseWasDown = false;
	/** Suppresses keyboard activate only (ghost Enter after travel); the mouse stays live. */
	float IgnoreActivateSeconds = 0.0f;

	// Layout cached for hit-testing (Paint + TickInput).
	float ItemsTopPx = 0.0f;
	float LineH = HudLineHeight;
	int32 ViewportH = 0;
};
