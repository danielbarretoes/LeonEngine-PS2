#pragma once

#include <glm/vec3.hpp>

#include "Shader.h"
#include "Fonts/TextLayout.h"
#include <string>
#include <vector>


/// Immediate-mode screen text: top-right stats, bottom-left hints, bottom-right chrome,
/// and top-left timed debug console (Unreal-like AddOnScreenDebugMessage).
class FDebugOverlay {
public:
    bool Initialize(const std::string& ShaderDirectory);
    void Shutdown();

    /// Optional top-left block (unused by default Engine HUD — messages live there).
    void SetText(const std::string& InText);
    /// Bottom-left block (e.g. F1/F2 debug toggles), left-aligned.
    void SetBottomLeftText(const std::string& InText);
    /// Horizontally + vertically centered block (menus / status).
    void SetCenterText(const std::string& InText);
    /// Right-aligned block (Engine stats top-right / level chrome bottom-right).
    void SetRightText(const std::string& InText);
    /// Vertical origin for `SetRightText` (default top margin).
    void SetRightTextOriginY(float OriginY);

    /// Queue a temporary message (top-left console). Newest stays at the top; older
    /// lines shift down. Default color is red; duration and color are configurable.
    void AddOnScreenDebugMessage(std::string Message, float DisplaySeconds = 2.0f,
                                 const glm::vec3& InColor = {1.0f, 0.0f, 0.0f});
    void TickOnScreenMessages(float DeltaTime);

    /// Screen-space geometry for UUserWidget / HUD (cleared each Paint). Pixel coords, top-left.
    void ClearScreenGeometry();
    void AddScreenLine(float InX0, float InY0, float InX1, float InY1, const glm::vec3& InColor,
                       float InThickness = 2.0f);
    void AddScreenRect(float InX, float InY, float InW, float InH, const glm::vec3& InColor);
    /// Widget text. `x`/`y` = left / center / right of the first line per `justify`.
    void AddScreenText(std::string InText, float InX, float InY, const glm::vec3& InColor,
                       float InPixelScale = HudFontScale,
                       ETextJustify InJustify = ETextJustify::Left);

    /// Measure multiline bitmap text at `pixelScale` (width = longest line).
    static void MeasureText(const std::string& InText, float InPixelScale, float& OutWidth,
                            float& OutHeight);

    void Draw(int FramebufferWidth, int FramebufferHeight);
    [[nodiscard]] EShaderReloadResult ReloadShader(bool bForce = false);

    [[nodiscard]] bool IsValid() const { return Shader.Valid() && Vao != 0; }

private:
    struct FOnScreenMessage {
        std::string Text;
        float TimeRemaining = 0.0f;
        float Duration = 0.0f;
        glm::vec3 Color{1.0f, 0.0f, 0.0f};
    };

    struct FScreenLine {
        float X0 = 0.0f;
        float Y0 = 0.0f;
        float X1 = 0.0f;
        float Y1 = 0.0f;
        float Thickness = 2.0f;
        glm::vec3 Color{1.0f};
    };

    struct FScreenRect {
        float X = 0.0f;
        float Y = 0.0f;
        float W = 0.0f;
        float H = 0.0f;
        glm::vec3 Color{1.0f};
    };

    struct FScreenText {
        std::string Text;
        float X = 0.0f;
        float Y = 0.0f;
        float PixelScale = HudFontScale;
        ETextJustify Justify = ETextJustify::Left;
        glm::vec3 Color{1.0f};
    };

    void RebuildMesh(int FramebufferWidth, int FramebufferHeight);

    FShader Shader;
    std::string Text;
    std::string BottomLeftText;
    std::string CenterText;
    std::string RightText;
    float RightTextOriginY = 10.0f;
    std::vector<FOnScreenMessage> OnScreenMessages;
    std::vector<FScreenLine> ScreenLines;
    std::vector<FScreenRect> ScreenRects;
    std::vector<FScreenText> ScreenTexts;
    unsigned int Vao = 0;
    unsigned int Vbo = 0;
    int VertexCount = 0;
    bool bDirty = true;
    int BuiltForWidth = 0;
    int BuiltForHeight = 0;
};

