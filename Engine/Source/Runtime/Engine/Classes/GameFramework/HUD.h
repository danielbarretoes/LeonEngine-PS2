#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HUD.generated.h"

class FDebugOverlay;

/**
 * Unreal-like AHUD (an actor): owns UserWidgets painted each frame into screen geometry. UE spawns one per player
 * controller; until P13 the engine owns a single HUD outside any world (UGameEngine::GetHUD) and ticks and paints it.
 */
UCLASS()
class ENGINE_API AHUD : public AActor
{
	GENERATED_BODY()

public:
	AHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void Clear();

	/**
	 * Unreal CreateWidget + AddToViewport (lite): NewObject with the HUD as outer, NativeConstruct, retain (the HUD's
	 * Widgets keep it alive).
	 */
	template <typename T>
	T* AddWidget()
	{
		static_assert(TIsDerivedFrom<T, UUserWidget>::Value, "T must derive from UserWidget");
		T* Widget = NewObject<T>(this);
		Widget->OwningHud = this;
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

	/** Clears prior frame screen geometry, then paints visible widgets. */
	void Paint(FDebugOverlay& Overlay, int FramebufferWidth, int FramebufferHeight);

	[[nodiscard]] const TArray<UUserWidget*>& GetWidgets() const
	{
		return Widgets;
	}

private:
	/** The added widgets, in paint order. */
	UPROPERTY(Transient)
	TArray<UUserWidget*> Widgets;
};
