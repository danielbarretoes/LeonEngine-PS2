#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"
#include "Misc/Exec.h"
#include "SceneView.h"
#include "Templates/UniquePtr.h"
#include "UObject/Object.h"
#include "UnrealClient.h"
#include "GameViewportClient.generated.h"

class APlayerController;
class FCanvas;
class FGenericWindow;
class UCameraComponent;
class UGameInstance;
class ULocalPlayer;
class UWorld;
struct FWorldContext;

/**
 * The game's view (UE: UGameViewportClient), created by UGameEngine::Init (`[/Script/Engine.Engine]
 * GameViewportClientClassName=`) for the game instance's world context.
 *
 * - Input: each frame (ProcessInput, Leon's stand-in for Slate's input events) the window's key changes become
 *   InputKey events and the mouse's motion MouseX / MouseY InputAxis samples, routed to the first local player's
 *   controller.
 * - Draw: the player's camera view of the world's scene (FSceneViewFamily, IRendererModule::BeginRenderingViewFamily),
 *   then the player's HUD and the engine's on-screen text into the frame's canvas.
 * - Exec: `show <Flag>` toggles EngineShowFlags, then the game instance, then the engine (UE's chain). The console
 *   reaches it through ULocalPlayer::Exec; UPlayerInput's DebugExecBindings (F1-F6) and `-ExecCmds=` too.
 * - Without a window (`-nullrhi`, tests) it only keeps the local player and the show flags.
 */
UCLASS(Transient, Config = Engine)
class ENGINE_API UGameViewportClient
	: public UObject
	, public FExec
{
	GENERATED_BODY()

public:
	UGameViewportClient(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	~UGameViewportClient() override;

	/** What the view draws besides the scene (UE: EngineShowFlags). */
	FEngineShowFlags EngineShowFlags;

	/** Takes the world context and its game instance (UE: Init). */
	virtual void Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance);

	/** Shows the view in a window (Leon: UE's CreateGameViewport makes the Slate viewport); null for none. */
	void SetViewportWindow(FGenericWindow* InWindow);

	/** The first local player (UE: SetupInitialLocalPlayer). */
	virtual ULocalPlayer* SetupInitialLocalPlayer(FString& OutError);

	/** A key event for a player (UE: InputKey); true when the player's input took it. */
	virtual bool InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent EventType,
		float AmountDepressed = 1.0f, bool bGamepad = false);

	/** An axis sample for a player (UE: InputAxis). */
	virtual bool InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime,
		int32 NumSamples = 1, bool bGamepad = false);

	/** Polls the window's keys and mouse into InputKey / InputAxis (Leon: Slate delivers these in UE). */
	virtual void ProcessInput(float DeltaTime);

	/** After the world ticked: the on-screen messages and the stats text (UE: Tick). */
	virtual void Tick(float DeltaTime);

	/** Draws the view and the HUD into the frame's canvas (UE: Draw). */
	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas);

	/** Saves the frame when a screenshot was requested (UE: ProcessScreenShots; Leon writes a 24-bit .bmp). */
	virtual bool ProcessScreenShots(FViewport* InViewport);

	/** `show <Flag>`, then the game instance's and the engine's commands (UE: UGameViewportClient::Exec). */
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/** Captures the mouse or lets it go (UE: SetMouseCaptureMode / the viewport's capture). */
	void SetMouseCaptureMode(EMouseCaptureMode Mode);
	[[nodiscard]] bool IsCursorCaptured() const;

	/** The game instance's world (UE: GetWorld). */
	[[nodiscard]] UWorld* GetWorld() const;
	[[nodiscard]] UGameInstance* GetGameInstance() const
	{
		return GameInstance;
	}
	/** The viewport, or null without a window (UE: Viewport). */
	[[nodiscard]] FViewport* GetGameViewport() const
	{
		return Viewport.Get();
	}
	/** The window, or null. */
	[[nodiscard]] FGenericWindow* GetWindow() const;

	/** The camera the view is drawn with: the first local player's camera, else a default one. */
	[[nodiscard]] UCameraComponent* GetViewCamera() const;

	/** The first local player's controller in the world, or null. */
	[[nodiscard]] APlayerController* GetFirstLocalPlayerController() const;

protected:
	/** `show [Flag]` (UE: HandleShowCommand). */
	bool HandleShowCommand(const TCHAR* Cmd, FOutputDevice& Ar);

private:
	/** Refreshes the stats text a few times a second while it is visible. */
	void UpdateHudStats(float DeltaTime);

	/** The game instance the view belongs to (UE: GameInstance). */
	UPROPERTY(Transient)
	UGameInstance* GameInstance = nullptr;

	/** The view when no player has a camera. */
	UPROPERTY(Transient)
	UCameraComponent* DefaultViewCamera = nullptr;

	TUniquePtr<FViewport> Viewport;
	/** The keys the window reported down last frame. */
	TSet<FKey> DownKeys;
	bool bMouseLookSampleValid = false;
	double LastMouseX = 0.0;
	double LastMouseY = 0.0;

	bool bStatsVisibleLastTick = false;
	float FpsAccumTime = 0.0f;
	int32 FpsAccumFrames = 0;
	float DisplayFps = 0.0f;
	float DisplayMs = 0.0f;
};
