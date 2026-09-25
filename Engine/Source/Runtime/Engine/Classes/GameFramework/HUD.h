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

	/** Unreal CreateWidget + AddToViewport (lite): construct, NativeConstruct, retain. */
	template <typename T, typename... ArgsType>
	T* AddWidget(ArgsType&&... Args)
	{
		static_assert(TIsDerivedFrom<T, UUserWidget>::Value, "T must derive from UserWidget");
		auto Owned = MakeUnique<T>(Forward<ArgsType>(Args)...);
		T* Raw = Owned.Get();
		Raw->OwningHud = this;
		Raw->NativeConstruct();
		Widgets.Add(MoveTemp(Owned));
		return Raw;
	}

	/** Remove first widget of type T (NativeDestruct). Returns true if removed. */
	template <typename T>
	bool RemoveWidget()
	{
		static_assert(TIsDerivedFrom<T, UUserWidget>::Value, "T must derive from UserWidget");
		for (int32 Index = 0; Index < Widgets.Num(); ++Index)
		{
			if (dynamic_cast<T*>(Widgets[Index].Get()) != nullptr)
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
		for (const auto& W : Widgets)
		{
			if (T* Typed = dynamic_cast<T*>(W.Get()))
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
	void BeginDestroy() override;

	/** Clears prior frame screen geometry, then paints visible widgets. */
	void Paint(FDebugOverlay& Overlay, int FramebufferWidth, int FramebufferHeight);

	[[nodiscard]] const TArray<TUniquePtr<UUserWidget>>& GetWidgets() const
	{
		return Widgets;
	}

private:
	TArray<TUniquePtr<UUserWidget>> Widgets;
};
