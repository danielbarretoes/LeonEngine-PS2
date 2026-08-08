#include "Ps2SceneState.h"

namespace leon::rhi::ps2 {

SceneState& GetSceneState() {
    static SceneState state{};
    return state;
}

void InvalidateBoundTexture() {
    GetSceneState().BoundTextureVram = -1;
}

} // namespace leon::rhi::ps2

namespace leon::rhi {

void Ps2SetViewTarget(const Ps2ViewTarget& viewTarget) {
    auto& s = ps2::GetSceneState();
    s.ViewTarget = viewTarget;
    s.ViewDirty = true;
}

void Ps2SetDirectionalLight(const DirectionalLight& light) {
    ps2::GetSceneState().Sun = light;
}

void Ps2SetAmbientLightColor(float r, float g, float b) {
    auto& s = ps2::GetSceneState();
    s.AmbientR = r;
    s.AmbientG = g;
    s.AmbientB = b;
}

void Ps2BindMaterial(const Ps2Material& material) {
    ps2::GetSceneState().BoundMaterial = material;
}

} // namespace leon::rhi
