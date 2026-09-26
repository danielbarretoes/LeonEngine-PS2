#pragma once

#include "Blueprint/PaintContext.h"
#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "UserWidget.generated.h"

class AHUD;
class UWidgetTree;

/**
 * A widget made of a tree of widgets (UE: UUserWidget). AHUD::AddWidget makes it with the HUD as its outer and calls
 * Initialize, which makes its WidgetTree and calls NativeOnInitialized (build the tree there), then NativeConstruct
 * (UE: CreateWidget + AddToViewport). The HUD ticks the visible ones (NativeTick: refresh the tree from the game) and
 * paints them (NativePaint: the tree over the whole viewport).
 */
UCLASS()
class UMG_API UUserWidget : public UWidget
{
	GENERATED_BODY()

public:
	UUserWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The widget's widgets (UE: WidgetTree). */
	UPROPERTY(Transient)
	UWidgetTree* WidgetTree = nullptr;

	/** Makes the tree once, then NativeOnInitialized; false when already initialized (UE: Initialize). */
	bool Initialize();

	virtual void NativeOnInitialized()
	{
	}
	virtual void NativeConstruct()
	{
	}
	virtual void NativeTick(float /*DeltaTime*/)
	{
	}
	/** Paints the tree's root over the viewport. */
	virtual void NativePaint(FPaintContext& Ctx);
	virtual void NativeDestruct()
	{
	}

	/** The HUD that added the widget; null if not added (Leon; UE's user widgets know their player instead). */
	[[nodiscard]] AHUD* GetOwningHUD() const
	{
		return OwningHud;
	}

protected:
	/** The root's. */
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	friend class AHUD;
	/** The HUD that added the widget (not a UPROPERTY: UMG cannot reflect Engine's AHUD; the HUD outlives it). */
	AHUD* OwningHud = nullptr;
};
