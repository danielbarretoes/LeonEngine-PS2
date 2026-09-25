#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ShaderCore.h"
#include "GameEngine.generated.h"

class UWorld;

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
	/** The audio listener follows the view camera. */
	void TickPlayAudio();
};
