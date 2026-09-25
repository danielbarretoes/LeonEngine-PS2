#pragma once

#include "AudioDevice.h"
#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Debug/DebugOverlay.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "GameFramework/HUD.h"
#include "GameFramework/InputMapping.h"
#include "GameFramework/PlayInputTarget.h"
#include "GenericPlatform/GenericApplication.h"
#include "ResourceCache.h"
#include "SceneView.h"
#include "ShaderCore.h"
#include "UObject/GCObject.h"
#include "UObject/GarbageCollection.h"

class UWorld;

/**
 * Top-level runtime: platform window (FGenericWindow), per-frame Tick, orbit-camera input, FPS overlay,
 * UGameInstance, and a FResourceCache the level loader fills.
 *
 * It draws through the Renderer module's interface (IRendererModule, found by name): Initialize starts the renderer
 * on the window's context, each frame renders a view family of the game world's scene through the view camera and
 * flushes a canvas with the HUD and the debug text, and Shutdown stops the renderer before the window goes. Engine
 * never includes a Renderer header.
 *
 * Not a UObject yet (P13 makes it UEngine / UGameEngine with GEngine): it keeps its UObjects alive as an FGCObject.
 * It creates the UGameInstance, whose world context holds the game UWorld; GetLevel is that world's persistent level.
 * Shutdown ends the world and collects garbage (a safe point, plan decision D11).
 */
class ENGINE_API UGameEngine : public FGCObject
{
public:
	using FUpdateCallback = TFunction<void(float DeltaTime)>;
	/** Runs after PollEvents, before camera/input handling (level UI, etc.). */
	using FPreInputCallback = TFunction<void()>;
	/** Runs after the 3D + stats HUD pass (level browser chrome, etc.). */
	using FPostRenderCallback = TFunction<void(int32 FbWidth, int32 FbHeight)>;

	UGameEngine();
	~UGameEngine() override;

	UGameEngine(const UGameEngine&) = delete;
	UGameEngine& operator=(const UGameEngine&) = delete;

	bool Initialize(int32 Width, int32 Height, const TCHAR* Title);
	/** No window / RHI — CPU meshes only. For dedicated servers. */
	bool InitializeHeadless();
	void Shutdown();

	/** Announces the play controls (UE: UGameEngine::Start). Call once before the first Tick. */
	void Start();
	/**
	 * One windowed frame (UE: UGameEngine::Tick): input, update hook, HUD, render, present.
	 * Returns false once the engine should stop (window closed or RequestQuit).
	 */
	bool Tick(float DeltaTime, const FUpdateCallback& OnUpdate = {}, const FPreInputCallback& OnPreInput = {},
		const FPostRenderCallback& OnPostRender = {});
	/** Start + Tick until stopped (standalone loops; FEngineLoop drives Tick itself). */
	void Run(const FUpdateCallback& OnUpdate = {}, const FPreInputCallback& OnPreInput = {},
		const FPostRenderCallback& OnPostRender = {});
	/** Fixed-timestep simulation loop (no render / swap). */
	void RunHeadless(const FUpdateCallback& OnUpdate, float TickHz = 60.0f);

	/** Editor PIE / custom loops: same audio listener + device tick as Run. */
	void TickPlayAudio();
	/** Editor PIE / custom loops: HUD widget tick + on-screen messages + optional F4 stats. */
	void TickPlayHud(float DeltaTime);
	/** Editor PIE / custom loops: paint HUD widgets + FDebugOverlay into a canvas and draw it (after the scene). */
	void PaintHudAndOverlay(int32 FramebufferWidth, int32 FramebufferHeight);
	/** Paints the HUD's widgets and the debug text into Canvas (UE: the viewport client drawing the HUD). */
	void PaintHudAndOverlay(FCanvas& Canvas);
	/** Saves the next rendered frame as a 24-bit .bmp (UE: FScreenshotRequest). */
	void RequestScreenshot(const FString& Path)
	{
		PendingScreenshotPath = Path;
	}

	void RequestQuit()
	{
		bRunning = false;
	}

	/**
	 * Collects garbage once gc.TimeBetweenPurgingPendingKillObjects has passed (UE: UEngine::ConditionalCollectGarbage,
	 * [/Script/Engine.GarbageCollectionSettings] of the engine config). Tick calls it after the world ticked (a safe
	 * point, plan decision D11); a headless loop calls it after its step. Returns true when it collected.
	 */
	bool ConditionalCollectGarbage(float DeltaSeconds);
	[[nodiscard]] bool IsHeadless() const
	{
		return bHeadless;
	}
	[[nodiscard]] bool IsRunning() const
	{
		return bRunning;
	}

	/** The game world (the game instance's world context). */
	[[nodiscard]] UWorld* GetWorld() const;

	/** The game world's persistent level: the loaded .llev content the renderer draws. */
	[[nodiscard]] ULevel& GetLevel();
	[[nodiscard]] const ULevel& GetLevel() const;
	[[nodiscard]] FResourceCache& GetResources()
	{
		return Resources;
	}
	[[nodiscard]] const FResourceCache& GetResources() const
	{
		return Resources;
	}
	[[nodiscard]] UCameraComponent& GetCamera()
	{
		return *Camera;
	}
	[[nodiscard]] const UCameraComponent& GetCamera() const
	{
		return *Camera;
	}
	[[nodiscard]] FGenericWindow& GetWindow()
	{
		return *Window;
	}
	[[nodiscard]] const FGenericWindow& GetWindow() const
	{
		return *Window;
	}
	[[nodiscard]] UPlayerInput& GetInput()
	{
		return *PlayerInput;
	}
	[[nodiscard]] const UPlayerInput& GetInput() const
	{
		return *PlayerInput;
	}
	/** What the view draws besides the scene: the bounds (F1) and the axes gizmo (F6) (UE: EngineShowFlags). */
	[[nodiscard]] FEngineShowFlags& GetEngineShowFlags()
	{
		return EngineShowFlags;
	}
	[[nodiscard]] const FEngineShowFlags& GetEngineShowFlags() const
	{
		return EngineShowFlags;
	}
	/** F6: 1 m world axes and the view orientation gizmo (EngineShowFlags.AxesGizmo); off by default. */
	void SetAxesGizmoEnabled(bool bEnabled)
	{
		EngineShowFlags.AxesGizmo = bEnabled;
	}
	[[nodiscard]] bool IsAxesGizmoEnabled() const
	{
		return EngineShowFlags.AxesGizmo;
	}
	[[nodiscard]] FAudioDevice& GetAudioDevice()
	{
		return AudioDevice;
	}
	[[nodiscard]] const FAudioDevice& GetAudioDevice() const
	{
		return AudioDevice;
	}
	[[nodiscard]] bool IsInitialized() const
	{
		return bInitialized;
	}

	[[nodiscard]] UGameInstance& GetGameInstance()
	{
		return *GameInstance;
	}
	[[nodiscard]] const UGameInstance& GetGameInstance() const
	{
		return *GameInstance;
	}

	/** Replaces the game instance with a new T and its own world (the old world is destroyed). */
	template <typename T>
	T* SetGameInstance()
	{
		static_assert(TIsDerivedFrom<T, UGameInstance>::Value, "T must derive from GameInstance");
		T* NewInstance = NewObject<T>(GetTransientPackage());
		SetGameInstanceObject(NewInstance);
		return NewInstance;
	}

	/** When true, mouse look / orbit are paused (level browser chrome, etc.). */
	void SetSuppressCameraDrag(bool bSuppress)
	{
		bSuppressCameraDrag = bSuppress;
	}
	[[nodiscard]] bool IsCameraDragSuppressed() const
	{
		return bSuppressCameraDrag;
	}

	/** Capture + hide OS cursor for continuous mouse look (enabled by default for all levels). */
	void SetCursorCaptured(bool bCaptured);
	[[nodiscard]] bool IsCursorCaptured() const;

	/**
	 * Optional secondary window for PIE "New Window" input / cursor capture.
	 * Prefer GetPlayInputTarget() when configuring multiple fields; these remain the
	 * Unreal-like convenience API used by GameMode / APlayerController.
	 */
	void SetPlayInputWindow(FGenericWindow* InWindow);
	[[nodiscard]] FGenericWindow& GetPlayInputWindow();
	[[nodiscard]] const FGenericWindow& GetPlayInputWindow() const;

	/** Grouped PIE / multi-window play input state (window override + mouse-look gate). */
	[[nodiscard]] FPlayInputTarget& GetPlayInputTarget()
	{
		return PlayInputTarget;
	}
	[[nodiscard]] const FPlayInputTarget& GetPlayInputTarget() const
	{
		return PlayInputTarget;
	}

	/**
	 * Editor PIE: when cursor is not OS-captured (Selected Viewport), mouse look only applies
	 * while this is true (typically Viewport hovered / play window focused).
	 */
	void SetPlayMouseLookActive(bool bActive)
	{
		PlayInputTarget.SetMouseLookActive(bActive);
	}
	[[nodiscard]] bool IsPlayMouseLookActive() const
	{
		return PlayInputTarget.IsMouseLookActive();
	}

	/** When false, WASD/arrows do not tumble the orbit camera (gameplay may use them). */
	void SetKeyboardOrbitEnabled(bool bEnabled)
	{
		bKeyboardOrbitEnabled = bEnabled;
	}

	/** When false, Engine mouse orbit + scroll→camera zoom are off (games may drive SpringArm). */
	void SetOrbitMouseEnabled(bool bEnabled)
	{
		bOrbitMouseEnabled = bEnabled;
	}

	/** F2 collision volumes debug (Engine tool flag, not a renderer show flag). */
	void SetCollisionDebugEnabled(bool bEnabled)
	{
		bCollisionDebugEnabled = bEnabled;
	}
	void ToggleCollisionDebug()
	{
		bCollisionDebugEnabled = !bCollisionDebugEnabled;
	}
	[[nodiscard]] bool IsCollisionDebugEnabled() const
	{
		return bCollisionDebugEnabled;
	}

	/** F3 NavMesh grid debug (walkable / blocked cells). */
	void SetNavMeshDebugEnabled(bool bEnabled)
	{
		bNavMeshDebugEnabled = bEnabled;
	}
	void ToggleNavMeshDebug()
	{
		bNavMeshDebugEnabled = !bNavMeshDebugEnabled;
	}
	[[nodiscard]] bool IsNavMeshDebugEnabled() const
	{
		return bNavMeshDebugEnabled;
	}

	/**
	 * Consume accumulated mouse-wheel Y this frame (platform wheel units). Cleared after return.
	 * When orbit mouse is enabled, Engine applies scroll to Orbit distance in handleInput first.
	 */
	[[nodiscard]] float ConsumeScrollY();

	/** Unreal-like Print String / AddOnScreenDebugMessage (top-left console; default red). */
	void AddOnScreenDebugMessage(const FString& Message, float DisplaySeconds = 2.0f,
		const FLinearColor& Color = FLinearColor(1.0f, 0.0f, 0.0f));

	/** Persistent top-center HUD line (cleared when empty). Games update each Tick. */
	void SetCenterHudText(FString Text)
	{
		CenterHudText = MoveTemp(Text);
	}
	void ClearCenterHudText()
	{
		CenterHudText.Empty();
	}

	/** Runtime FPS / RAM / TRI overlay (F4). Off by default so Shipping matches Viewport / PIE. */
	void SetHudStatsVisible(bool bVisible);
	[[nodiscard]] bool IsHudStatsVisible() const
	{
		return bShowHudStats;
	}

	/** Unreal-like AHUD (UserWidgets / crosshair, etc.). */
	[[nodiscard]] AHUD& GetHUD()
	{
		return *Hud;
	}
	[[nodiscard]] const AHUD& GetHUD() const
	{
		return *Hud;
	}

	// FGCObject
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	FString GetReferencerName() const override
	{
		return TEXT("UGameEngine");
	}

private:
	void SetGameInstanceObject(UGameInstance* NewInstance);
	/** Destroys the game instance's world and collects garbage (world teardown is a safe point). */
	void DestroyGameWorld();
	[[nodiscard]] EShaderReloadResult ReloadAllShaders(bool bForce);
	void HandleInput(float DeltaTime);
	void Render(const FPostRenderCallback& OnPostRender);
	void WritePendingScreenshot();
	void UpdateHudStats(float DeltaTime);

	TUniquePtr<GenericApplication> Application;
	TSharedPtr<FGenericWindow> Window;
	FPlayInputTarget PlayInputTarget;
	/** The player's input (UE keeps it on the player controller; P13 moves it there with input by config). */
	UPlayerInput* PlayerInput = nullptr;
	/** The view's show flags (UE: the viewport client's EngineShowFlags). */
	FEngineShowFlags EngineShowFlags;
	/** The on-screen debug text, drawn into each frame's canvas. */
	FDebugOverlay Overlay;
	/** The HUD, outside any world (UE spawns one per player controller; P13). */
	AHUD* Hud = nullptr;
	FAudioDevice AudioDevice;
	/** The view camera (a standalone component until P13's player camera manager). */
	UCameraComponent* Camera = nullptr;
	FResourceCache Resources;
	/** Times the periodic garbage collection (UE: TimeSinceLastPendingKillPurge). */
	FGarbageCollectionTimer GarbageCollectionTimer;

	/** The game session; its world context holds the game world (UE: UGameEngine::GameInstance). */
	UGameInstance* GameInstance = nullptr;

	bool bRunning = false;
	bool bInitialized = false;
	bool bHeadless = false;
	bool bSuppressCameraDrag = false;
	bool bKeyboardOrbitEnabled = true;
	bool bOrbitMouseEnabled = true;
	float PendingScrollY = 0.0f;
	FString CenterHudText;
	FString PendingScreenshotPath;

	bool bMouseLookSampleValid = false;
	bool bDebugKeyWasDown = false;
	bool bCollisionDebugEnabled = false;
	bool bCollisionDebugKeyWasDown = false;
	bool bNavMeshDebugEnabled = false;
	bool bNavMeshDebugKeyWasDown = false;
	bool bReloadKeyWasDown = false;
	bool bAxesGizmoKeyWasDown = false;
	bool bShowHudStats = false;
	bool bHudStatsKeyWasDown = false;
	double LastMouseX = 0.0;
	double LastMouseY = 0.0;

	int32 LastFbWidth = 0;
	int32 LastFbHeight = 0;

	float FpsAccumTime = 0.0f;
	int32 FpsAccumFrames = 0;
	float DisplayFps = 0.0f;
	float DisplayMs = 0.0f;
};
