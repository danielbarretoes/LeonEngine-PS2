# Plugins/RHI/PS2

Lean Emotion Engine GS backend (`leon_rhi_ps2`).

| File | Role |
| --- | --- |
| `Ps2GsContext` | Framebuffer / z-buffer / packet |
| `Ps2RHIDevice` | `IRHIDevice`, display init, clear / vsync |
| `Ps2DrawPrimitives` | 2D unlit triangle / rect, sin LUT |
| `Ps2Draw3D` | Perspective unlit box (`math3d` + `draw3d`) |

Public API: `<leon/rhi/Ps2RHI.h>`.
