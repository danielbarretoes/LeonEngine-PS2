#pragma once

#include "Blueprint/PaintContext.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UserWidget.generated.h"

class AHUD;

/**
 * UE-like UUserWidget: game HUD elements override NativePaint / NativeTick. Created by AHUD::AddWidget with the HUD as
 * its outer (UE: CreateWidget + AddToViewport); the HUD keeps it alive.
 */
UCLASS(Abstract)
class UMG_API UUserWidget : public UObject
{
	GENERATED_BODY()

public:
	UUserWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY()
	bool bIsVisible = true;

	virtual void NativeConstruct()
	{
	}
	virtual void NativeTick(float /*DeltaTime*/)
	{
	}
	virtual void NativePaint(FPaintContext& /*Ctx*/)
	{
	}
	virtual void NativeDestruct()
	{
	}

	void SetVisibility(bool bVisible)
	{
		bIsVisible = bVisible;
	}
	[[nodiscard]] bool IsVisible() const
	{
		return bIsVisible;
	}

	/** Owning AHUD (set by AHUD::AddWidget); null if not added. */
	[[nodiscard]] AHUD* GetOwningHUD() const
	{
		return OwningHud;
	}

private:
	friend class AHUD;

	/** The HUD that added the widget (not a UPROPERTY: UMG cannot reflect Engine's AHUD; the HUD outlives it). */
	AHUD* OwningHud = nullptr;
};
