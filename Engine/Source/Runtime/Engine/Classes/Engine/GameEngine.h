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
#include "SceneRenderer.h"

/**
 * Top-level runtime: platform window (FGenericWindow), per-frame Tick, orbit-camera input, FPS overlay,
 * UGameInstance, and a Level/FResourceCache filled by the level loader.
 */
class ENGINE_API UGameEngine
{
public:
	using FUpdateCallback = TFunction<void(float DeltaTime)>;
	/** Runs after PollEvents, before camera/input handling (level UI, etc.). */
	using FPreInputCallback = TFunction<void()>;
	/** Runs after the 3D + stats HUD pass (level browser chrome, etc.). */
	using FPostRenderCallback = TFunction<void(int32 FbWidth, int32 FbHeight)>;

	UGameEngine();
	~UGameEngine();

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
	/** Editor PIE / custom loops: paint HUD widgets + FDebugOverlay (call after DrawScene). */
	void PaintHudAndOverlay(int32 FramebufferWidth, int32 FramebufferHeight);

	void RequestQuit()
	{
		bRunning = false;
	}
	[[nodiscard]] bool IsHeadless() const
	{
		return bHeadless;
	}
	[[nodiscard]] bool IsRunning() const
	{
		return bRunning;
	}

	[[nodiscard]] ULevel& GetLevel()
	{
		return Level;
	}
	[[nodiscard]] const ULevel& GetLevel() const
	{
		return Level;
	}
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
		return Camera;
	}
	[[nodiscard]] const UCameraComponent& GetCamera() const
	{
		return Camera;
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
		return PlayerInput;
	}
	[[nodiscard]] const UPlayerInput& GetInput() const
	{
		return PlayerInput;
	}
	[[nodiscard]] FSceneRenderer& GetRenderer()
	{
		return Renderer;
	}
	[[nodiscard]] const FSceneRenderer& GetRenderer() const
	{
		return Renderer;
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

	template <typename T, typename... ArgsType>
	T* SetGameInstance(ArgsType&&... Args)
	{
		static_assert(TIsDerivedFrom<T, UGameInstance>::Value, "T must derive from GameInstance");
		if (GameInstance && bInitialized)
		{
			GameInstance->Shutdown();
		}
		auto Owned = MakeUnique<T>(Forward<ArgsType>(Args)...);
		T* Raw = Owned.get();
		GameInstance = MoveTemp(Owned);
		if (bInitialized)
		{
			GameInstance->Init();
		}
		return Raw;
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

	/** F2 collision volumes debug (Engine tool flag — not owned by the forward FSceneRenderer). */
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
		return Hud;
	}
	[[nodiscard]] const AHUD& GetHUD() const
	{
		return Hud;
	}

private:
	[[nodiscard]] EShaderReloadResult ReloadAllShaders(bool bForce);
	void HandleInput(float DeltaTime);
	void Render(const FPostRenderCallback& OnPostRender);
	void UpdateHudStats(float DeltaTime);

	TUniquePtr<GenericApplication> Application;
	TSharedPtr<FGenericWindow> Window;
	FPlayInputTarget PlayInputTarget;
	UPlayerInput PlayerInput;
	FSceneRenderer Renderer;
	FDebugOverlay Overlay;
	AHUD Hud;
	FAudioDevice AudioDevice;
	UCameraComponent Camera;
	ULevel Level;
	FResourceCache Resources;
	TUniquePtr<UGameInstance> GameInstance;

	bool bRunning = false;
	bool bInitialized = false;
	bool bHeadless = false;
	bool bSuppressCameraDrag = false;
	bool bKeyboardOrbitEnabled = true;
	bool bOrbitMouseEnabled = true;
	float PendingScrollY = 0.0f;
	FString CenterHudText;

	bool bMouseLookSampleValid = false;
	bool bDebugKeyWasDown = false;
	bool bCollisionDebugEnabled = false;
	bool bCollisionDebugKeyWasDown = false;
	bool bNavMeshDebugEnabled = false;
	bool bNavMeshDebugKeyWasDown = false;
	bool bReloadKeyWasDown = false;
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
