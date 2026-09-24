#pragma once

#include "Ps2RHI.h"

namespace leon::rhi::ps2 {

struct SceneState {
    Ps2ViewTarget ViewTarget{};
    DirectionalLight Sun{};
    float AmbientR = 0.20f;
    float AmbientG = 0.22f;
    float AmbientB = 0.28f;
    Ps2Material BoundMaterial{};
    /// Last TEX0 VRAM address bound this frame (-1 = none). Avoids redundant Bind().
    int BoundTextureVram = -1;
    bool ViewDirty = true;
};

[[nodiscard]] SceneState& GetSceneState();

void InvalidateBoundTexture();

} // namespace leon::rhi::ps2
