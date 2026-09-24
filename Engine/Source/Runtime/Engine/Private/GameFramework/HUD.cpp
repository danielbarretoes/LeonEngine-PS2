#include "GameFramework/HUD.h"

#include "Blueprint/PaintContext.h"
#include "Debug/DebugOverlay.h"

void AHUD::Clear()
{
	for (const std::unique_ptr<UUserWidget>& Widget : Widgets)
	{
		if (Widget != nullptr)
		{
			Widget->NativeDestruct();
			Widget->OwningHud = nullptr;
		}
	}
	Widgets.clear();
}

bool AHUD::RemoveWidget(UUserWidget* Widget)
{
	if (Widget == nullptr)
	{
		return false;
	}
	for (auto It = Widgets.begin(); It != Widgets.end(); ++It)
	{
		if (It->get() == Widget)
		{
			(*It)->NativeDestruct();
			(*It)->OwningHud = nullptr;
			Widgets.erase(It);
			return true;
		}
	}
	return false;
}

void AHUD::Tick(float DeltaTime)
{
	for (const std::unique_ptr<UUserWidget>& Widget : Widgets)
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
	if (FramebufferWidth <= 0 || FramebufferHeight <= 0 || Widgets.empty())
	{
		return;
	}

	FPaintContext Ctx(Overlay, FramebufferWidth, FramebufferHeight);
	for (const std::unique_ptr<UUserWidget>& Widget : Widgets)
	{
		if (Widget != nullptr && Widget->bIsVisible)
		{
			Widget->NativePaint(Ctx);
		}
	}
}
