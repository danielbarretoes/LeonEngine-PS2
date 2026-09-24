#include <leon/runtime/WorldRuntime.h>

#include <iostream>
#include <leon/Engine.h>

namespace leon::runtime {

bool WorldRuntime::Initialize(Engine& engine, const std::string& shaderDirectory) {
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

bool WorldRuntime::LoadPack(Engine& engine, const std::string& packDirectory,
                            std::string_view preferredLevelKey) {
    return director_.ScanPackAndLoad(engine, packDirectory, preferredLevelKey);
}

void WorldRuntime::Tick(Engine& engine, GameplayRouter& gameplay, float deltaTime) {
    director_.Update(engine, deltaTime);
    gameplay.Update(engine, director_, deltaTime);
}

void WorldRuntime::HandleUiInput(Engine& engine) {
    const bool blockDrag = director_.HandleUiInput(engine);
    engine.SetSuppressCameraDrag(blockDrag);
}

void WorldRuntime::DrawUi(int framebufferWidth, int framebufferHeight) {
    director_.DrawUi(framebufferWidth, framebufferHeight);
}

void WorldRuntime::Shutdown() {
    director_.Shutdown();
}

} // namespace leon::runtime
