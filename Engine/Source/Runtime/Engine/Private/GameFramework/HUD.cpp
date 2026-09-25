#include "GameFramework/HUD.h"

#include "Blueprint/PaintContext.h"
#include "CanvasTypes.h"

AHUD::AHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidden = true;
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
		if (Widget != nullptr && Widget->bIsVisible)
		{
			Widget->NativeTick(DeltaTime);
		}
	}
}

void AHUD::Paint(FCanvas& Canvas)
{
	if (Canvas.GetSizeX() <= 0 || Canvas.GetSizeY() <= 0 || Widgets.Num() == 0)
	{
		return;
	}

	FPaintContext Ctx(Canvas);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget != nullptr && Widget->bIsVisible)
		{
			Widget->NativePaint(Ctx);
		}
	}
}
