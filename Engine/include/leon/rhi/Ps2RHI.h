#pragma once

namespace leon::rhi {

/// Flow: PS2 display
/// 1. Ps2InitDisplay — VRAM + CRTC + z-buffer + draw environment
/// 2. Ps2ClearColor / Ps2DrawUnlit* / Ps2DrawUnlitBox — GIF submit
/// 3. Ps2WaitVsync — present (also Window::SwapBuffers)

[[nodiscard]] bool Ps2InitDisplay(int width, int height);

void Ps2ClearColor(float r, float g, float b);

void Ps2WaitVsync();

/// sin/cos for angle in 1/256-turn units (LUT; no libm on EE).
[[nodiscard]] float Ps2Sin256(unsigned angle256);
[[nodiscard]] float Ps2Cos256(unsigned angle256);

/// Default centered yellow debug triangle (2D screen space).
[[nodiscard]] bool Ps2DrawUnlitTriangle();

/// Filled triangle in centered screen space ((0,0) = screen center).
[[nodiscard]] bool Ps2DrawUnlitTriangleAt(float centerX, float centerY, float size,
                                          unsigned angle256, float r, float g, float b);

/// Axis-aligned filled rect in centered screen space.
[[nodiscard]] bool Ps2DrawUnlitRect(float x0, float y0, float x1, float y1, float r, float g,
                                    float b);

/// Validate + draw a cooked LPS2 blob (see Docs/ASSET_FORMATS.md § PS2).
[[nodiscard]] bool Ps2DrawCookedMesh(const void* data, unsigned size);

/// Unlit box in world space (perspective camera). Angles are 1/256-turn units.
[[nodiscard]] bool Ps2DrawUnlitBox(float centerX, float centerY, float centerZ, float halfExtent,
                                   unsigned yaw256, unsigned pitch256, float r, float g, float b);

} // namespace leon::rhi
