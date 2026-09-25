#include "GameFramework/HUD.h"

#include "Blueprint/PaintContext.h"
#include "Debug/DebugOverlay.h"

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

void AHUD::BeginDestroy()
{
	Clear();
	Super::BeginDestroy();
}

void AHUD::Clear()
{
	for (const TUniquePtr<UUserWidget>& Widget : Widgets)
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
		if (Widgets[Index].Get() == Widget)
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
	for (const TUniquePtr<UUserWidget>& Widget : Widgets)
	{
		if (Widget != nullptr && Widget->bIsVisible)
		{
			Widget->NativeTick(DeltaTime);
		}
	}
}

void AHUD::Paint(FDebugOverlay& Overlay, int FramebufferWidth, int FramebufferHeight)
{
	Overlay.ClearScreenGeometry();
	if (FramebufferWidth <= 0 || FramebufferHeight <= 0 || Widgets.Num() == 0)
	{
		return;
	}

	FPaintContext Ctx(Overlay, FramebufferWidth, FramebufferHeight);
	for (const TUniquePtr<UUserWidget>& Widget : Widgets)
	{
		if (Widget != nullptr && Widget->bIsVisible)
		{
			Widget->NativePaint(Ctx);
		}
	}
}
