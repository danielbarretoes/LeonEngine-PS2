#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"
#include "ProgressBar.generated.h"

/** UE-like UProgressBar (lite): background + fill rect, optional percent label. */
UCLASS()
class UMG_API UProgressBar : public UUserWidget
{
	GENERATED_BODY()

public:
	UProgressBar(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

	/** Normalized fill amount in [0, 1]. */
	void SetPercent(float InPercent)
	{
		Percent = FMath::Clamp(InPercent, 0.0f, 1.0f);
	}
	[[nodiscard]] float GetPercent() const
	{
		return Percent;
	}

	void SetShowPercentText(bool bShow)
	{
		bShowPercentText = bShow;
	}
	void SetBackgroundColor(const FLinearColor& Color)
	{
		BackgroundColor = Color;
	}
	void SetFillColor(const FLinearColor& Color)
	{
		FillColor = Color;
	}
	void SetBorderColor(const FLinearColor& Color)
	{
		BorderColor = Color;
	}
	void SetTextColor(const FLinearColor& Color)
	{
		TextColor = Color;
	}

	/** Places the bar horizontally centered near the bottom of the viewport each paint. */
	void SetAnchoredBottomCenter(bool bEnabled)
	{
		bAnchoredBottomCenter = bEnabled;
	}

	void NativePaint(FPaintContext& Ctx) override;

private:
	float X = 0.0f;
	float Y = 0.0f;
	float W = 280.0f;
	float H = 18.0f;
	float Percent = 0.0f;
	bool bShowPercentText = false;
	bool bAnchoredBottomCenter = false;

	FLinearColor BackgroundColor = FLinearColor(0.10f, 0.10f, 0.12f);
	FLinearColor FillColor = FLinearColor(0.85f, 0.65f, 0.20f);
	FLinearColor BorderColor = FLinearColor(0.35f, 0.30f, 0.18f);
	FLinearColor TextColor = FLinearColor(1.0f, 0.92f, 0.75f);
};
