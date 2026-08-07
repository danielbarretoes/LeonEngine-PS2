#pragma once

namespace leon::rhi {

/// Allocate framebuffer, init GS CRTC + draw environment (call once from Window::Create).
[[nodiscard]] bool Ps2InitDisplay(int width, int height);

/// Clear the PS2 framebuffer with a solid color (GIF draw_clear).
void Ps2ClearColor(float r, float g, float b);

/// Wait for GS VBlank (presentation).
void Ps2WaitVsync();

/// sin/cos for angle in 1/256-turn units (EE-safe LUT, no libm).
[[nodiscard]] float Ps2Sin256(unsigned angle256);
[[nodiscard]] float Ps2Cos256(unsigned angle256);

/// Default centered yellow debug triangle.
[[nodiscard]] bool Ps2DrawUnlitTriangle();

/// Filled triangle in centered screen space ((0,0) = screen center).
/// `angle256` is turns in 1/256 units (0..255). `size` is half-extent in pixels.
[[nodiscard]] bool Ps2DrawUnlitTriangleEx(float centerX, float centerY, float size,
                                          unsigned angle256, float r, float g, float b);

/// Axis-aligned filled rect in centered screen space.
[[nodiscard]] bool Ps2DrawUnlitRect(float x0, float y0, float x1, float y1, float r, float g,
                                    float b);

/// Upload + draw a cooked PS2 mesh blob (see Docs/ASSET_FORMATS.md § PS2).
[[nodiscard]] bool Ps2DrawCookedMesh(const void* data, unsigned size);

} // namespace leon::rhi
