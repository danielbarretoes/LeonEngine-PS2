# Plugins/RHI/PS2

Lean Emotion Engine GS backend (`leon_rhi_ps2`).

| File | Role |
| --- | --- |
| `Ps2GsContext` | Framebuffer / packet / ready flag |
| `Ps2RHIDevice` | `IRHIDevice`, `Ps2InitDisplay`, clear / vsync |
| `Ps2DrawPrimitives` | Unlit triangle / rect, sin LUT, LPS2 header check |

Public API: `<leon/rhi/Ps2RHI.h>`.
