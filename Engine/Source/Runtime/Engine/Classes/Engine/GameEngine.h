#pragma once

#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "GameFramework/HUD.h"
#include "GameFramework/InputMapping.h"
#include "GenericPlatform/GenericApplication.h"
#include "SceneView.h"
#include "ShaderCore.h"
#include "GameEngine.generated.h"

class UWorld;

/**
 * The engine of a game (UE: UGameEngine), GEngine's class by default (`[/Script/Engine.Engine]
 * GameEngine=/Script/Engine.GameEngine`).
 *
 * - Init creates the game window and starts the renderer on its context (unless headless), then the game instance
 *   (GameInstanceClass of UGameMapsSettings) with its world context and its first local player.
 * - Start: the game instance opens the first map (UGameInstance::StartGameInstance → Browse → LoadMap).
 * - Tick: a frame (input, the world, the garbage collection timer, the HUD, the render, the present).
 * - PreExit: the game instance shuts down, the world goes (a garbage collection safe point, plan decision D11), then
 *   the renderer and the window.
 *
 * It draws through the Renderer module's interface (IRendererModule, found by name); Engine never includes a Renderer
 * header. Until the second stage of P13 it still owns the view camera, the HUD and the player input.
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

	/** Saves the next rendered frame as a 24-bit .bmp (UE: FScreenshotRequest). */
	void RequestScreenshot(const FString& Path)
	{
		PendingScreenshotPath = Path;
	}

	/** The game window, or null when headless. */
	[[nodiscard]] FGenericWindow* GetWindow() const
	{
		return Window.Get();
	}

	/** The view camera (a standalone component until the player camera manager). */
	[[nodiscard]] UCameraComponent& GetCamera()
	{
		return *Camera;
	}
	[[nodiscard]] UPlayerInput& GetInput()
	{
		return *PlayerInput;
	}
	[[nodiscard]] AHUD& GetHUD()
	{
		return *Hud;
	}

	/** What the view draws besides the scene: the bounds (F1) and the axes gizmo (F6) (UE: EngineShowFlags). */
	[[nodiscard]] FEngineShowFlags& GetEngineShowFlags()
	{
		return EngineShowFlags;
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

	/** Capture + hide the OS cursor for continuous mouse look. */
	void SetCursorCaptured(bool bCaptured);
	[[nodiscard]] bool IsCursorCaptured() const;

private:
	/** Destroys the game instance's world and collects garbage (world teardown is a safe point). */
	void DestroyGameWorld();
	[[nodiscard]] EShaderReloadResult ReloadAllShaders(bool bForce);
	void HandleInput(float DeltaTime);
	void TickPlayAudio();
	void TickPlayHud(float DeltaTime);
	/** Paints the HUD's widgets and the debug text into Canvas (UE: the viewport client drawing the HUD). */
	void PaintHudAndOverlay(FCanvas& Canvas);
	void Render();
	void WritePendingScreenshot();
	void UpdateHudStats(float DeltaTime);

	TUniquePtr<GenericApplication> Application;
	TSharedPtr<FGenericWindow> Window;
	/** The player's input (UE keeps it on the player controller; the second stage of P13 moves it there). */
	UPROPERTY(Transient)
	UPlayerInput* PlayerInput = nullptr;
	/** The HUD, outside any world (UE spawns one per player controller). */
	UPROPERTY(Transient)
	AHUD* Hud = nullptr;
	/** The view camera (a standalone component until the player camera manager). */
	UPROPERTY(Transient)
	UCameraComponent* Camera = nullptr;
	/** The view's show flags (UE: the viewport client's EngineShowFlags). */
	FEngineShowFlags EngineShowFlags;

	FString PendingScreenshotPath;

	bool bMouseLookSampleValid = false;
	bool bDebugKeyWasDown = false;
	bool bCollisionDebugEnabled = false;
	bool bCollisionDebugKeyWasDown = false;
	bool bNavMeshDebugEnabled = false;
	bool bNavMeshDebugKeyWasDown = false;
	bool bReloadKeyWasDown = false;
	bool bAxesGizmoKeyWasDown = false;
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
