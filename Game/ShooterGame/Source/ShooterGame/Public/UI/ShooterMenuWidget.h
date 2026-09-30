#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CoreMinimal.h"
#include "ShooterTypes.h"
#include "ShooterMenuWidget.generated.h"

class AShooterPlayerController;
class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnShooterMenuValueStep, int32);

/**
 * A line of a ShooterGame menu (ps2-polish P9): a button with a label and, for an option, its value on the right
 * (`Difficulty    < Normal >`). Left and right (the arrows, the D-pad) step the option's value (OnValueStep with -1 or
 * +1) instead of moving the focus; a click or Accept (Enter, Cross) steps it forward, or does the line's action
 * (OnClicked) for a line without a value.
 */
UCLASS()
class SHOOTERGAME_API UShooterMenuButton : public UButton
{
	GENERATED_BODY()

public:
	UShooterMenuButton(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Left (-1) or right (+1) on an option line; a click on it steps +1. */
	FOnShooterMenuValueStep OnValueStep;

	/** The line's label and its value (null for a line without one). */
	[[nodiscard]] UTextBlock* GetLabel() const
	{
		return Label;
	}
	[[nodiscard]] UTextBlock* GetValue() const
	{
		return Value;
	}
	/** Shows the value as `< Text >` (an option line). */
	void SetValueText(const FString& Text);

	/** Builds the line in Tree: its label, and a value when bIsOption (UShooterMenuWidget::AddMenuButton). */
	void Build(class UWidgetTree& Tree, const FString& InLabel, bool bIsOption, int32 FontSize);

	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	/** A click on an option line steps it. */
	void OnOptionClicked();

	UPROPERTY()
	UTextBlock* Label = nullptr;
	UPROPERTY()
	UTextBlock* Value = nullptr;
};

/**
 * The options page (ps2-polish P9), the main menu's and the pause menu's: the aim sensitivity, the inverted Y axis,
 * the volume and the crouch key (a toggle or held), the player's UShooterPersistentUser. A step applies at once
 * (AShooterPlayerController::ApplyPersistentUser); leaving the page saves them (the memory card on the PS2), once.
 */
struct SHOOTERGAME_API FShooterOptionsPage
{
	/** The sensitivity's steps and range. */
	static constexpr float SensitivityStep = 0.25f;
	static constexpr float MinSensitivity = 0.25f;
	static constexpr float MaxSensitivity = 3.0f;
	/** The volume's step (10 %). */
	static constexpr float VolumeStep = 0.1f;

	UShooterMenuButton* Sensitivity = nullptr;
	UShooterMenuButton* InvertY = nullptr;
	UShooterMenuButton* Volume = nullptr;
	UShooterMenuButton* Crouch = nullptr;
	UShooterMenuButton* Back = nullptr;
	/** An option changed since the page was shown: leaving it saves. */
	bool bDirty = false;

	/** Steps an option of the player's options and applies them. */
	void StepSensitivity(AShooterPlayerController* Player, int32 Direction);
	void StepInvertY(AShooterPlayerController* Player);
	void StepVolume(AShooterPlayerController* Player, int32 Direction);
	void StepCrouch(AShooterPlayerController* Player);
	/** Shows the options' values. */
	void Refresh(AShooterPlayerController* Player) const;
	/** Saves them when one changed. */
	void SaveIfDirty(AShooterPlayerController* Player);
};

/**
 * The base of ShooterGame's menus (ps2-polish P9; UE ShooterGame's FShooterMenuWidget, a UMG user widget here): a
 * centred panel (a UBorder around a UVerticalBox: a title and a UWidgetSwitcher of pages of menu lines), readable at
 * the PS2's 640 x 448 with the engine's fonts (the title 32 px, the lines 20 px, the hints 14 px), and a hint line
 * at the bottom.
 *
 * - The pad alone, or the keyboard and the mouse, drive it (UMG's focus, P5): the arrows and the D-pad move between
 *   the lines, Accept (Enter, Space, Cross) presses one, Back (Escape, Backspace, Circle) calls OnBack, left and right
 *   step an option (UShooterMenuButton). On Win64 the mouse points and clicks while a menu has the input
 *   (FInputModeUIOnly).
 * - It is modal while shown: it takes every key that is not a navigation key (the game sees none), so the keys it
 *   does not use do not reach the player's input either. A collapsed menu takes nothing.
 * - Showing a page focuses its first line (ShowPage).
 */
UCLASS(Abstract)
class SHOOTERGAME_API UShooterMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UShooterMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The menu's colours: the panel, the lines (normal, focused or hovered, pressed), the text, the title. */
	static const FLinearColor PanelColor;
	static const FLinearColor TextColor;
	static const FLinearColor TitleColor;
	static const FLinearColor HintColor;

	void NativeOnInitialized() override;
	/** Centres the panel in the frame before painting it (the canvas's size is only known here). */
	void NativePaint(FPaintContext& Ctx) override;
	/** Back calls OnBack; any other key but the navigation ones is taken (the menu is modal). */
	FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Shows the menu (and focuses its page's first line) or collapses it. */
	virtual void SetShown(bool bShown);
	[[nodiscard]] bool IsShown() const
	{
		return IsVisible();
	}

	/** The page shown (an index of the switcher), and shows another one, focusing its first line. */
	[[nodiscard]] int32 GetPage() const;
	void ShowPage(int32 Page);

	/** The first line of a page (the focus's first stop), or null. */
	[[nodiscard]] UShooterMenuButton* GetFirstButton(int32 Page) const;

	/** The panel (its size and place: the tests). */
	[[nodiscard]] UBorder* GetPanel() const
	{
		return Panel;
	}

	/** The player controller of the HUD that shows the menu, or null. */
	[[nodiscard]] AShooterPlayerController* GetShooterPlayerController() const;

	/** Plays a UI cue (the audio device's; Confirm for a confirmation, else Click). */
	static void PlayMenuSoundStatic(bool bConfirm);

protected:
	/** Builds the pages (NativeOnInitialized calls it after making the panel): AddPage, AddMenuButton. */
	virtual void BuildPages()
	{
	}
	/** Back (Escape, Circle): nothing by default. */
	virtual void OnBack()
	{
	}
	/** A key the menu handles itself before the modal rule (Start for the pause menu); unhandled by default. */
	virtual FReply HandleMenuKey(const FKeyEvent& InKeyEvent)
	{
		(void)InKeyEvent;
		return FReply::Unhandled();
	}

	/** Adds a page to the switcher: a vertical box of lines (returns its index in OutPage). */
	UVerticalBox* AddPage(int32& OutPage);
	/** Adds a line to a page: an action (OnClicked) or an option (OnValueStep; bIsOption). */
	UShooterMenuButton* AddMenuButton(UVerticalBox& Page, const FString& InLabel, bool bIsOption = false);
	/** Adds a text line to a page (a note, a status). */
	UTextBlock* AddTextLine(UVerticalBox& Page, const FString& Text, int32 FontSize, const FLinearColor& Color);

	/** Sets the title and the hint line. */
	void SetTitle(const FString& Text);
	void SetHint(const FString& Text);

	/**
	 * Adds the options page (FShooterOptionsPage: its lines bound to the player's options, and Back); Back from it
	 * saves them and shows page 0.
	 */
	void BuildOptionsPage();
	/** Shows the options page with the options' values. */
	void ShowOptionsPage();
	/** Leaves the options page: saves them and shows page 0. */
	void LeaveOptionsPage();

	/** The width of a line's box (the panel's inside). */
	static constexpr float LineWidth = 380.0f;

	/** The options page's lines, and its index (INDEX_NONE without one). */
	FShooterOptionsPage OptionsPage;
	int32 OptionsPageIndex = INDEX_NONE;

	UPROPERTY()
	UCanvasPanel* Root = nullptr;
	UPROPERTY()
	UBorder* Panel = nullptr;
	UPROPERTY()
	UTextBlock* Title = nullptr;
	UPROPERTY()
	UWidgetSwitcher* Pages = nullptr;
	UPROPERTY()
	UTextBlock* Hint = nullptr;
	/** A full-screen shade behind the panel (the pause menu dims the game); hidden by default. */
	UPROPERTY()
	UBorder* Shade = nullptr;

private:
	UPROPERTY()
	UCanvasPanelSlot* PanelSlot = nullptr;
	UPROPERTY()
	UCanvasPanelSlot* ShadeSlot = nullptr;
};
