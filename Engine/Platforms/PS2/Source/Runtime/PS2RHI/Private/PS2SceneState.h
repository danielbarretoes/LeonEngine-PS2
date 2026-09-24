#pragma once

#include "PS2RHI.h"

namespace Leon::PS2 {

struct FPS2SceneState {
    FPS2ViewTarget ViewTarget{};
    FPS2DirectionalLight Sun{};
    float AmbientR = 0.20f;
    float AmbientG = 0.22f;
    float AmbientB = 0.28f;
    FPS2Material BoundMaterial{};
    /// Last TEX0 VRAM address bound this frame (-1 = none). Avoids redundant Bind().
    int BoundTextureVram = -1;
    bool bViewDirty = true;
};

[[nodiscard]] FPS2SceneState& GetSceneState();

void InvalidateBoundTexture();

} // namespace Leon::PS2
