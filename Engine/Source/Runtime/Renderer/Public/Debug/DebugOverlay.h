#pragma once

#include <glm/vec3.hpp>

#include "Shader.h"
#include "Fonts/TextLayout.h"
#include <string>
#include <vector>

namespace leon {

/// Immediate-mode screen text: top-right stats, bottom-left hints, bottom-right chrome,
/// and top-left timed debug console (Unreal-like AddOnScreenDebugMessage).
class DebugOverlay {
public:
    bool Initialize(const std::string& shaderDirectory);
    void Shutdown();

    /// Optional top-left block (unused by default Engine HUD — messages live there).
    void SetText(const std::string& text);
    /// Bottom-left block (e.g. F1/F2 debug toggles), left-aligned.
    void SetBottomLeftText(const std::string& text);
    /// Horizontally + vertically centered block (menus / status).
    void SetCenterText(const std::string& text);
    /// Right-aligned block (Engine stats top-right / level chrome bottom-right).
    void SetRightText(const std::string& text);
    /// Vertical origin for `SetRightText` (default top margin).
    void SetRightTextOriginY(float originY);

    /// Queue a temporary message (top-left console). Newest stays at the top; older
    /// lines shift down. Default color is red; duration and color are configurable.
    void AddOnScreenDebugMessage(std::string message, float displaySeconds = 2.0f,
                                 const glm::vec3& color = {1.0f, 0.0f, 0.0f});
    void TickOnScreenMessages(float deltaTime);

    /// Screen-space geometry for UserWidget / HUD (cleared each Paint). Pixel coords, top-left.
    void ClearScreenGeometry();
    void AddScreenLine(float x0, float y0, float x1, float y1, const glm::vec3& color,
                       float thickness = 2.0f);
    void AddScreenRect(float x, float y, float w, float h, const glm::vec3& color);
    /// Widget text. `x`/`y` = left / center / right of the first line per `justify`.
    void AddScreenText(std::string text, float x, float y, const glm::vec3& color,
                       float pixelScale = kHudFontScale,
                       ETextJustify justify = ETextJustify::Left);

    /// Measure multiline bitmap text at `pixelScale` (width = longest line).
    static void MeasureText(const std::string& text, float pixelScale, float& outWidth,
                            float& outHeight);

    void Draw(int framebufferWidth, int framebufferHeight);
    [[nodiscard]] EShaderReloadResult ReloadShader(bool force = false);

    [[nodiscard]] bool IsValid() const { return shader_.Valid() && vao_ != 0; }

private:
    struct OnScreenMessage {
        std::string text;
        float timeRemaining = 0.0f;
        float duration = 0.0f;
        glm::vec3 color{1.0f, 0.0f, 0.0f};
    };

    struct ScreenLine {
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float thickness = 2.0f;
        glm::vec3 color{1.0f};
    };

    struct ScreenRect {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        glm::vec3 color{1.0f};
    };

    struct ScreenText {
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
        float pixelScale = kHudFontScale;
        ETextJustify justify = ETextJustify::Left;
        glm::vec3 color{1.0f};
    };

    void RebuildMesh(int framebufferWidth, int framebufferHeight);

    Shader shader_;
    std::string text_;
    std::string bottomLeftText_;
    std::string centerText_;
    std::string rightText_;
    float rightTextOriginY_ = 10.0f;
    std::vector<OnScreenMessage> onScreenMessages_;
    std::vector<ScreenLine> screenLines_;
    std::vector<ScreenRect> screenRects_;
    std::vector<ScreenText> screenTexts_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int vertexCount_ = 0;
    bool dirty_ = true;
    int builtForWidth_ = 0;
    int builtForHeight_ = 0;
};

} // namespace leon
