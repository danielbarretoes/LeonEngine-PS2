#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CoreMinimal.h"
#include "VerticalBox.generated.h"

class FGenericWindow;

/**
 * UE-like UVerticalBox (lite): title + stacked buttons + hint.
 * Add through AHUD::AddWidget; call TickInput each frame from the game mode.
 */
UCLASS()
class UMG_API UVerticalBox : public UUserWidget
{
	GENERATED_BODY()

public:
	UVerticalBox(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetTitle(const FText& InTitle)
	{
		Title = InTitle;
	}
	void SetHint(const FText& InHint)
	{
		Hint = InHint;
	}

	void ClearChildren();

	/** Appends a button. NAME_None as the id makes a status row that cannot be activated. */
	UButton* AddButton(FName Id, const FText& Label);

	[[nodiscard]] int32 NumButtons() const
	{
		return Buttons.Num();
	}
	[[nodiscard]] UButton* GetButton(int32 Index);
	[[nodiscard]] const UButton* GetButton(int32 Index) const;

	[[nodiscard]] int32 SelectedIndex() const
	{
		return Selected;
	}
	void SetSelectedIndex(int32 Index);

	/** Seeds the edges as pressed + a short keyboard activate lockout (safe after travel). */
	void ResetEdges();

	/** The id of the button activated this frame (NAME_None if none). */
	[[nodiscard]] FName TickInput(FGenericWindow& Window, bool bCursorCaptured, float DeltaTime);

	void NativePaint(FPaintContext& Ctx) override;

private:
	void CacheLayout(int32 ViewportW, int32 ViewportH);
	void SnapSelectionToSelectable();
	void StepSelectable(int32 Delta);
	void ApplySelectionVisuals();
	[[nodiscard]] static bool IsSelectable(const UButton& Button);

	FText Title = FText::FromString("Menu");
	FText Hint;
	/** The buttons, created with the box as their outer. */
	UPROPERTY()
	TArray<UButton*> Buttons;
	int32 Selected = 0;

	bool bUpWasDown = false;
	bool bDownWasDown = false;
	bool bEnterWasDown = false;
	bool bMouseWasDown = false;
	/** Suppresses keyboard activate only (ghost Enter after travel); the mouse stays live. */
	float IgnoreActivateSeconds = 0.0f;

	float BoxX = 0.0f;
	float BoxY = 0.0f;
	float BoxW = 0.0f;
	float TitleH = 0.0f;
	float HintH = 0.0f;
	float ButtonGap = 8.0f;
	float MinButtonW = 220.0f;
};
