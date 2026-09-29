#pragma once

#include "Stats/Stats.h"

// The engine frame's parts as cycle stats (UE: EngineStats.h), the scopes of UGameEngine::Tick, FViewport::Draw and
// UGameViewportClient::Draw. UGameEngine's `-LogFrameTimes` reads them for its `Frame split` and `FrameStats Summary:`
// (Docs/PLANS/ps2-shipping.md N9).

// The frame's top level, in UGameEngine::Tick's order.
DECLARE_CYCLE_STAT_EXTERN(TEXT("Input"), STAT_EngineInput, STATGROUP_Engine, ENGINE_API);
/** ProcessAsyncLoading: the packages LoadPackageAsync read, serialized (Docs/PLANS/ps2-shipping.md N24). */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Async Loading"), STAT_AsyncLoading, STATGROUP_Engine, ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Audio"), STAT_AudioTick, STATGROUP_Engine, ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("World Tick"), STAT_WorldTick, STATGROUP_Engine, ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Garbage Collection"), STAT_GarbageCollection, STATGROUP_Engine, ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Viewport Tick"), STAT_GameViewportTick, STATGROUP_Engine, ENGINE_API);
/** FViewport::Draw's parts: the viewport client's draw (the scene and the HUD into the canvas), the canvas's flush. */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Viewport Draw"), STAT_ViewportDraw, STATGROUP_Engine, ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Canvas Flush"), STAT_CanvasFlush, STATGROUP_Engine, ENGINE_API);
/** The renderer's end of the frame and the window's swap (on the PS2 the GIF packet, its DMA and the vblank wait). */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Present"), STAT_ViewportPresent, STATGROUP_Engine, ENGINE_API);

// UGameViewportClient::Draw's parts.
DECLARE_CYCLE_STAT_EXTERN(TEXT("End of Frame Updates"), STAT_EndOfFrameUpdates, STATGROUP_Engine, ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Scene"), STAT_SceneRendering, STATGROUP_Engine, ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("HUD"), STAT_HUD, STATGROUP_Engine, ENGINE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Debug Overlay"), STAT_DebugOverlay, STATGROUP_Engine, ENGINE_API);
