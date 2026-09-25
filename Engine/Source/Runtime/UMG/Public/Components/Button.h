#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"
#include "Button.generated.h"

class FGenericWindow;

/**
 * UE-like UButton (lite): filled rect + label; hover / selected / press.
 * Usually owned by UVerticalBox; can also be a root HUD widget with SetPosition. The id is what TickInput reports
 * when the button is activated (NAME_None: a status row that cannot be activated).
 */
UCLASS()
class UMG_API UButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UButton(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetId(FName InId)
	{
		Id = InId;
	}
	[[nodiscard]] FName GetId() const
	{
		return Id;
	}

	void SetLabel(const FText& InLabel)
	{
		Label = InLabel;
	}
	[[nodiscard]] const FText& GetLabel() const
	{
		return Label;
	}

	void SetPosition(float InX, float InY)
	{
		X = InX;
		Y = InY;
	}
	void SetSize(float InW, float InH)
	{
		W = InW;
		H = InH;
	}

	[[nodiscard]] float GetX() const
	{
		return X;
	}
	[[nodiscard]] float GetY() const
	{
		return Y;
	}
	[[nodiscard]] float GetWidth() const
	{
		return W;
	}
	[[nodiscard]] float GetHeight() const
	{
		return H;
	}

	void SetSelected(bool bInSelected)
	{
		bSelected = bInSelected;
	}
	[[nodiscard]] bool IsSelected() const
	{
		return bSelected;
	}

	void SetEnabled(bool bInEnabled)
	{
		bEnabled = bInEnabled;
	}
	[[nodiscard]] bool IsEnabled() const
	{
		return bEnabled;
	}

	void SetTextColor(const FLinearColor& Color)
	{
		TextColor = Color;
	}
	void SetBackgroundColor(const FLinearColor& Color)
	{
		BackgroundColor = Color;
	}
	void SetSelectedBackgroundColor(const FLinearColor& Color)
	{
		SelectedBackgroundColor = Color;
	}

	/** Preferred size for the current label (padding included). */
	void MeasureDesiredSize(float& OutW, float& OutH) const;

	/** Hit test in framebuffer pixels (top-left origin). */
	[[nodiscard]] bool Contains(float FbX, float FbY) const;

	void NativePaint(FPaintContext& Ctx) override;

private:
	FName Id;
	FText Label = FText::FromString("Button");
	float X = 0.0f;
	float Y = 0.0f;
	float W = 160.0f;
	float H = HudLineHeight + 16.0f;
	bool bSelected = false;
	bool bEnabled = true;
	bool bHovered = false;

	FLinearColor TextColor = FLinearColor(1.0f, 0.92f, 0.75f);
	FLinearColor BackgroundColor = FLinearColor(0.12f, 0.12f, 0.14f);
	FLinearColor SelectedBackgroundColor = FLinearColor(0.28f, 0.22f, 0.10f);
	FLinearColor HoverBackgroundColor = FLinearColor(0.18f, 0.16f, 0.12f);
	FLinearColor DisabledTextColor = FLinearColor(0.45f, 0.45f, 0.45f);

	friend class UVerticalBox;
	void SetHovered(bool bInHovered)
	{
		bHovered = bInHovered;
	}
};
