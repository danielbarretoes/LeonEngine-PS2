#pragma once

#include "AudioDevice.h"
#include "CoreMinimal.h"
#include "Debug/DebugOverlay.h"
#include "Engine/EngineBaseTypes.h"
#include "FixedStepClock.h"
#include "Misc/Exec.h"
#include "Templates/SubclassOf.h"
#include "UObject/GarbageCollection.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Engine.generated.h"

class IEngineLoop;
class UGameViewportClient;
class ULocalPlayer;
class UPendingNetGame;
class USoundWave;
class UTexture2D;
class UWorld;
struct FWorldContext;

/**
 * The engine (UE: UEngine), a config class of the Engine config ([/Script/Engine.Engine]) and the base of UGameEngine.
 * FEngineLoop::Init creates GEngine from `[/Script/Engine.Engine] GameEngine=` (plan decision D18), roots it and calls
 * Init, then Start; each frame it measures the frame's time and the fixed steps it holds
 * (UpdateTimeAndHandleMaxTickRate), runs the deferred commands and Tick; PreExit ends it.
 *
 * - The game steps at a fixed rate (ps2-shipping D4): FixedStepClock turns the real time (integer microseconds of
 *   FPlatformTime::Cycles64) into whole steps of 1 / FixedStepsPerSecond s (at most MaxStepsPerFrame a frame), and
 *   the render draws between the last two (GetRenderInterpolationAlpha). A benchmark (`-benchmark`) or a capture
 *   (FApp::IsUnattended) runs exactly one step a frame instead, drawn as it is: the frames then do not follow the
 *   clock, so a capture is the same frame every run.
 *
 * - Browse / LoadMap: a URL opens a map in a world context (the flow is on LoadMap).
 * - Exec: the engine's console commands (`exit`, `obj gc`, `stat unit`, `stat cycles`, `stat memory`,
 *   `RecompileShaders`, `open`),
 *   then every FSelfRegisteringExec. The console reaches it through the viewport client and the local player.
 * - Its default assets come from the config (the *Name paths below, as UE's DefaultTextureName & co.): Init loads
 *   them (InitializeObjectReferences). The audio device and the on-screen debug text (AddOnScreenDebugMessage) belong
 *   to it.
 *
 * Leon keeps the world contexts on the game instances (P12): GetWorldContexts collects them (UE: the engine's
 * WorldList).
 */
UCLASS(Abstract, Config = Engine, Transient)
class ENGINE_API UEngine
	: public UObject
	, public FExec
{
	GENERATED_BODY()

public:
	UEngine(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The local player class (UE: LocalPlayerClassName, [/Script/Engine.Engine] LocalPlayerClassName=). */
	UPROPERTY(Config)
	FSoftClassPath LocalPlayerClassName;

	/** LocalPlayerClassName, loaded by Init (UE: LocalPlayerClass). */
	UPROPERTY(Transient)
	TSubclassOf<ULocalPlayer> LocalPlayerClass;

	/** The game viewport client's class (UE: GameViewportClientClassName). */
	UPROPERTY(Config)
	FSoftClassPath GameViewportClientClassName;

	/** The game's view (UE: GameViewport); null before Init. */
	UPROPERTY(Transient)
	UGameViewportClient* GameViewport = nullptr;

	/** Shows the stats overlay from the start (Leon: bShowStatsByDefault; `-showstats` does the same). */
	UPROPERTY(Config)
	bool bShowStatsByDefault = false;

	/**
	 * The material of a mesh slot without one (UE: the `DefaultMaterialName` of the engine config, which
	 * UMaterial::GetDefaultMaterial loads).
	 */
	UPROPERTY(GlobalConfig)
	FSoftObjectPath DefaultMaterialName;

	/** The engine's default texture, a grey checker (UE: DefaultTextureName). */
	UPROPERTY(GlobalConfig)
	FSoftObjectPath DefaultTextureName;

	/** DefaultTextureName, loaded by Init (UE: DefaultTexture). */
	UPROPERTY(Transient)
	UTexture2D* DefaultTexture = nullptr;

	/**
	 * The sound wave of each UI cue FAudioDevice::PlayUiSound plays (Leon; UE's Slate styles name their sounds):
	 * Click, Confirm, Back and Error. Empty leaves the cue silent.
	 */
	UPROPERTY(GlobalConfig)
	FSoftObjectPath UIClickSoundName;
	UPROPERTY(GlobalConfig)
	FSoftObjectPath UIConfirmSoundName;
	UPROPERTY(GlobalConfig)
	FSoftObjectPath UIBackSoundName;
	UPROPERTY(GlobalConfig)
	FSoftObjectPath UIErrorSoundName;

	/** The UI cues' sound waves, loaded by Init in EUISound order (null: a silent cue) (Leon). */
	UPROPERTY(Transient)
	TArray<USoundWave*> UISounds;

	/** Console commands to run at the start of the next frame (UE: DeferredCommands; `-ExecCmds=` fills it). */
	TArray<FString> DeferredCommands;

	/** Starts the engine (UE: Init): the config classes, the default assets, the audio device, the game instance. */
	virtual void Init(IEngineLoop* InEngineLoop);

	/**
	 * Loads the default assets the config names from their packages (UE: InitializeObjectReferences):
	 * DefaultTexture, the default material (UMaterial::GetDefaultMaterial) and the UI sounds.
	 */
	virtual void InitializeObjectReferences();

	/** Starts the game (UE: Start): the game instance opens its first map. */
	virtual void Start();

	/** Ends the game before the modules shut down (UE: PreExit). */
	virtual void PreExit();

	/** One frame (UE: Tick). */
	virtual void Tick(float DeltaSeconds, bool bIdleMode);

	/** Runs the queued console commands through the first local player, else the engine (UE: TickDeferredCommands). */
	void TickDeferredCommands();

	/**
	 * The frame's time (UE: UpdateTimeAndHandleMaxTickRate): the real time since the last frame goes to FApp's delta
	 * time and to the fixed step clock, which says how many world steps Tick runs (ConsumeFrameSteps) and where the
	 * render draws between the last two (GetRenderInterpolationAlpha). A headless run that is not a benchmark waits
	 * here until its next step is due (UE's max tick rate), instead of spinning; `-benchmark` runs one step a frame
	 * without waiting, and a capture (FApp::IsUnattended) one step a frame.
	 */
	virtual void UpdateTimeAndHandleMaxTickRate();

	/**
	 * The world steps this frame runs: UpdateTimeAndHandleMaxTickRate's, else (a Tick called without it: tests,
	 * tools) the steps DeltaSeconds holds on the fixed step clock. Tick calls it once a frame.
	 */
	int32 ConsumeFrameSteps(float DeltaSeconds);
	/** Where the frame is drawn between the world's last two steps, [0, 1] (1: the last step). */
	[[nodiscard]] float GetRenderInterpolationAlpha() const
	{
		return RenderInterpolationAlpha;
	}
	/** The game's fixed step clock (D4). */
	[[nodiscard]] const FFixedStepClock& GetFixedStepClock() const
	{
		return FixedStepClock;
	}

	/** The world's step rate (D4: 30 Hz; `[/Script/Engine.Engine] FixedStepsPerSecond`). */
	UPROPERTY(Config)
	int32 FixedStepsPerSecond = FFixedStepClock::DefaultStepsPerSecond;

	/** The most world steps a frame runs before the time beyond is dropped (the spiral-of-death guard). */
	UPROPERTY(Config)
	int32 MaxStepsPerFrame = FFixedStepClock::DefaultMaxStepsPerFrame;

	/**
	 * The time ProcessAsyncLoading may take a frame, in milliseconds, after one package at least (UE:
	 * s.AsyncLoadingTimeLimit); 0 serializes every package whose bytes are in (Docs/PLANS/ps2-shipping.md N24).
	 */
	UPROPERTY(Config)
	float AsyncLoadingTimeLimit = 0.0f;

	/** The engine's console commands, then every FSelfRegisteringExec (UE: UEngine::Exec). */
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Opens the URL in the world context (UE: Browse): an invalid URL fails, anything else is a local map and goes to
	 * LoadMap.
	 */
	virtual EBrowseReturnVal::Type Browse(FWorldContext& WorldContext, FURL URL, FString& Error);

	/**
	 * Replaces the context's world with the URL's map (UE: LoadMap):
	 * 1. the map is found: a `.lmap` package, named by its long package name (`/Game/Maps/X`, `/Engine/Maps/X`) or by
	 *    its file (a map file no mount point contains mounts its content folder, the folder above its `Maps/`
	 *    folder); a missing map fails and leaves the current world as it is;
	 * 2. the players leave their controllers and the old world ends play (EEndPlayReason::LevelTransition), is
	 *    destroyed and the garbage collected (a safe point, plan decision D11);
	 * 3. the map package is loaded (LoadPackage) and its world (UWorld::FindWorldInPackage) is rooted and initialized
	 *    (UWorld::InitWorld);
	 * 4. the game mode (UWorld::SetGameMode: `?game=`, the world settings, GlobalDefaultGameMode) and
	 *    InitializeActorsForPlay (the map's components register, AGameModeBase::InitGame, the actors initialize);
	 * 5. every local player logs in (ULocalPlayer::SpawnPlayActor: Login, PostLogin, RestartPlayer);
	 * 6. UWorld::BeginPlay, then UGameInstance::LoadComplete.
	 */
	virtual bool LoadMap(FWorldContext& WorldContext, FURL URL, UPendingNetGame* Pending, FString& Error);

	/** Queues a travel for the next frame, so a map never changes while its world ticks (UE: SetClientTravel). */
	void SetClientTravel(UWorld* InWorld, const TCHAR* NextURL, ETravelType InTravelType);

	/** Browses the context's pending travel, if any (UE: TickWorldTravel). */
	void TickWorldTravel(FWorldContext& WorldContext, float DeltaSeconds);

	/**
	 * The garbage collector's step (UE: ConditionalCollectGarbage, with the step's time passed in): a slice of the
	 * incremental collection under way, a new one every gc.TimeBetweenPurgingPendingKillObjects seconds, or the full
	 * collection ForceGarbageCollection asked for (FGarbageCollectionTimer). Tick calls it after each world step, a
	 * safe point (D11). True when a collection ended.
	 */
	bool ConditionalCollectGarbage(float DeltaSeconds);

	/**
	 * Makes the next ConditionalCollectGarbage a full collection (UE: ForceGarbageCollection): a game asks for one when
	 * a lot went at once (ShooterGame: a round's start). Leon's collections always purge fully; bFullPurge is UE's.
	 */
	void ForceGarbageCollection(bool bFullPurge = false);

	/** The context that holds InWorld, or null (UE: GetWorldContextFromWorld). */
	[[nodiscard]] virtual FWorldContext* GetWorldContextFromWorld(const UWorld* InWorld);

	/** Every world context (UE: GetWorldContexts, the engine's WorldList; Leon: the game instances' contexts). */
	[[nodiscard]] virtual TArray<FWorldContext*> GetWorldContexts();

	/**
	 * Shows a message in the top-left console for TimeToDisplay seconds (UE: AddOnScreenDebugMessage; the colour is
	 * an FLinearColor, and the key is kept for UE's signature: every message is added). Headless, it is logged.
	 */
	void AddOnScreenDebugMessage(
		int32 Key, float TimeToDisplay, const FLinearColor& DisplayColor, const FString& DebugMessage);

	/** The on-screen debug text: messages, stats and toggle hints (UE: the engine's screen messages and stats). */
	[[nodiscard]] FDebugOverlay& GetDebugOverlay()
	{
		return Overlay;
	}

	[[nodiscard]] FAudioDevice& GetAudioDevice()
	{
		return AudioDevice;
	}

	/** Init ran and PreExit did not. */
	[[nodiscard]] bool IsInitialized() const
	{
		return bIsInitialized;
	}

	/** Nothing renders: no window, silent audio (`-nullrhi`, tests) (UE: !FApp::CanEverRender). */
	[[nodiscard]] bool IsHeadless() const
	{
		return bHeadless;
	}

	/**
	 * The frames-per-second / RAM / triangles overlay (UE: `stat unit`, F4 on the desktop, R3 on the DualShock); off
	 * unless bShowStatsByDefault.
	 */
	void SetHudStatsVisible(bool bVisible);
	[[nodiscard]] bool IsHudStatsVisible() const;

	/**
	 * The cycle stats page (UE: `stat cycles`, F7 on the desktop): the frame's scopes (Stats/Stats.h) in the overlay's
	 * top-left block, refreshed four times a second. The stats collect while it shows.
	 */
	void SetCycleStatsVisible(bool bVisible);
	[[nodiscard]] bool IsCycleStatsVisible() const
	{
		return bCycleStatsVisible;
	}

	/**
	 * The memory page (UE: `stat memory`, `stat llm`; F8 on the desktop): GMalloc's heap, arena and churn, the frame's
	 * stack and every memory tag's current, peak and budget (HAL/LowLevelMemTracker.h), in the overlay's top-left block
	 * instead of the cycles page.
	 */
	void SetMemoryStatsVisible(bool bVisible);
	[[nodiscard]] bool IsMemoryStatsVisible() const
	{
		return bMemoryStatsVisible;
	}

protected:
	/** The on-screen debug text. */
	FDebugOverlay Overlay;
	FAudioDevice AudioDevice;
	/** Times the periodic garbage collection (UE: TimeSinceLastPendingKillPurge). */
	FGarbageCollectionTimer GarbageCollectionTimer;
	/** The fixed steps (D4) and what UpdateTimeAndHandleMaxTickRate made of the frame. */
	FFixedStepClock FixedStepClock;
	int32 NumStepsThisFrame = 0;
	/** UpdateTimeAndHandleMaxTickRate ran since the last Tick. */
	bool bFrameTimeUpdated = false;
	float RenderInterpolationAlpha = 1.0f;
	/** When the last frame's time was taken (FPlatformTime::Cycles64), 0 before the first. */
	uint64 LastFrameCycles = 0;

	bool bIsInitialized = false;
	bool bHeadless = false;
	bool bHudStatsVisible = false;
	bool bCycleStatsVisible = false;
	bool bMemoryStatsVisible = false;
};

/** The engine (UE: GEngine); FEngineLoop::Init creates it from the Engine config. */
extern ENGINE_API UEngine* GEngine;
