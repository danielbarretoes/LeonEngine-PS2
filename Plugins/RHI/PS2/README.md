# Plugins/RHI/PS2

Lean Emotion Engine GS backend (`leon_rhi_ps2`).

| File | Role |
| --- | --- |
| `Ps2GsContext` | Framebuffer / z-buffer / packet |
| `Ps2RHIDevice` | `IRHIDevice`, display init, clear / vsync |
| `Ps2SceneState` | ViewTarget, DirectionalLight, ambient, bound material |
| `Ps2DrawPrimitives` | 2D unlit triangle / rect, sin LUT |
| `Ps2Draw3D` | Lit/textured box (`math3d` + `draw3d`) |
| `Ps2Texture` | VRAM upload + TEX0 bind (`T_*_D`) |
| `Ps2DebugHud` | Timer + 5×7 HUD text (drawn by the engine debug overlay, `leon/core/DebugOverlay.h`) |

Public API: `<leon/rhi/Ps2RHI.h>`.

Frame contract: clear → set view/lights → bind material → draw → HUD → vsync.
