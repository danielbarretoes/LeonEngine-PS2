#include "GameFramework/HUD.h"

#include "Blueprint/PaintContext.h"
#include "CanvasTypes.h"
#include "GameFramework/PlayerController.h"

AHUD::AHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidden = true;
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
