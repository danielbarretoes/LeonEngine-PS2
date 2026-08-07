#pragma once

namespace leon::rhi {

/// Flow: PS2 display
/// 1. Ps2InitDisplay — VRAM + CRTC + draw environment
/// 2. Ps2ClearColor / Ps2DrawUnlit* — GIF submit
/// 3. Ps2WaitVsync — present (also Window::SwapBuffers)

[[nodiscard]] bool Ps2InitDisplay(int width, int height);

void Ps2ClearColor(float r, float g, float b);

void Ps2WaitVsync();

/// sin/cos for angle in 1/256-turn units (LUT; no libm on EE).
[[nodiscard]] float Ps2Sin256(unsigned angle256);
[[nodiscard]] float Ps2Cos256(unsigned angle256);

/// Default centered yellow debug triangle.
[[nodiscard]] bool Ps2DrawUnlitTriangle();

/// Filled triangle in centered screen space ((0,0) = screen center).
/// `angle256` is turns in 1/256 units. `size` is half-extent in pixels.
[[nodiscard]] bool Ps2DrawUnlitTriangleAt(float centerX, float centerY, float size,
                                          unsigned angle256, float r, float g, float b);

/// Axis-aligned filled rect in centered screen space.
[[nodiscard]] bool Ps2DrawUnlitRect(float x0, float y0, float x1, float y1, float r, float g,
                                    float b);

/// Validate + draw a cooked LPS2 blob (see Docs/ASSET_FORMATS.md § PS2).
[[nodiscard]] bool Ps2DrawCookedMesh(const void* data, unsigned size);

} // namespace leon::rhi
