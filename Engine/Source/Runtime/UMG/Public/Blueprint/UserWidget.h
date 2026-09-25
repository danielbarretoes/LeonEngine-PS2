#pragma once

#include "Blueprint/PaintContext.h"
#include "CoreMinimal.h"

class AHUD;

/** UE-like UUserWidget: game HUD elements override NativePaint / NativeTick. */
class UMG_API UUserWidget
{
public:
	virtual ~UUserWidget() = default;

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
	AHUD* OwningHud = nullptr;
};
