#pragma once

#include "AudioDevice.h"
#include "CoreMinimal.h"
#include "Debug/DebugOverlay.h"
#include "Engine/EngineBaseTypes.h"
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
 * Init, then Start; each frame it runs the deferred commands and Tick; PreExit ends it.
 *
 * - Browse / LoadMap: a URL opens a map in a world context (the flow is on LoadMap).
 * - Exec: the engine's console commands (`exit`, `obj gc`, `stat unit`, `RecompileShaders`, `open`), then every
 *   FSelfRegisteringExec. The console reaches it through the viewport client and the local player.
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

	/** The engine's bump normal map, a strong procedural ripple (Leon). */
	UPROPERTY(GlobalConfig)
	FSoftObjectPath DefaultBumpNormalTextureName;

	/** DefaultBumpNormalTextureName, loaded by Init (Leon). */
	UPROPERTY(Transient)
	UTexture2D* DefaultBumpNormalTexture = nullptr;

	/**
	 * The sound wave of each UI cue FAudioDevice::PlayUiSound plays (Leon; UE's Slate styles name their sounds):
	 * Click, Confirm, Back and Error. Empty keeps the cue's procedural tone.
	 */
	UPROPERTY(GlobalConfig)
	FSoftObjectPath UIClickSoundName;
	UPROPERTY(GlobalConfig)
	FSoftObjectPath UIConfirmSoundName;
	UPROPERTY(GlobalConfig)
	FSoftObjectPath UIBackSoundName;
	UPROPERTY(GlobalConfig)
	FSoftObjectPath UIErrorSoundName;

	/** The UI cues' sound waves, loaded by Init in EUISound order (null: the procedural tone) (Leon). */
	UPROPERTY(Transient)
	TArray<USoundWave*> UISounds;

	/** Console commands to run at the start of the next frame (UE: DeferredCommands; `-ExecCmds=` fills it). */
	TArray<FString> DeferredCommands;

	/** Starts the engine (UE: Init): the config classes, the default assets, the audio device, the game instance. */
	virtual void Init(IEngineLoop* InEngineLoop);

	/**
	 * Loads the default assets the config names from their packages (UE: InitializeObjectReferences):
	 * DefaultTexture, DefaultBumpNormalTexture, the default material (UMaterial::GetDefaultMaterial) and the UI
	 * sounds.
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

	/** The engine's console commands, then every FSelfRegisteringExec (UE: UEngine::Exec). */
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Opens the URL in the world context (UE: Browse): an invalid URL fails, anything else is a local map and goes to
	 * LoadMap.
	 */
	virtual EBrowseReturnVal::Type Browse(FWorldContext& WorldContext, FURL URL, FString& Error);

	/**
	 * Replaces the context's world with the URL's map (UE: LoadMap):
	 * 1. the map is found (a long package name's `.llev` file under its mount point, or a `.llev` path; the `.lmap`
	 *    packages come in P15); a missing map fails and leaves the current world as it is;
	 * 2. the players leave their controllers and the old world ends play (EEndPlayReason::LevelTransition), is
	 *    destroyed and the garbage collected (a safe point, plan decision D11);
	 * 3. a new world (UWorld::CreateWorld) gets the level's actors from the `.llev` reader, and a `.llev` camera
	 *    framing becomes an APlayerStartPIE (Leon: the editor's Play From Here start);
	 * 4. the game mode (UWorld::SetGameMode: `?game=`, the world settings, GlobalDefaultGameMode) and
	 *    InitializeActorsForPlay (AGameModeBase::InitGame);
	 * 5. every local player logs in (ULocalPlayer::SpawnPlayActor: Login, PostLogin, RestartPlayer);
	 * 6. UWorld::BeginPlay, then UGameInstance::LoadComplete.
	 */
	virtual bool LoadMap(FWorldContext& WorldContext, FURL URL, UPendingNetGame* Pending, FString& Error);

	/** Queues a travel for the next frame, so a map never changes while its world ticks (UE: SetClientTravel). */
	void SetClientTravel(UWorld* InWorld, const TCHAR* NextURL, ETravelType InTravelType);

	/** Browses the context's pending travel, if any (UE: TickWorldTravel). */
	void TickWorldTravel(FWorldContext& WorldContext, float DeltaSeconds);

	/**
	 * Collects garbage once gc.TimeBetweenPurgingPendingKillObjects has passed (UE: ConditionalCollectGarbage, with the
	 * frame time passed in). Tick calls it after the world tick, a safe point (D11). True when it collected.
	 */
	bool ConditionalCollectGarbage(float DeltaSeconds);

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

	/** The frames-per-second / RAM / triangles overlay (UE: `stat unit`); off unless bShowStatsByDefault. */
	void SetHudStatsVisible(bool bVisible);
	[[nodiscard]] bool IsHudStatsVisible() const
	{
		return bShowHudStats;
	}

protected:
	/** The on-screen debug text. */
	FDebugOverlay Overlay;
	FAudioDevice AudioDevice;
	/** Times the periodic garbage collection (UE: TimeSinceLastPendingKillPurge). */
	FGarbageCollectionTimer GarbageCollectionTimer;

	bool bIsInitialized = false;
	bool bHeadless = false;
	bool bShowHudStats = false;
};

/** The engine (UE: GEngine); FEngineLoop::Init creates it from the Engine config. */
extern ENGINE_API UEngine* GEngine;
