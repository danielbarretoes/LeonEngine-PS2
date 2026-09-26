#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUD.generated.h"

class APlayerController;
class FCanvas;

/**
 * Unreal-like AHUD (an actor): owns UserWidgets painted each frame into screen geometry. Each player controller gets
 * one from the game mode (AGameModeBase::HUDClass, APlayerController::ClientSetHUD); it ticks in the world and the
 * viewport paints it after the scene.
 */
UCLASS()
class ENGINE_API AHUD : public AActor
{
	GENERATED_BODY()

public:
	AHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void Clear();

	/** The player controller that owns this HUD (UE: PlayerOwner), set from the owner when it spawns. */
	UPROPERTY(Transient)
	APlayerController* PlayerOwner = nullptr;

	/** Takes its owner as PlayerOwner (UE). */
	void PostInitializeComponents() override;

	/**
	 * Unreal CreateWidget + AddToViewport (lite): NewObject with the HUD as outer, Initialize (its widget tree),
	 * NativeConstruct, retain (the HUD's Widgets keep it alive).
	 */
	template <typename T>
	T* AddWidget()
	{
		static_assert(TIsDerivedFrom<T, UUserWidget>::Value, "T must derive from UserWidget");
		T* Widget = NewObject<T>(this);
		Widget->OwningHud = this;
		(void)Widget->Initialize();
		Widget->NativeConstruct();
		Widgets.Add(Widget);
		return Widget;
	}

	/** Remove first widget of type T (NativeDestruct). Returns true if removed. */
	template <typename T>
	bool RemoveWidget()
	{
		static_assert(TIsDerivedFrom<T, UUserWidget>::Value, "T must derive from UserWidget");
		for (int32 Index = 0; Index < Widgets.Num(); ++Index)
		{
			if (Cast<T>(Widgets[Index]) != nullptr)
			{
				Widgets[Index]->NativeDestruct();
				Widgets[Index]->OwningHud = nullptr;
				Widgets.RemoveAt(Index);
				return true;
			}
		}
		return false;
	}

	bool RemoveWidget(UUserWidget* Widget);

	template <typename T>
	[[nodiscard]] T* GetWidgetOfClass() const
	{
		static_assert(TIsDerivedFrom<T, UUserWidget>::Value, "T must derive from UserWidget");
		for (UUserWidget* W : Widgets)
		{
			if (T* Typed = Cast<T>(W))
			{
				return Typed;
			}
		}
		return nullptr;
	}

	/** Ticks the visible widgets. */
	void Tick(float DeltaTime) override;

	/** Removes the widgets when the HUD is destroyed or ends play. */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Paints the HUD into the frame's canvas: DrawHUD with Canvas set, then the visible widgets (UMG widgets paint
	 * through it).
	 */
	void Paint(FCanvas& InCanvas);

	/**
	 * Draws the HUD's own items into Canvas, before the widgets (UE: DrawHUD); a game's HUD draws its crosshair here.
	 * Nothing by default.
	 */
	virtual void DrawHUD()
	{
	}

	/** The canvas of the frame being drawn, set during Paint only (UE: Canvas, a UCanvas there). */
	FCanvas* Canvas = nullptr;

	[[nodiscard]] const TArray<UUserWidget*>& GetWidgets() const
	{
		return Widgets;
	}

private:
	/** The added widgets, in paint order. */
	UPROPERTY(Transient)
	TArray<UUserWidget*> Widgets;
};
