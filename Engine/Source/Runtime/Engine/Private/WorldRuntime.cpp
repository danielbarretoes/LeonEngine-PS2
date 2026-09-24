#include "WorldRuntime.h"

#include <iostream>
#include "Engine/GameEngine.h"


bool FWorldRuntime::Initialize(UGameEngine& Engine, const std::string& ShaderDirectory) {
    // Headless dedicated servers have no GL context — skip overlay chrome/shaders.
    if (Engine.IsHeadless()) {
        return true;
    }
    if (!Director.Initialize(ShaderDirectory)) {
        return false;
    }
    Engine.SetShaderReloadHook([this](bool bForce) { return Director.ReloadShaders(bForce); });
    return true;
}

bool FWorldRuntime::LoadPack(UGameEngine& Engine, const std::string& PackDirectory,
                            std::string_view PreferredLevelKey) {
    return Director.ScanPackAndLoad(Engine, PackDirectory, PreferredLevelKey);
}

void FWorldRuntime::Tick(UGameEngine& Engine, FGameplayRouter& Gameplay, float DeltaTime) {
    Director.Update(Engine, DeltaTime);
    Gameplay.Update(Engine, Director, DeltaTime);
}

void FWorldRuntime::HandleUiInput(UGameEngine& Engine) {
    const bool bBlockDrag = Director.HandleUiInput(Engine);
    Engine.SetSuppressCameraDrag(bBlockDrag);
}

void FWorldRuntime::DrawUi(int FramebufferWidth, int FramebufferHeight) {
    Director.DrawUi(FramebufferWidth, FramebufferHeight);
}

void FWorldRuntime::Shutdown() {
    Director.Shutdown();
}

