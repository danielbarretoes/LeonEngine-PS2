#include "Engine/GameEngine.h"

#include "Camera/CameraComponent.h"
#include "CoreGlobals.h"
#include "Engine/BlockingVolume.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineLogs.h"
#include "EngineStats.h"
#include "GameMapsSettings.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformTime.h"
#include "HAL/UnrealMemory.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/MemStack.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RendererInterface.h"
#include "UObject/GarbageCollection.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"

DEFINE_STAT(STAT_EngineInput);
DEFINE_STAT(STAT_AsyncLoading);
DEFINE_STAT(STAT_AudioTick);
DEFINE_STAT(STAT_WorldTick);
DEFINE_STAT(STAT_GarbageCollection);
DEFINE_STAT(STAT_GameViewportTick);

UGameEngine::UGameEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGameEngine::BeginDestroy()
{
	if (bIsInitialized)
	{
		PreExit();
	}
	Super::BeginDestroy();
}

void UGameEngine::Init(IEngineLoop* InEngineLoop)
{
	Super::Init(InEngineLoop);

	FGenericWindow* Window = !bHeadless ? InEngineLoop->GetMainWindow() : nullptr;
	if (Window != nullptr)
	{
		// The renderer's GPU objects need the window's context (UE: the renderer comes up with the viewport). The
		// shaders are the engine's (UE: the /Engine/Shaders virtual folder, Engine/Shaders on disk).
		const FString ShaderDir = FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders"));
		IRendererModule* RendererModule = GetRendererModulePtr();
		if (RendererModule == nullptr || !RendererModule->InitRenderer(ShaderDir))
		{
			UE_LOG(LogEngine, Error, "Failed to start the renderer");
			bIsInitialized = false;
			return;
		}
		// Stats off unless the config or -showstats asks for them.
		Overlay.SetRightText(FString());
		Overlay.SetBottomLeftText(FString());
		SetHudStatsVisible(bShowStatsByDefault || FParse::Param(FCommandLine::Get(), "showstats"));
		UE_LOG(LogEngine, Log, "HUD stats: %s", IsHudStatsVisible() ? "on" : "off");
	}
	else
	{
		UE_LOG(LogEngine, Log, "Leon Engine headless (no OpenGL / window)");
	}
	// The frame's times and its cycle stats, logged every few seconds and summed up at exit.
	bLogFrameTimes = FParse::Param(FCommandLine::Get(), "LogFrameTimes");
	if (bLogFrameTimes)
	{
		FThreadStats::MasterEnableAdd();
	}

	// The game instance of the project's class and its world context (UE).
	UClass* GameInstanceClass = GetDefault<UGameMapsSettings>()->GameInstanceClass.IsValid()
		? GetDefault<UGameMapsSettings>()->GameInstanceClass.TryLoadClass<UGameInstance>()
		: nullptr;
	if (GameInstanceClass == nullptr)
	{
		GameInstanceClass = UGameInstance::StaticClass();
	}
	GameInstance = NewObject<UGameInstance>(this, GameInstanceClass);
	GameInstance->InitializeStandalone();

	// The game viewport client shows the context's world in the window, and makes the first local player (UE).
	UClass* ViewportClientClass = GameViewportClientClassName.IsValid()
		? GameViewportClientClassName.TryLoadClass<UGameViewportClient>()
		: nullptr;
	if (ViewportClientClass == nullptr)
	{
		ViewportClientClass = UGameViewportClient::StaticClass();
	}
	GameViewport = NewObject<UGameViewportClient>(this, ViewportClientClass);
	GameViewport->Init(*GameInstance->GetWorldContext(), GameInstance);
	// No one is at the controls of an unattended run (a scripted capture): the OS input must not move the view.
	GameViewport->SetIgnoreInput(FApp::IsUnattended());
	if (Window != nullptr)
	{
		GameViewport->SetViewportWindow(Window);
		// The application's gamepad (the PS2's DualShock; the desktop has none), read with the window (UE: Slate's
		// controller events).
		GenericApplication* Application = InEngineLoop->GetApplication();
		GameViewport->SetInputInterface(Application != nullptr ? Application->GetInputInterface() : nullptr);
		if (FParse::Param(FCommandLine::Get(), "AxesGizmo"))
		{
			GameViewport->EngineShowFlags.AxesGizmo = true;
		}
	}
	FString Error;
	if (GameViewport->SetupInitialLocalPlayer(Error) == nullptr)
	{
		UE_LOG(LogEngine, Error, "Could not create the local player: %s", *Error);
	}
	GameInstance->Init();
}

void UGameEngine::Start()
{
	GameInstance->StartGameInstance();
	if (bHeadless || IsEngineExitRequested())
	{
		return;
	}
	int32 NumStaticMeshes = 0;
	if (UWorld* World = GetGameWorld(); World != nullptr && World->PersistentLevel != nullptr)
	{
		for (const AActor* Actor : World->PersistentLevel->Actors)
		{
			if (Actor != nullptr && (Actor->IsA<AStaticMeshActor>() || Actor->IsA<ABlockingVolume>()))
			{
				++NumStaticMeshes;
			}
		}
	}
	UE_LOG(LogEngine, Log, "Level static meshes: %d", NumStaticMeshes);
	UE_LOG(LogEngine, Log, "Controls: mouse look (cursor captured); close window to quit");
	UE_LOG(LogEngine, Log, "Default pawn: mouse look, WASD fly along view, Q/E down/up (BaseInput.ini)");
	UE_LOG(LogEngine, Log, "Debug: F1 show Bounds (mesh AABBs + light frustum); F2 show Collision");
	UE_LOG(LogEngine, Log, "Debug: F3 show Navigation");
	UE_LOG(LogEngine, Log, "Stats: F4 stat unit (FPS / RAM / TRI overlay, off by default)");
	UE_LOG(LogEngine, Log, "Stats: F7 stat cycles (the frame's cycle stats, off by default)");
	UE_LOG(LogEngine, Log, "Stats: F8 stat memory (the heap, the frame's stack and the memory tags, off by default)");
	UE_LOG(LogEngine, Log, "Shaders: F5 RecompileShaders (also auto-reloads when files change)");
	UE_LOG(LogEngine, Log, "Debug: F6 show AxesGizmo (X red, Y green, Z blue; world origin + view corner)");
}

void UGameEngine::PreExit()
{
	if (!bIsInitialized)
	{
		return;
	}
	if (bLogFrameTimes)
	{
		if (RunStats.Frames > 0)
		{
			LogRunSummary();
		}
		FThreadStats::MasterEnableSubtract();
		bLogFrameTimes = false;
	}
	if (GameInstance != nullptr)
	{
		GameInstance->Shutdown();
	}
	AudioDevice.Shutdown();
	// The world goes first: its actors end play while the resources they use still exist.
	DestroyGameWorld();
	FGenericWindow* Window = GameViewport != nullptr ? GameViewport->GetWindow() : nullptr;
	if (Window != nullptr)
	{
		Overlay.Clear();
		// The GPU copies of the assets and the renderer's objects go while the context exists.
		GetRendererModule().ShutdownRenderer();
		Window->SetCursorCaptured(false);
		GameViewport->SetViewportWindow(nullptr);
	}
	Super::PreExit();
}

void UGameEngine::DestroyGameWorld()
{
	if (GameInstance != nullptr && GameInstance->GetWorld() != nullptr)
	{
		GameInstance->DestroyWorldContextWorld();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
}

UWorld* UGameEngine::GetGameWorld() const
{
	return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
}

TArray<FWorldContext*> UGameEngine::GetWorldContexts()
{
	TArray<FWorldContext*> Contexts;
	if (GameInstance != nullptr)
	{
		Contexts.Add(GameInstance->GetWorldContext());
	}
	return Contexts;
}

EShaderReloadResult UGameEngine::ReloadAllShaders(bool bForce)
{
	IRendererModule* RendererModule = GetRendererModulePtr();
	return RendererModule != nullptr ? RendererModule->ReloadShaders(bForce) : EShaderReloadResult::Unchanged;
}

void UGameEngine::Tick(float DeltaSeconds, bool /*bIdleMode*/)
{
	if (!bIsInitialized)
	{
		return;
	}
	FGenericWindow* Window = GameViewport != nullptr ? GameViewport->GetWindow() : nullptr;
	if (Window != nullptr && Window->ShouldClose())
	{
		RequestEngineExit("Main window closed");
		return;
	}
	// The frame's clock only with -LogFrameTimes (on the PS2 a Cycles64 read is a kernel call).
	const uint64 TickStart = bLogFrameTimes ? FPlatformTime::Cycles64() : 0;
	const uint64 FrameCycles = LastTickStartCycles > 0 ? TickStart - LastTickStartCycles : 0;
	const uint64 LoopCycles = LastTickEndCycles > 0 ? TickStart - LastTickEndCycles : 0;
	LastTickStartCycles = TickStart;
	const uint64 TickStartAllocations = bLogFrameTimes ? FMemory::GetUsage().TotalAllocations : 0;
	const uint64 FrameAllocations = LastTickStartAllocations > 0 ? TickStartAllocations - LastTickStartAllocations : 0;
	LastTickStartAllocations = TickStartAllocations;
	if (bLogFrameTimes)
	{
		LogFrameSpike(FrameCycles);
	}
	FThreadStats::AdvanceFrame();
	{
		// The asynchronous loads first (UE: StaticTick's ProcessAsyncLoading): the packages whose bytes came in are
		// serialized before the world steps, so what a step spawns finds them (Docs/PLANS/ps2-shipping.md N24).
		SCOPE_CYCLE_COUNTER(STAT_AsyncLoading);
		(void)ProcessAsyncLoading(AsyncLoadingTimeLimit > 0.0f, false, AsyncLoadingTimeLimit / 1000.0f);
	}
	// The fixed steps this frame holds (ps2-shipping D4; UpdateTimeAndHandleMaxTickRate).
	const int32 NumSteps = ConsumeFrameSteps(DeltaSeconds);
	const float StepSeconds = FixedStepClock.GetStepSeconds();
	if (Window != nullptr)
	{
		SCOPE_CYCLE_COUNTER(STAT_EngineInput);
		(void)ReloadAllShaders(false);
		// The window's keys and mouse reach the player's controller before the world steps (UE: Slate's events); a
		// frame without a step leaves them to the next one, so a key or a mouse move is not counted twice.
		if (NumSteps > 0)
		{
			GameViewport->ProcessInput(DeltaSeconds);
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_WorldTick);
		FWorldContext& Context = *GameInstance->GetWorldContext();
		TickWorldTravel(Context, DeltaSeconds);
		for (int32 Step = 0; Step < NumSteps; ++Step)
		{
			UWorld* World = Context.World();
			if (World == nullptr)
			{
				break;
			}
			// The player controllers process their input as they tick, the characters move after their controllers,
			// then the physics steps, the timers run and the world updates the cameras (UWorld::TickGameplayFrame).
			FWorldGameplayFrameParams Frame;
			Frame.DeltaTime = StepSeconds;
			// The viewport's show flags draw into the world's line batch, which the scene renderer flushes (UE);
			// without a window nothing would empty it. Only the frame's last step draws.
			if (Window != nullptr && GameViewport != nullptr && Step == NumSteps - 1)
			{
				const FEngineShowFlags& ShowFlags = GameViewport->EngineShowFlags;
				Frame.CollisionDebugDraw = ShowFlags.Collision ? &World->LineBatcher : nullptr;
				Frame.NavigationDebugDraw = ShowFlags.Navigation ? &World->LineBatcher : nullptr;
			}
			World->TickGameplayFrame(Frame);
			{
				// After each step, a safe point (D11): the garbage collector's work follows the steps, not the frames,
				// so a run collects the same objects at the same step whatever its frame rate.
				SCOPE_CYCLE_COUNTER(STAT_GarbageCollection);
				(void)ConditionalCollectGarbage(StepSeconds);
			}
		}
	}
	if (Window != nullptr)
	{
		// After the world, as UE updates its audio device: the frame's plays start and the listener is the camera the
		// world just moved.
		SCOPE_CYCLE_COUNTER(STAT_AudioTick);
		LLM_SCOPE(ELLMTag::Audio);
		TickPlayAudio();
	}

	if (Window != nullptr)
	{
		{
			SCOPE_CYCLE_COUNTER(STAT_GameViewportTick);
			GameViewport->Tick(DeltaSeconds);
		}
		// UE: RedrawViewports.
		GameViewport->GetGameViewport()->Draw(true);
	}
	if (bLogFrameTimes)
	{
		AccumulateFrameTimes(DeltaSeconds, FrameCycles, LoopCycles, FrameAllocations);
	}
	if (Window != nullptr)
	{
		if (Window->ShouldClose())
		{
			RequestEngineExit("Main window closed");
		}
	}
	else
	{
		// Keep the console current when stdout is redirected (CI smoke, servers stopped with Ctrl+C).
		GLog->Flush();
	}
	LastTickEndCycles = bLogFrameTimes ? FPlatformTime::Cycles64() : 0;
}

void FEngineRunStats::AddFrame(uint64 Microseconds)
{
	FirstFrameMicroseconds = Frames == 0 ? Microseconds : FirstFrameMicroseconds;
	WorstLaterFrameMicroseconds = Frames > 0 ? FMath::Max(WorstLaterFrameMicroseconds, Microseconds) : 0;
	++Frames;
	FrameMicroseconds += Microseconds;
	WorstFrameMicroseconds = FMath::Max(WorstFrameMicroseconds, Microseconds);
	const uint64 Bucket = Microseconds / FrameBucketMicroseconds;
	++FrameBuckets[Bucket < uint64(NumFrameBuckets) ? int32(Bucket) : NumFrameBuckets - 1];
}

uint64 FEngineRunStats::GetPercentileMicroseconds(float Fraction) const
{
	if (Frames <= 0)
	{
		return 0;
	}
	// The first bucket whose running count reaches Fraction of the frames (at least one frame).
	const int64 Target =
		FMath::Max<int64>(1, int64(FMath::CeilToInt(float(Frames) * FMath::Clamp(Fraction, 0.0f, 1.0f))));
	int64 Count = 0;
	for (int32 Bucket = 0; Bucket < NumFrameBuckets; ++Bucket)
	{
		Count += FrameBuckets[Bucket];
		if (Count >= Target)
		{
			return FMath::Min(uint64(Bucket + 1) * FrameBucketMicroseconds, WorstFrameMicroseconds);
		}
	}
	return WorstFrameMicroseconds;
}

namespace
{
	/** A stat's milliseconds per frame over a window. */
	double GetStatMilliseconds(const FCycleStatsWindow& Window, TStatId Stat)
	{
		return Window.GetMillisecondsPerFrame(Window.GetStatCounts(Stat).Cycles);
	}

	/** Engine loop cycles (FPlatformTime::Cycles64) per frame in milliseconds. */
	double GetLoopMilliseconds(uint64 Cycles, double Frames)
	{
		return Frames > 0.0 ? double(FPlatformTime::CyclesToMicroseconds(Cycles)) / (1000.0 * Frames) : 0.0;
	}

	/** The window's `Profile over N frames` block: the frame, then every scope as a hierarchy. */
	void LogProfile(const FCycleStatsWindow& Window)
	{
		const FCycleStatCounts Frame = Window.GetNodeCounts(0);
		const double Frames = double(FMath::Max<int64>(1, Window.GetNumFrames()));
		FString Header = FString::Printf("Profile over %lld frames (ms, calls): frame %.2f ms",
			static_cast<long long>(Window.GetNumFrames()), Window.GetMillisecondsPerFrame(Frame.Cycles));
		for (int32 Index = 0; Index < FPlatformTime::NumPerfCounters; ++Index)
		{
			Header += FString::Printf(
				", %.0f %s", double(Frame.Events[Index]) / Frames, FPlatformTime::GetPerfCounterName(Index));
		}
		UE_LOG(LogEngine, Display, "%s", *Header);
		TArray<FString> Lines;
		Window.GetReportLines(Lines);
		for (const FString& Line : Lines)
		{
			UE_LOG(LogEngine, Display, "  %s", *Line);
		}
	}

} // namespace

void UGameEngine::LogFrameSpike(uint64 FrameCycles)
{
	// FrameCycles is the frame that just ended, from its Tick's start to this one's: LastFrameWindow holds its scopes.
	const uint64 FrameMicroseconds = FPlatformTime::CyclesToMicroseconds(FrameCycles);
	if (FrameCycles > 0 && FrameMicroseconds > LongFrameMicroseconds &&
		FrameMicroseconds * 2 > LastFrameMicroseconds * 3 && NumSpikesLogged < MaxSpikesLogged)
	{
		++NumSpikesLogged;
		UE_LOG(LogEngine, Display,
			"Frame spike: %.1f ms (the frame before %.1f ms), its scopes:", double(FrameMicroseconds) / 1000.0,
			double(LastFrameMicroseconds) / 1000.0);
		TArray<FString> Lines;
		LastFrameWindow.GetReportLines(Lines, 2.0, 6, 40);
		for (const FString& Line : Lines)
		{
			UE_LOG(LogEngine, Display, "  %s", *Line);
		}
	}
	LastFrameMicroseconds = FrameMicroseconds;
	LastFrameWindow.Restart();
}

void UGameEngine::AccumulateFrameTimes(
	float DeltaSeconds, uint64 FrameCycles, uint64 LoopCycles, uint64 FrameAllocations)
{
	const IRendererModule* Renderer = GetRendererModulePtr();
	const FFrameStats Stats =
		Renderer != nullptr && Renderer->IsRendererInitialized() ? Renderer->GetFrameStats() : FFrameStats();
	if (FrameCycles > 0)
	{
		const uint64 FrameMicroseconds = FPlatformTime::CyclesToMicroseconds(FrameCycles);
		RunStats.AddFrame(FrameMicroseconds);
		// A frame longer than three fields is named (the first dozen): what it was is in the log around it.
		if (FrameMicroseconds > LongFrameMicroseconds && NumLongFramesLogged < MaxLongFramesLogged)
		{
			++NumLongFramesLogged;
			UE_LOG(LogEngine, Display, "Long frame %lld: %.1f ms", static_cast<long long>(RunStats.Frames),
				double(FrameMicroseconds) / 1000.0);
		}
		RunStats.LoopCycles += LoopCycles;
		RunStats.Allocations += FrameAllocations;
		RunStats.Triangles += Stats.TrianglesSubmitted;
		RunStats.RegisterWrites += Stats.RegisterWrites;
		RunStats.TextureUploads += Stats.TextureUploads;
		RunStats.TextureUploadBytes += Stats.TextureUploadBytes;
		RunStats.TextureEvictions += Stats.TextureEvictions;
		RunStats.ClutLoads += Stats.ClutLoads;
		RunStats.ObjectsVisible += Stats.ObjectsVisible;
		RunStats.DrawsSubmitted += Stats.DrawsSubmitted;
		RunStats.BatchesOnVU1 += Stats.BatchesOnVU1;
		RunStats.BatchesOnEmitter += Stats.BatchesOnEmitter;
		RunStats.BatchesClipped += Stats.BatchesClipped;
		RunStats.TrianglesClipped += Stats.TrianglesClipped;
		RunStats.PeakTextureResidentBytes = FMath::Max(RunStats.PeakTextureResidentBytes, Stats.TextureResidentBytes);
		RunStats.PeakObjects = FMath::Max(RunStats.PeakObjects, GetNumGCObjects());
	}
	else
	{
		// The first frame has no length (and the map's load before it is no frame's): the cycle stats' windows start
		// after it.
		RunWindow.Restart();
		FrameLogWindow.Restart();
	}

	++FrameLogFrames;
	FrameLogTime += DeltaSeconds;
	FrameLogWorst = FMath::Max(FrameLogWorst, DeltaSeconds);
	FrameLogLoopCycles += LoopCycles;
	FrameLogAllocations += FrameAllocations;
	FrameLogTriangles += Stats.TrianglesSubmitted;
	FrameLogRegisterWrites += Stats.RegisterWrites;
	FrameLogTextureUploads += Stats.TextureUploads;
	FrameLogTextureUploadBytes += Stats.TextureUploadBytes;
	FrameLogPeakTriangles = FMath::Max(FrameLogPeakTriangles, Stats.TrianglesSubmitted);
	FrameLogCellsVisible += Stats.CellsVisible;
	FrameLogCulledByCells += Stats.ObjectsCulledByCells;
	if (FrameLogTime < FrameLogSeconds)
	{
		return;
	}
	const double Frames = double(FrameLogFrames);
	const FCycleStatsWindow& Window = FrameLogWindow;
	// A part's average in milliseconds (the only floating point of the log, once a window).
	const auto Ms = [&Window](TStatId Stat) { return GetStatMilliseconds(Window, Stat); };
	UE_LOG(LogEngine, Display,
		"Frame times over %d frames: %.1f ms average (%.1f fps), %.1f ms worst; world %.1f ms, "
		"draw and present %.1f ms",
		FrameLogFrames, double(FrameLogTime) * 1000.0 / Frames, Frames / double(FrameLogTime),
		double(FrameLogWorst) * 1000.0, Ms(GET_STATID(STAT_WorldTick)) + Ms(GET_STATID(STAT_GarbageCollection)),
		Ms(GET_STATID(STAT_GameViewportTick)) + Ms(GET_STATID(STAT_ViewportDraw)) + Ms(GET_STATID(STAT_CanvasFlush)) +
			Ms(GET_STATID(STAT_ViewportPresent)));
	// Every part of the frame, so they add up to its time: what to make cheaper.
	UE_LOG(LogEngine, Display,
		"Frame split over %d frames (ms): input %.1f, audio %.1f, world %.1f, gc %.1f, viewport tick %.1f, "
		"scene and HUD %.1f (updates %.1f, scene %.1f, HUD %.1f, overlay %.1f), canvas %.1f, present %.1f, "
		"engine loop %.1f",
		FrameLogFrames, Ms(GET_STATID(STAT_EngineInput)), Ms(GET_STATID(STAT_AudioTick)),
		Ms(GET_STATID(STAT_WorldTick)), Ms(GET_STATID(STAT_GarbageCollection)), Ms(GET_STATID(STAT_GameViewportTick)),
		Ms(GET_STATID(STAT_ViewportDraw)), Ms(GET_STATID(STAT_EndOfFrameUpdates)), Ms(GET_STATID(STAT_SceneRendering)),
		Ms(GET_STATID(STAT_HUD)), Ms(GET_STATID(STAT_DebugOverlay)), Ms(GET_STATID(STAT_CanvasFlush)),
		Ms(GET_STATID(STAT_ViewportPresent)), GetLoopMilliseconds(FrameLogLoopCycles, Frames));
	// The GS work a frame records, identical on the PC and the PS2 (the same scene renderer): what the PS2's frame
	// costs follows from it (Docs/PLANS/ps2-preview.md V2; the milliseconds once calibrated in PCSX2).
	UE_LOG(LogEngine, Display,
		"Frame work over %d frames: %.0f triangles average (%d peak), %.0f GS register writes, %.2f texture uploads "
		"(%.2f KB), %.1f cells seen, %.1f objects left out by the cells",
		FrameLogFrames, double(FrameLogTriangles) / Frames, FrameLogPeakTriangles,
		double(FrameLogRegisterWrites) / Frames, double(FrameLogTextureUploads) / Frames,
		double(FrameLogTextureUploadBytes) / (1024.0 * Frames), double(FrameLogCellsVisible) / Frames,
		double(FrameLogCulledByCells) / Frames);
	// The heap and its churn (Docs/PLANS/ps2-shipping.md N17): a hot path should not allocate every frame.
	const FMallocUsage Heap = FMemory::GetUsage();
	UE_LOG(LogEngine, Display,
		"Memory over %d frames: heap %llu KB (peak %llu KB, %llu blocks), %.1f allocations per frame; frame stack peak "
		"%llu KB",
		FrameLogFrames, static_cast<unsigned long long>(Heap.CurrentBytes / 1024),
		static_cast<unsigned long long>(Heap.PeakBytes / 1024), static_cast<unsigned long long>(Heap.NumAllocations),
		double(FrameLogAllocations) / Frames,
		static_cast<unsigned long long>(FMemStack::Get().GetPeakByteCount() / 1024));
	LogProfile(Window);
	FrameLogAllocations = 0;
	FrameLogTriangles = 0;
	FrameLogRegisterWrites = 0;
	FrameLogTextureUploads = 0;
	FrameLogTextureUploadBytes = 0;
	FrameLogPeakTriangles = 0;
	FrameLogCellsVisible = 0;
	FrameLogCulledByCells = 0;
	FrameLogFrames = 0;
	FrameLogTime = 0.0f;
	FrameLogWorst = 0.0f;
	FrameLogLoopCycles = 0;
	FrameLogWindow.Restart();
}

void UGameEngine::LogRunSummary() const
{
	const FEngineRunStats& Run = RunStats;
	const double Frames = double(Run.Frames);
	const auto Ms = [](uint64 Microseconds) { return double(Microseconds) / 1000.0; };
	// A part's average per frame in milliseconds.
	const FCycleStatsWindow& Window = RunWindow;
	const auto PartMs = [&Window](TStatId Stat) { return GetStatMilliseconds(Window, Stat); };
	const double Seconds = double(Run.FrameMicroseconds) / 1000000.0;
	// Each GS register write is a 16-byte A+D quadword of the GIF packet (FGSGifPacket).
	const double GifKilobytes = (double(Run.RegisterWrites) * 16.0) / (1024.0 * Frames);
	UE_LOG(LogEngine, Display,
		"FrameStats Summary: frames=%lld seconds=%.1f avg_ms=%.2f fps=%.2f p50_ms=%.2f p95_ms=%.2f p99_ms=%.2f "
		"worst_ms=%.2f first_ms=%.2f worst_later_ms=%.2f input_ms=%.2f audio_ms=%.2f world_ms=%.2f gc_ms=%.2f "
		"viewport_ms=%.2f scene_ms=%.2f hud_ms=%.2f "
		"canvas_ms=%.2f present_ms=%.2f loop_ms=%.2f tris=%.0f gs_writes=%.0f gif_kb=%.1f tex_uploads=%.2f "
		"tex_upload_kb=%.2f tex_evictions=%.2f clut_loads=%.2f tex_resident_kb=%d gmalloc_peak_kb=%lld heap_kb=%lld "
		"allocs_per_frame=%.1f "
		"uobjects_peak=%d",
		static_cast<long long>(Run.Frames), Seconds, Ms(Run.FrameMicroseconds) / Frames, Frames / Seconds,
		Ms(Run.GetPercentileMicroseconds(0.50f)), Ms(Run.GetPercentileMicroseconds(0.95f)),
		Ms(Run.GetPercentileMicroseconds(0.99f)), Ms(Run.WorstFrameMicroseconds), Ms(Run.FirstFrameMicroseconds),
		Ms(Run.WorstLaterFrameMicroseconds), PartMs(GET_STATID(STAT_EngineInput)), PartMs(GET_STATID(STAT_AudioTick)),
		PartMs(GET_STATID(STAT_WorldTick)), PartMs(GET_STATID(STAT_GarbageCollection)),
		PartMs(GET_STATID(STAT_GameViewportTick)),
		PartMs(GET_STATID(STAT_EndOfFrameUpdates)) + PartMs(GET_STATID(STAT_SceneRendering)),
		PartMs(GET_STATID(STAT_HUD)) + PartMs(GET_STATID(STAT_DebugOverlay)), PartMs(GET_STATID(STAT_CanvasFlush)),
		PartMs(GET_STATID(STAT_ViewportPresent)), GetLoopMilliseconds(Run.LoopCycles, Frames),
		double(Run.Triangles) / Frames, double(Run.RegisterWrites) / Frames, GifKilobytes,
		double(Run.TextureUploads) / Frames, double(Run.TextureUploadBytes) / (1024.0 * Frames),
		double(Run.TextureEvictions) / Frames, double(Run.ClutLoads) / Frames,
		(Run.PeakTextureResidentBytes + 1023) / 1024, static_cast<long long>(FMemory::GetUsage().PeakBytes / 1024),
		static_cast<long long>(FMemory::GetUsage().CurrentBytes / 1024), double(Run.Allocations) / Frames,
		Run.PeakObjects);
	// The scene's work a frame (N29): what its time goes into.
	UE_LOG(LogEngine, Display,
		"SceneWork: objects=%.1f draws=%.1f batches_vu1=%.1f batches_ee=%.1f batches_clipped=%.1f clipped_tris=%.1f",
		double(Run.ObjectsVisible) / Frames, double(Run.DrawsSubmitted) / Frames, double(Run.BatchesOnVU1) / Frames,
		double(Run.BatchesOnEmitter) / Frames, double(Run.BatchesClipped) / Frames,
		double(Run.TrianglesClipped) / Frames);
	// Each memory tag's peak over the run, and the small-block arena's (its budget: FPlatformProperties).
	if (FLowLevelMemTracker::IsEnabled())
	{
		const FMallocUsage Heap = FMemory::GetUsage();
		FString Tags = FString::Printf("MemoryTags: arena_peak_kb=%llu arena_kb=%llu arena_overflows=%llu",
			static_cast<unsigned long long>(Heap.ArenaPeakBytes / 1024),
			static_cast<unsigned long long>(Heap.ArenaBytes / 1024),
			static_cast<unsigned long long>(Heap.ArenaOverflows));
		for (int32 Index = 0; Index <= FLowLevelMemTracker::NumTags; ++Index)
		{
			const FLLMTagStats TagStats = FLowLevelMemTracker::GetTagStats(ELLMTag(Index));
			Tags += FString::Printf(" %s_peak_kb=%llu", FLowLevelMemTracker::GetTagName(ELLMTag(Index)),
				static_cast<unsigned long long>(TagStats.PeakBytes / 1024));
		}
		UE_LOG(LogEngine, Display, "%s", *Tags);
	}

	// The whole run's hierarchy, then the top level scopes' averages as key=value (the stat's name without STAT_), what
	// MeasurePS2 adds to its CSV.
	LogProfile(Window);
	const FCycleStatCounts Frame = Window.GetNodeCounts(0);
	const double ProfileFrames = double(FMath::Max<int64>(1, Window.GetNumFrames()));
	FString Summary = FString::Printf("ProfileSummary: frames=%lld frame_ms=%.2f",
		static_cast<long long>(Window.GetNumFrames()), Window.GetMillisecondsPerFrame(Frame.Cycles));
	for (int32 Index = 0; Index < FPlatformTime::NumPerfCounters; ++Index)
	{
		Summary += FString::Printf(
			" frame_%s=%.0f", FPlatformTime::GetPerfCounterName(Index), double(Frame.Events[Index]) / ProfileFrames);
	}
	for (int32 Node = FThreadStats::GetNode(0).FirstChild; Node != INDEX_NONE;
		Node = FThreadStats::GetNode(Node).NextSibling)
	{
		const FCycleStatCounts Counts = Window.GetNodeCounts(Node);
		if (Counts.Calls == 0)
		{
			continue;
		}
		FString Key(FThreadStats::GetNode(Node).Stat->StatName);
		Key.RemoveFromStart(TEXT("STAT_"));
		Summary += FString::Printf(" %s_ms=%.2f", *Key, Window.GetMillisecondsPerFrame(Counts.Cycles));
		for (int32 Index = 0; Index < FPlatformTime::NumPerfCounters; ++Index)
		{
			Summary += FString::Printf(" %s_%s=%.0f", *Key, FPlatformTime::GetPerfCounterName(Index),
				double(Counts.Events[Index]) / ProfileFrames);
		}
	}
	UE_LOG(LogEngine, Display, "%s", *Summary);
}

void UGameEngine::TickPlayAudio()
{
	const UCameraComponent& Camera = *GameViewport->GetViewCamera();
	const FVector Eye = Camera.GetCameraLocation();
	const FVector Forward = Camera.ForwardVector();
	const FVector Up = FVector(0.0f, 0.0f, 1.0f);
	AudioDevice.SetListener(Eye, Forward, Up);
	AudioDevice.Tick();
}
