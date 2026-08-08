#pragma once

#include <cstdint>

namespace leon::rhi {

/// Flow: PS2 display frame (editor-ready contract)
/// 1. Ps2InitDisplay — VRAM + CRTC + z-buffer + draw environment
/// 2. Ps2SetViewTarget / Ps2SetDirectionalLight / Ps2SetAmbientLightColor
/// 3. Ps2ClearColor → Ps2BindMaterial → Ps2DrawBox* → Ps2DrawDebugHudText
/// 4. Ps2WaitVsync — present (also Window::SwapBuffers)

[[nodiscard]] bool Ps2InitDisplay(int width, int height);

void Ps2ClearColor(float r, float g, float b);

void Ps2WaitVsync();

/// sin/cos for angle in 1/256-turn units (LUT; no libm on EE).
[[nodiscard]] float Ps2Sin256(unsigned angle256);
[[nodiscard]] float Ps2Cos256(unsigned angle256);

/// EE system timer → microseconds (for FPS / ms HUD).
[[nodiscard]] std::uint64_t Ps2GetSystemTimeUs();

// --- View / lights (Unreal-like POD; no U/A/F prefixes) ---

/// Active view target (camera). Rotation in radians (Pitch/Yaw for math3d).
struct Ps2ViewTarget {
    float LocationX = 0.0f;
    float LocationY = 14.0f;
    float LocationZ = 58.0f;
    float Pitch = -0.24f;
    float Yaw = 0.0f;
};

/// Directional sun: aim via Yaw256/Pitch256 (1/256-turn); Intensity scales LightColor.
struct DirectionalLight {
    float Intensity = 1.0f;
    float LightColorR = 1.00f;
    float LightColorG = 0.97f;
    float LightColorB = 0.90f;
    unsigned Yaw256 = 12;
    unsigned Pitch256 = 80; // high pitch ≈ from above
};

void Ps2SetViewTarget(const Ps2ViewTarget& viewTarget);
void Ps2SetDirectionalLight(const DirectionalLight& light);
void Ps2SetAmbientLightColor(float r, float g, float b);

// --- Texture / material (.lmat subset) ---

enum class EShadingModel : std::uint8_t {
    DefaultLit = 0,
    Unlit = 1,
};

/// GS VRAM texture (RGBA8 → PSMCT32). Accessors match host Texture Create/Bind/Valid.
class Ps2Texture {
public:
    Ps2Texture() = default;
    ~Ps2Texture();

    Ps2Texture(const Ps2Texture&) = delete;
    Ps2Texture& operator=(const Ps2Texture&) = delete;
    Ps2Texture(Ps2Texture&& other) noexcept;
    Ps2Texture& operator=(Ps2Texture&& other) noexcept;

    [[nodiscard]] static Ps2Texture Create(int width, int height, const unsigned char* rgba);
    /// Content name: T_Checker_D
    [[nodiscard]] static Ps2Texture CreateChecker(int size = 64);
    /// Content name: T_Grid_D
    [[nodiscard]] static Ps2Texture CreateGrid(int size = 64);

    void Destroy();
    [[nodiscard]] bool Valid() const;
    /// Bind TEX0 / sampling for subsequent textured draws.
    void Bind() const;

    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }
    [[nodiscard]] int VramAddress() const { return vramAddress_; }

private:
    explicit Ps2Texture(int width, int height, int vramAddress, int bufferWidth)
        : width_(width), height_(height), vramAddress_(vramAddress), bufferWidth_(bufferWidth) {}

    /// `rgba` must be 16-byte aligned. Uploads to VRAM; does not free `rgba`.
    [[nodiscard]] static Ps2Texture CreateFromAlignedRgba(int width, int height,
                                                          unsigned char* rgba);

    int width_ = 0;
    int height_ = 0;
    int vramAddress_ = 0;
    int bufferWidth_ = 0;
};

/// PS2-lite material aligned to .lmat: BaseColor, BaseColorMap, ShadingModel.
struct Ps2Material {
    float BaseColorR = 1.0f;
    float BaseColorG = 1.0f;
    float BaseColorB = 1.0f;
    const Ps2Texture* BaseColorMap = nullptr;
    EShadingModel ShadingModel = EShadingModel::DefaultLit;
    /// When true and no BaseColorMap: multiply per-face RGB (debug cube).
    bool UseFaceAlbedo = false;
};

void Ps2BindMaterial(const Ps2Material& material);

// --- 2D unlit (screen space) ---

[[nodiscard]] bool Ps2DrawUnlitTriangle();

[[nodiscard]] bool Ps2DrawUnlitTriangleAt(float centerX, float centerY, float size,
                                          unsigned angle256, float r, float g, float b);

[[nodiscard]] bool Ps2DrawUnlitRect(float x0, float y0, float x1, float y1, float r, float g,
                                    float b);

/// DebugOverlay-lite: 5×7 glyphs via rects (ASCII subset for FPS HUD).
void Ps2DrawDebugHudText(float x, float y, const char* text, float r = 0.95f, float g = 0.95f,
                         float b = 0.85f);

/// Validate + draw a cooked LPS2 blob (see Docs/ASSET_FORMATS.md § PS2).
[[nodiscard]] bool Ps2DrawCookedMesh(const void* data, unsigned size);

/// Lit/textured box: Location / Rotation (1/256-turn) / uniform Scale (half-extent).
[[nodiscard]] bool Ps2DrawBox(float locationX, float locationY, float locationZ, unsigned yaw256,
                              unsigned pitch256, float scale);

} // namespace leon::rhi
