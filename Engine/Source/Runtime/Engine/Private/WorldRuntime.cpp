#include "WorldRuntime.h"

#include <iostream>
#include "Engine/GameEngine.h"


bool FWorldRuntime::Initialize(UGameEngine& engine, const std::string& shaderDirectory) {
    // Headless dedicated servers have no GL context — skip overlay chrome/shaders.
    if (engine.IsHeadless()) {
        return true;
    }
    if (!director_.Initialize(shaderDirectory)) {
        return false;
    }
    engine.SetShaderReloadHook([this](bool force) { return director_.ReloadShaders(force); });
    return true;
}

bool FWorldRuntime::LoadPack(UGameEngine& engine, const std::string& packDirectory,
                            std::string_view preferredLevelKey) {
    return director_.ScanPackAndLoad(engine, packDirectory, preferredLevelKey);
}

void FWorldRuntime::Tick(UGameEngine& engine, FGameplayRouter& gameplay, float deltaTime) {
    director_.Update(engine, deltaTime);
    gameplay.Update(engine, director_, deltaTime);
}

void FWorldRuntime::HandleUiInput(UGameEngine& engine) {
    const bool blockDrag = director_.HandleUiInput(engine);
    engine.SetSuppressCameraDrag(blockDrag);
}

void FWorldRuntime::DrawUi(int framebufferWidth, int framebufferHeight) {
    director_.DrawUi(framebufferWidth, framebufferHeight);
}

void FWorldRuntime::Shutdown() {
    director_.Shutdown();
}

