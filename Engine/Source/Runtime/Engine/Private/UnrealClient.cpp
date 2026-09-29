#include "UnrealClient.h"

#include "CanvasTypes.h"
#include "Engine/GameViewportClient.h"
#include "EngineStats.h"
#include "GenericPlatform/GenericWindow.h"
#include "RendererInterface.h"

DEFINE_STAT(STAT_ViewportDraw);
DEFINE_STAT(STAT_CanvasFlush);
DEFINE_STAT(STAT_ViewportPresent);

namespace
{
	/** The pending screenshot's file, empty when none is requested. */
	FString& GetScreenshotFilename()
	{
		static FString Filename;
		return Filename;
	}

} // namespace

FViewport::FViewport(UGameViewportClient* InViewportClient, FGenericWindow* InWindow)
	: ViewportClient(InViewportClient)
	, Window(InWindow)
{
}

FIntPoint FViewport::GetWindowSize() const
{
	int32 Width = 0;
	int32 Height = 0;
	if (Window != nullptr)
	{
		Window->GetFramebufferSize(Width, Height);
	}
	return FIntPoint(Width, Height);
}

FIntPoint FViewport::GetSizeXY() const
{
	const FIntPoint WindowSize = GetWindowSize();
	const IRendererModule* RendererModule = GetRendererModulePtr();
	return RendererModule != nullptr && WindowSize.X > 0 && WindowSize.Y > 0
		? RendererModule->GetRenderTargetSize(WindowSize)
		: WindowSize;
}

void FViewport::Draw(bool bShouldPresent)
{
	if (Window == nullptr)
	{
		return;
	}
	const FIntPoint Size = GetSizeXY();
	if (Size.X > 0 && Size.Y > 0 && ViewportClient != nullptr)
	{
		FCanvas Canvas(Size.X, Size.Y);
		{
			SCOPE_CYCLE_COUNTER(STAT_ViewportDraw);
			ViewportClient->Draw(this, &Canvas);
		}
		SCOPE_CYCLE_COUNTER(STAT_CanvasFlush);
		Canvas.Flush_GameThread();
		(void)ViewportClient->ProcessScreenShots(this);
	}
	if (bShouldPresent)
	{
		SCOPE_CYCLE_COUNTER(STAT_ViewportPresent);
		if (IRendererModule* RendererModule = GetRendererModulePtr())
		{
			RendererModule->EndDrawingViewport(GetWindowSize());
		}
		Window->SwapBuffers();
	}
}

void FScreenshotRequest::RequestScreenshot(const FString& InFilename, bool /*bInShowUI*/, bool /*bAddFilenameSuffix*/)
{
	GetScreenshotFilename() = InFilename;
}

bool FScreenshotRequest::IsScreenshotRequested()
{
	return !GetScreenshotFilename().IsEmpty();
}

const FString& FScreenshotRequest::GetFilename()
{
	return GetScreenshotFilename();
}

void FScreenshotRequest::Reset()
{
	GetScreenshotFilename().Empty();
}
