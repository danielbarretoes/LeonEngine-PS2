#include "GameFramework/HUD.h"

#include "Blueprint/PaintContext.h"
#include "CanvasTypes.h"
#include "GameFramework/PlayerController.h"

AHUD::AHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidden = true;
	// The widgets tick with the HUD.
	PrimaryActorTick.bCanEverTick = true;
}

void AHUD::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	PlayerOwner = Cast<APlayerController>(GetOwner());
}

void AHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clear();
	Super::EndPlay(EndPlayReason);
}

void AHUD::Clear()
{
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget != nullptr)
		{
			Widget->NativeDestruct();
			Widget->OwningHud = nullptr;
		}
	}
	Widgets.Empty();
}

bool AHUD::RemoveWidget(UUserWidget* Widget)
{
	if (Widget == nullptr)
	{
		return false;
	}
	for (int32 Index = 0; Index < Widgets.Num(); ++Index)
	{
		if (Widgets[Index] == Widget)
		{
			Widget->NativeDestruct();
			Widget->OwningHud = nullptr;
			Widgets.RemoveAt(Index);
			return true;
		}
	}
	return false;
}

void AHUD::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget != nullptr && Widget->IsVisible())
		{
			Widget->NativeTick(DeltaTime);
		}
	}
}

bool AHUD::InputKey(const FKey& Key, EInputEvent EventType)
{
	if (EventType != IE_Pressed && EventType != IE_Released && EventType != IE_Repeat)
	{
		return false;
	}
	const bool bPressed = EventType != IE_Released;
	// The top widget first: the last added paints over the others.
	for (int32 Index = Widgets.Num() - 1; Index >= 0; --Index)
	{
		UUserWidget* Widget = Widgets[Index];
		if (Widget == nullptr || !Widget->IsVisible())
		{
			continue;
		}
		FReply Reply = FReply::Unhandled();
		if (Key.IsMouseButton())
		{
			const FPointerEvent MouseEvent(MousePosition, Key);
			Reply = bPressed ? Widget->ProcessMouseButtonDownEvent(MouseEvent)
							 : Widget->ProcessMouseButtonUpEvent(MouseEvent);
		}
		else
		{
			const FKeyEvent KeyEvent(Key, EventType == IE_Repeat);
			Reply = bPressed ? Widget->ProcessKeyDownEvent(KeyEvent) : Widget->ProcessKeyUpEvent(KeyEvent);
		}
		if (Reply.IsEventHandled())
		{
			return true;
		}
	}
	return false;
}

void AHUD::InputMouseMove(const FVector2D& CanvasPosition)
{
	MousePosition = CanvasPosition;
	const FPointerEvent MouseEvent(CanvasPosition, FKey());
	bool bTaken = false;
	for (int32 Index = Widgets.Num() - 1; Index >= 0; --Index)
	{
		UUserWidget* Widget = Widgets[Index];
		if (Widget == nullptr || !Widget->IsVisible())
		{
			continue;
		}
		// Only the top widget under the mouse hovers; the ones under it lose their hover.
		if (bTaken)
		{
			(void)Widget->ProcessMouseMoveEvent(FPointerEvent(FVector2D(-1.0f, -1.0f), FKey()));
			continue;
		}
		bTaken = Widget->ProcessMouseMoveEvent(MouseEvent).IsEventHandled();
	}
}

void AHUD::Paint(FCanvas& InCanvas)
{
	if (InCanvas.GetSizeX() <= 0 || InCanvas.GetSizeY() <= 0)
	{
		return;
	}
	Canvas = &InCanvas;
	DrawHUD();
	Canvas = nullptr;
	if (Widgets.Num() == 0)
	{
		return;
	}

	FPaintContext Ctx(InCanvas);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget != nullptr && Widget->IsVisible())
		{
			Widget->NativePaint(Ctx);
		}
	}
}
