#include "PS2SceneState.h"

namespace Leon::PS2 {

FPS2SceneState& GetSceneState() {
    static FPS2SceneState state{};
    return state;
}

void InvalidateBoundTexture() {
    GetSceneState().BoundTextureVram = -1;
}

} // namespace Leon::PS2


void FPS2RHI::SetViewTarget(const FPS2ViewTarget& viewTarget) {
    auto& s = Leon::PS2::GetSceneState();
    s.ViewTarget = viewTarget;
    s.ViewDirty = true;
}

void FPS2RHI::SetDirectionalLight(const FPS2DirectionalLight& light) {
    Leon::PS2::GetSceneState().Sun = light;
}

void FPS2RHI::SetAmbientLightColor(float r, float g, float b) {
    auto& s = Leon::PS2::GetSceneState();
    s.AmbientR = r;
    s.AmbientG = g;
    s.AmbientB = b;
}

void FPS2RHI::BindMaterial(const FPS2Material& material) {
    Leon::PS2::GetSceneState().BoundMaterial = material;
}

