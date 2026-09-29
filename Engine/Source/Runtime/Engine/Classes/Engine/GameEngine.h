#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ShaderCore.h"
#include "Stats/Stats.h"
#include "GameEngine.generated.h"

class UWorld;

/**
 * -LogFrameTimes over the whole run (Leon), logged at exit as the `FrameStats Summary:` line of key=value pairs that
 * MeasurePS2 reads (Docs/PLANS/ps2-shipping.md N1): the frames' times as a histogram for their percentiles, the time
 * outside the engine's tick, the renderer's work and the peaks of the heap and of the objects. The frame's parts are
 * the cycle stats' (UGameEngine's run window).
 */
struct ENGINE_API FEngineRunStats
{
	/** Buckets of FrameBucketMicroseconds, the last one holding every longer frame: up to 256 ms. */
	static constexpr int32 NumFrameBuckets = 1024;
	static constexpr uint64 FrameBucketMicroseconds = 250;

	int64 Frames = 0;
	uint64 FrameMicroseconds = 0;
	uint64 WorstFrameMicroseconds = 0;
	/** The first frame's time (the map's first tick: what its start spawns), and the worst of the others (N24's
	 * target). */
	uint64 FirstFrameMicroseconds = 0;
	uint64 WorstLaterFrameMicroseconds = 0;
	uint32 FrameBuckets[NumFrameBuckets] = {};
	/** FPlatformTime::Cycles64 from the end of a Tick to the start of the next: the engine loop's ticker and polling.
	 */
	uint64 LoopCycles = 0;
	int64 Triangles = 0;
	int64 RegisterWrites = 0;
	int64 TextureUploads = 0;
	int64 TextureUploadBytes = 0;
	int64 TextureEvictions = 0;
	int64 ClutLoads = 0;
	int32 PeakTextureResidentBytes = 0;
	int32 PeakObjects = 0;
	/** The scene's work (FFrameStats, N29): objects and draws, the batches by placement. */
	int64 ObjectsVisible = 0;
	int64 DrawsSubmitted = 0;
	int64 BatchesOnVU1 = 0;
	int64 BatchesOnEmitter = 0;
	int64 BatchesClipped = 0;
	int64 TrianglesClipped = 0;
	/** GMalloc's allocations in the frames (FMallocUsage::TotalAllocations): the heap churn per frame. */
	uint64 Allocations = 0;

	/** Adds a frame of Microseconds (from one tick's start to the next's). */
	void AddFrame(uint64 Microseconds);
	/** The time Fraction of the frames take at most, in microseconds (the upper edge of the bucket reaching it). */
	[[nodiscard]] uint64 GetPercentileMicroseconds(float Fraction) const;
};

/**
 * The engine of a game (UE: UGameEngine), GEngine's class by default (`[/Script/Engine.Engine]
 * GameEngine=/Script/Engine.GameEngine`).
 *
 * - Init starts the renderer on the main window FEngineLoop::PreInit made (none when headless), then creates the game
 *   instance (GameInstanceClass of UGameMapsSettings) with its world context, the game viewport client
 *   (GameViewportClientClassName) and its first local player.
 * - Start: the game instance opens the first map (UGameInstance::StartGameInstance → Browse → LoadMap).
 * - Tick: the viewport client's input, the pending travel, the world tick, the garbage collection timer, then the
 *   viewport draws and presents the frame.
 * - PreExit: the game instance shuts down, the world goes (a garbage collection safe point, plan decision D11), then
 *   the renderer; FEngineLoop destroys the window after.
 * - `-LogFrameTimes` (windowed or headless) holds the cycle stats on (Stats/Stats.h) and logs, every 5 seconds, the
 *   frames' average and worst time and the average spent in the world's tick and in the viewport's draw and present
 *   (Docs/PLANS/ps2-engine.md E4: the PS2's frame in its EE log), the `Frame split` (the average of each part of the
 *   frame: the top level scopes of EngineStats.h and the engine loop, so the parts add up to the frame) and the
 *   `Profile over N frames (ms, calls):` block (every scope of the frame as a hierarchy). At exit it logs the whole
 *   run's `FrameStats Summary:` (FEngineRunStats: the percentiles of the frame time, the parts, the peaks) and
 *   `ProfileSummary:` (each top level scope's average) for MeasurePS2 (Docs/PLANS/ps2-shipping.md N9).
 *
 * It draws through the Renderer module's interface (IRendererModule, found by name); Engine never includes a Renderer
 * header.
 */
UCLASS(Config = Engine, Transient)
class ENGINE_API UGameEngine : public UEngine
{
	GENERATED_BODY()

public:
	UGameEngine(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The game session; its world context holds the game world (UE: GameInstance). */
	UPROPERTY(Transient)
	UGameInstance* GameInstance = nullptr;

	// UEngine
	void Init(IEngineLoop* InEngineLoop) override;
	void Start() override;
	void PreExit() override;
	void Tick(float DeltaSeconds, bool bIdleMode) override;
	TArray<FWorldContext*> GetWorldContexts() override;

	// UObject
	void BeginDestroy() override;

	/** The game world: the game instance's world (UE: GetGameWorld). */
	[[nodiscard]] UWorld* GetGameWorld() const;

private:
	/** Destroys the game instance's world and collects garbage (world teardown is a safe point). */
	void DestroyGameWorld();
	/** The shaders whose files changed (Leon's hot reload; `RecompileShaders` forces it). */
	[[nodiscard]] EShaderReloadResult ReloadAllShaders(bool bForce);
	/** The audio listener follows the view camera, then the audio device ticks (after the world). */
	void TickPlayAudio();
	/**
	 * -LogFrameTimes: adds a frame (FrameCycles from the last tick's start to this one's, 0 for the first; LoopCycles
	 * from the last tick's end to this one's start) to the run and to the window, and logs the window's figures every
	 * FrameLogSeconds.
	 */
	void AccumulateFrameTimes(float DeltaSeconds, uint64 FrameCycles, uint64 LoopCycles, uint64 FrameAllocations);

	/** At a Tick's start: logs the scopes of the frame that just ended when it was a spike (LastFrameWindow). */
	void LogFrameSpike(uint64 FrameCycles);
	/** -LogFrameTimes: the run's `FrameStats Summary:` and `ProfileSummary:` lines, at exit. */
	void LogRunSummary() const;

	static constexpr float FrameLogSeconds = 5.0f;
	bool bLogFrameTimes = false;
	int32 FrameLogFrames = 0;
	float FrameLogTime = 0.0f;
	/**
	 * -LogFrameTimes names the frames longer than three NTSC fields (50.05 ms; ps2-shipping N24's target), the first
	 * MaxLongFramesLogged.
	 */
	static constexpr uint64 LongFrameMicroseconds = 51000;
	static constexpr int32 MaxLongFramesLogged = 12;
	int32 NumLongFramesLogged = 0;
	float FrameLogWorst = 0.0f;
	uint64 FrameLogLoopCycles = 0;
	/** The cycle stats over the log's window and over the run (from the end of the first frame). */
	FCycleStatsWindow FrameLogWindow;
	FCycleStatsWindow RunWindow;
	/**
	 * The last frame's cycle stats: a long frame half again as long as the one before (a spike, not the render's usual
	 * frame) logs its scopes, what to spread (-LogFrameTimes, the first MaxSpikesLogged; ps2-shipping N24b).
	 */
	FCycleStatsWindow LastFrameWindow;
	uint64 LastFrameMicroseconds = 0;
	static constexpr int32 MaxSpikesLogged = 4;
	int32 NumSpikesLogged = 0;
	/** When the last Tick started and ended (FPlatformTime::Cycles64, read with -LogFrameTimes), 0 before the first. */
	uint64 LastTickStartCycles = 0;
	uint64 LastTickEndCycles = 0;
	/** GMalloc's allocations since start-up when the last Tick started (FMallocUsage::TotalAllocations). */
	uint64 LastTickStartAllocations = 0;
	FEngineRunStats RunStats;
	/** GMalloc's allocations over the log's window. */
	uint64 FrameLogAllocations = 0;
	/** The renderer's work over the window (FFrameStats), the same on every platform: the PS2's frame cost's inputs. */
	int64 FrameLogTriangles = 0;
	int64 FrameLogRegisterWrites = 0;
	int64 FrameLogTextureUploads = 0;
	int64 FrameLogTextureUploadBytes = 0;
	int32 FrameLogPeakTriangles = 0;
	/** The map's cells the views saw and the objects the cells left out, over the window (N15's cells). */
	int64 FrameLogCellsVisible = 0;
	int64 FrameLogCulledByCells = 0;
};
