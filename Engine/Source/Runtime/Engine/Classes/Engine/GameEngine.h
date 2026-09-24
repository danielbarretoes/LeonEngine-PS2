#pragma once

#include "AudioDevice.h"
#include "Camera/CameraComponent.h"
#include "Debug/DebugOverlay.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "GameFramework/HUD.h"
#include "GameFramework/InputMapping.h"
#include "GameFramework/PlayInputTarget.h"
#include "GenericPlatform/GenericApplication.h"
#include "ResourceCache.h"
#include "SceneRenderer.h"

#include <glm/vec3.hpp>

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

/// Top-level runtime: GLFW window, main loop, orbit-camera input, FPS overlay,
/// UGameInstance, and a Level/FResourceCache filled by FLevelDirector (or the app).
class ENGINE_API UGameEngine
{
public:
	using FUpdateCallback = std::function<void(float DeltaTime)>;
	/// Runs after PollEvents, before camera/input handling (level UI, etc.).
	using FPreInputCallback = std::function<void()>;
	/// Runs after the 3D + stats HUD pass (level browser chrome, etc.).
	using FPostRenderCallback = std::function<void(int FbWidth, int FbHeight)>;

	UGameEngine();
	~UGameEngine();

	UGameEngine(const UGameEngine&) = delete;
	UGameEngine& operator=(const UGameEngine&) = delete;

	bool Initialize(int Width, int Height, const char* Title);
	/// No GLFW / OpenGL — CPU meshes only. For `leon-server`.
	bool InitializeHeadless();
	void Shutdown();

	/// Announces the play controls (UE: UGameEngine::Start). Call once before the first Tick.
	void Start();
	/// One windowed frame (UE: UGameEngine::Tick): input, update hook, HUD, render, present.
	/// Returns false once the engine should stop (window closed or RequestQuit).
	bool Tick(float DeltaTime, const FUpdateCallback& OnUpdate = {}, const FPreInputCallback& OnPreInput = {},
		const FPostRenderCallback& OnPostRender = {});
	/// Start + Tick until stopped (standalone loops; FEngineLoop drives Tick itself).
	void Run(const FUpdateCallback& OnUpdate = {}, const FPreInputCallback& OnPreInput = {},
		const FPostRenderCallback& OnPostRender = {});
	/// Fixed-timestep simulation loop (no render / swap).
	void RunHeadless(const FUpdateCallback& OnUpdate, float TickHz = 60.0f);

	/// Editor PIE / custom loops: same audio listener + device tick as `Run`.
	void TickPlayAudio();
	/// Editor PIE / custom loops: HUD widget tick + on-screen messages + optional F4 stats.
	void TickPlayHud(float DeltaTime);
	/// Editor PIE / custom loops: paint HUD widgets + FDebugOverlay (call after DrawScene).
	void PaintHudAndOverlay(int FramebufferWidth, int FramebufferHeight);

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
		static_assert(std::is_base_of_v<UGameInstance, T>, "T must derive from GameInstance");
		// Packs call SetGameInstance after Runtime wires travel/browser callbacks — keep them.
		UGameInstance::FLevelTravelFunction TravelFn;
		UGameInstance::FLevelBrowserVisibleFunction BrowserFn;
		if (GameInstance)
		{
			TravelFn = GameInstance->TakeLevelTravelFn();
			BrowserFn = GameInstance->TakeLevelBrowserVisibleFn();
			if (bInitialized)
			{
				GameInstance->Shutdown();
			}
		}
		auto Owned = std::make_unique<T>(std::forward<ArgsType>(Args)...);
		T* Raw = Owned.get();
		GameInstance = std::move(Owned);
		if (TravelFn)
		{
			GameInstance->SetLevelTravelFn(std::move(TravelFn));
		}
		if (BrowserFn)
		{
			GameInstance->SetLevelBrowserVisibleFn(std::move(BrowserFn));
		}
		if (bInitialized)
		{
			GameInstance->Init();
		}
		return Raw;
	}

	/// When true, mouse look / orbit are paused (level browser chrome, etc.).
	void SetSuppressCameraDrag(bool bSuppress)
	{
		bSuppressCameraDrag = bSuppress;
	}
	[[nodiscard]] bool IsCameraDragSuppressed() const
	{
		return bSuppressCameraDrag;
	}

	/// Capture + hide OS cursor for continuous mouse look (enabled by default for all levels).
	void SetCursorCaptured(bool bCaptured);
	[[nodiscard]] bool IsCursorCaptured() const;

	/// Optional secondary window for PIE "New Window" input / cursor capture.
	/// Prefer `GetPlayInputTarget()` when configuring multiple fields; these remain the
	/// Unreal-like convenience API used by GameMode / APlayerController.
	void SetPlayInputWindow(FGenericWindow* InWindow);
	[[nodiscard]] FGenericWindow& GetPlayInputWindow();
	[[nodiscard]] const FGenericWindow& GetPlayInputWindow() const;

	/// Grouped PIE / multi-window play input state (window override + mouse-look gate).
	[[nodiscard]] FPlayInputTarget& GetPlayInputTarget()
	{
		return PlayInputTarget;
	}
	[[nodiscard]] const FPlayInputTarget& GetPlayInputTarget() const
	{
		return PlayInputTarget;
	}

	/// Editor PIE: when cursor is not OS-captured (Selected Viewport), mouse look only applies
	/// while this is true (typically Viewport hovered / play window focused).
	void SetPlayMouseLookActive(bool bActive)
	{
		PlayInputTarget.SetMouseLookActive(bActive);
	}
	[[nodiscard]] bool IsPlayMouseLookActive() const
	{
		return PlayInputTarget.IsMouseLookActive();
	}

	/// When false, WASD/arrows do not tumble the orbit camera (gameplay may use them).
	void SetKeyboardOrbitEnabled(bool bEnabled)
	{
		bKeyboardOrbitEnabled = bEnabled;
	}

	/// When false, Engine mouse orbit + scroll→camera zoom are off (packs may drive SpringArm).
	void SetOrbitMouseEnabled(bool bEnabled)
	{
		bOrbitMouseEnabled = bEnabled;
	}

	/// F2 collision volumes debug (Engine tool flag — not owned by the forward FSceneRenderer).
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

	/// F3 NavMesh grid debug (walkable / blocked cells).
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

	/// Consume accumulated mouse-wheel Y this frame (GLFW units). Cleared after return.
	/// When orbit mouse is enabled, Engine applies scroll to Orbit distance in handleInput first.
	[[nodiscard]] float ConsumeScrollY();

	/// Unreal-like Print String / AddOnScreenDebugMessage (top-left console; default red).
	void AddOnScreenDebugMessage(
		std::string Message, float DisplaySeconds = 2.0f, const glm::vec3& Color = {1.0f, 0.0f, 0.0f});

	/// Persistent top-center HUD line (cleared when empty). Packs update each Tick.
	void SetCenterHudText(std::string Text)
	{
		CenterHudText = std::move(Text);
	}
	void ClearCenterHudText()
	{
		CenterHudText.clear();
	}

	/// Runtime FPS / RAM / TRI overlay (F4). Off by default so Shipping matches Viewport / PIE.
	void SetHudStatsVisible(bool bVisible);
	[[nodiscard]] bool IsHudStatsVisible() const
	{
		return bShowHudStats;
	}

	/// Unreal-like AHUD (UserWidgets / crosshair, etc.).
	[[nodiscard]] AHUD& GetHUD()
	{
		return Hud;
	}
	[[nodiscard]] const AHUD& GetHUD() const
	{
		return Hud;
	}

	/// Optional extra shader reload (level chrome, etc.) merged into F5 / auto-reload.
	using FShaderReloadHook = std::function<EShaderReloadResult(bool bForce)>;
	void SetShaderReloadHook(FShaderReloadHook Hook)
	{
		ShaderReloadHook = std::move(Hook);
	}

private:
	[[nodiscard]] EShaderReloadResult ReloadAllShaders(bool bForce);
	void HandleInput(float DeltaTime);
	void Render(const FPostRenderCallback& OnPostRender);
	void UpdateHudStats(float DeltaTime);

	std::unique_ptr<GenericApplication> Application;
	std::unique_ptr<FGenericWindow> Window;
	FPlayInputTarget PlayInputTarget;
	UPlayerInput PlayerInput;
	FSceneRenderer Renderer;
	FDebugOverlay Overlay;
	AHUD Hud;
	FAudioDevice AudioDevice;
	UCameraComponent Camera;
	ULevel Level;
	FResourceCache Resources;
	std::unique_ptr<UGameInstance> GameInstance;

	bool bRunning = false;
	bool bInitialized = false;
	bool bHeadless = false;
	bool bSuppressCameraDrag = false;
	bool bKeyboardOrbitEnabled = true;
	bool bOrbitMouseEnabled = true;
	float PendingScrollY = 0.0f;
	std::string CenterHudText;

	bool bMouseLookSampleValid = false;
	bool bDebugKeyWasDown = false;
	bool bCollisionDebugEnabled = false;
	bool bCollisionDebugKeyWasDown = false;
	bool bNavMeshDebugEnabled = false;
	bool bNavMeshDebugKeyWasDown = false;
	bool bReloadKeyWasDown = false;
	bool bShowHudStats = false;
	bool bHudStatsKeyWasDown = false;
	FShaderReloadHook ShaderReloadHook;
	double LastMouseX = 0.0;
	double LastMouseY = 0.0;

	int LastFbWidth = 0;
	int LastFbHeight = 0;

	float FpsAccumTime = 0.0f;
	int FpsAccumFrames = 0;
	float DisplayFps = 0.0f;
	float DisplayMs = 0.0f;
};
