#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include "Misc/Paths.h"
#include "Debug/DebugOverlay.h"
#include "OpenGLVertexAttrib.h"
#include <utility>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505) // unused statics in stb_easy_font.h
#endif
#include <stb_easy_font.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace {

constexpr float kHudPixelScale = 2.0f;
constexpr float kMessagePixelScale = 1.15f;
constexpr float kMarginX = 10.0f;
constexpr float kMarginY = 10.0f;
constexpr float kMessageLineStepY = 14.0f * kMessagePixelScale;
constexpr float kFadeTailSeconds = 0.5f;
constexpr std::size_t kMaxOnScreenMessages = 12;

struct FPackedVert {
    float x, y, z;
    std::array<unsigned char, 4> rgba{};
};

struct FDrawVert {
    float x, y, z;
    float r, g, b, a;
};

void appendTextMesh(std::vector<FDrawVert>& tris, const std::string& text, float originX,
                    float originY, float pixelScale, const std::array<unsigned char, 4>& color) {
    if (text.empty()) {
        return;
    }

    std::vector<char> fontBuf(text.size() * 300 + 64);
    std::vector<char> mutableText(text.begin(), text.end());
    mutableText.push_back('\0');

    std::array<unsigned char, 4> colorCopy = color;
    const int quads = stb_easy_font_print(0.0f, 0.0f, mutableText.data(), colorCopy.data(),
                                          fontBuf.data(), static_cast<int>(fontBuf.size()));
    if (quads <= 0) {
        return;
    }

    const auto* packed = reinterpret_cast<const FPackedVert*>(fontBuf.data());
    auto push = [&](const FPackedVert& v) {
        FDrawVert out{};
        out.x = (v.x * pixelScale) + originX;
        out.y = (v.y * pixelScale) + originY;
        out.z = 0.0f;
        out.r = color[0] / 255.0f;
        out.g = color[1] / 255.0f;
        out.b = color[2] / 255.0f;
        out.a = color[3] / 255.0f;
        tris.push_back(out);
    };

    for (int q = 0; q < quads; ++q) {
        push(packed[(q * 4) + 0]);
        push(packed[(q * 4) + 1]);
        push(packed[(q * 4) + 2]);
        push(packed[(q * 4) + 0]);
        push(packed[(q * 4) + 2]);
        push(packed[(q * 4) + 3]);
    }
}

void appendRightAlignedLines(std::vector<FDrawVert>& tris, const std::string& text,
                             int framebufferWidth, float originY, float pixelScale,
                             const std::array<unsigned char, 4>& color) {
    if (text.empty()) {
        return;
    }

    float y = originY;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::size_t count =
            (end == std::string::npos) ? (text.size() - start) : (end - start);
        const std::string line = text.substr(start, count);

        if (!line.empty()) {
            std::vector<char> mutableLine(line.begin(), line.end());
            mutableLine.push_back('\0');
            const auto rawWidth = static_cast<float>(stb_easy_font_width(mutableLine.data()));
            const float originX =
                static_cast<float>(framebufferWidth) - (rawWidth * pixelScale) - kMarginX;
            appendTextMesh(tris, line, originX, y, pixelScale, color);
        }

        y += 14.0f * pixelScale;
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

void appendCenterAlignedLines(std::vector<FDrawVert>& tris, const std::string& text,
                              int framebufferWidth, float originY, float pixelScale,
                              const std::array<unsigned char, 4>& color) {
    if (text.empty()) {
        return;
    }

    float y = originY;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::size_t count =
            (end == std::string::npos) ? (text.size() - start) : (end - start);
        const std::string line = text.substr(start, count);

        if (!line.empty()) {
            std::vector<char> mutableLine(line.begin(), line.end());
            mutableLine.push_back('\0');
            const auto rawWidth = static_cast<float>(stb_easy_font_width(mutableLine.data()));
            const float originX =
                (static_cast<float>(framebufferWidth) - (rawWidth * pixelScale)) * 0.5f;
            appendTextMesh(tris, line, originX, y, pixelScale, color);
        }

        y += 14.0f * pixelScale;
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

/// Draw multiline text. `anchorX` is left / center / right of each line per `justify`.
void appendJustifiedLines(std::vector<FDrawVert>& tris, const std::string& text, float anchorX,
                          float originY, float pixelScale, ETextJustify justify,
                          const std::array<unsigned char, 4>& color) {
    if (text.empty()) {
        return;
    }

    float y = originY;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::size_t count =
            (end == std::string::npos) ? (text.size() - start) : (end - start);
        const std::string line = text.substr(start, count);

        if (!line.empty()) {
            std::vector<char> mutableLine(line.begin(), line.end());
            mutableLine.push_back('\0');
            const auto rawWidth = static_cast<float>(stb_easy_font_width(mutableLine.data()));
            const float lineW = rawWidth * pixelScale;
            float originX = anchorX;
            if (justify == ETextJustify::Center) {
                originX = anchorX - (lineW * 0.5f);
            } else if (justify == ETextJustify::Right) {
                originX = anchorX - lineW;
            }
            appendTextMesh(tris, line, originX, y, pixelScale, color);
        }

        y += 14.0f * pixelScale;
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

/// Measure multiline HUD text: max line width (raw font units) + line count.
void MeasureMultilineText(const std::string& text, float& outMaxRawWidth, int& outLineCount) {
    outMaxRawWidth = 0.0f;
    outLineCount = 1;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::size_t count =
            (end == std::string::npos) ? (text.size() - start) : (end - start);
        if (count > 0) {
            std::vector<char> line(text.begin() + static_cast<std::ptrdiff_t>(start),
                                   text.begin() + static_cast<std::ptrdiff_t>(start + count));
            line.push_back('\0');
            outMaxRawWidth =
                std::max(outMaxRawWidth, static_cast<float>(stb_easy_font_width(line.data())));
        }
        if (end == std::string::npos) {
            break;
        }
        ++outLineCount;
        start = end + 1;
    }
}

[[nodiscard]] std::array<unsigned char, 4> colorWithAlpha(const glm::vec3& rgb, float alpha) {
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    return {static_cast<unsigned char>(std::clamp(rgb.r, 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(std::clamp(rgb.g, 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(std::clamp(rgb.b, 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(a * 255.0f)};
}

void appendScreenQuad(std::vector<FDrawVert>& tris, float x0, float y0, float x1, float y1, float x2,
                      float y2, float x3, float y3, const glm::vec3& color) {
    auto push = [&](float x, float y) {
        FDrawVert out{};
        out.x = x;
        out.y = y;
        out.z = 0.0f;
        out.r = color.r;
        out.g = color.g;
        out.b = color.b;
        out.a = 1.0f;
        tris.push_back(out);
    };
    push(x0, y0);
    push(x1, y1);
    push(x2, y2);
    push(x0, y0);
    push(x2, y2);
    push(x3, y3);
}

void appendThickScreenLine(std::vector<FDrawVert>& tris, float x0, float y0, float x1, float y1,
                           float thickness, const glm::vec3& color) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = std::sqrt((dx * dx) + (dy * dy));
    if (len < 1.0e-4f) {
        return;
    }
    const float hx = (-dy / len) * (thickness * 0.5f);
    const float hy = (dx / len) * (thickness * 0.5f);
    appendScreenQuad(tris, x0 - hx, y0 - hy, x0 + hx, y0 + hy, x1 + hx, y1 + hy, x1 - hx, y1 - hy,
                     color);
}

} // namespace

bool FDebugOverlay::Initialize(const std::string& /*shaderDirectory*/) {
    const std::string vert = FPaths::ResolveAssetPath("assets/Shaders/debug_overlay.vert");
    const std::string frag = FPaths::ResolveAssetPath("assets/Shaders/debug_overlay.frag");
    if (!shader_.LoadFromFiles(vert, frag)) {
        std::cerr << "Failed to load debug overlay shaders\n";
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FDrawVert), GlAttribOffset(&FDrawVert::x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(FDrawVert), GlAttribOffset(&FDrawVert::r));
    glBindVertexArray(0);
    return true;
}

void FDebugOverlay::Shutdown() {
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    shader_.Destroy();
    text_.clear();
    bottomLeftText_.clear();
    centerText_.clear();
    rightText_.clear();
    rightTextOriginY_ = kMarginY;
    onScreenMessages_.clear();
    screenLines_.clear();
    screenRects_.clear();
    vertexCount_ = 0;
    dirty_ = true;
    builtForWidth_ = 0;
    builtForHeight_ = 0;
}

void FDebugOverlay::SetText(const std::string& text) {
    if (text_ == text) {
        return;
    }
    text_ = text;
    dirty_ = true;
}

void FDebugOverlay::SetBottomLeftText(const std::string& text) {
    if (bottomLeftText_ == text) {
        return;
    }
    bottomLeftText_ = text;
    dirty_ = true;
}

void FDebugOverlay::SetCenterText(const std::string& text) {
    if (centerText_ == text) {
        return;
    }
    centerText_ = text;
    dirty_ = true;
}

void FDebugOverlay::SetRightText(const std::string& text) {
    if (rightText_ == text) {
        return;
    }
    rightText_ = text;
    dirty_ = true;
}

void FDebugOverlay::SetRightTextOriginY(float originY) {
    if (rightTextOriginY_ == originY) {
        return;
    }
    rightTextOriginY_ = originY;
    dirty_ = true;
}

void FDebugOverlay::AddOnScreenDebugMessage(std::string message, float displaySeconds,
                                           const glm::vec3& color) {
    if (message.empty()) {
        return;
    }
    const float duration = displaySeconds > 0.0f ? displaySeconds : 0.01f;
    onScreenMessages_.push_back(FOnScreenMessage{std::move(message), duration, duration, color});
    while (onScreenMessages_.size() > kMaxOnScreenMessages) {
        onScreenMessages_.erase(onScreenMessages_.begin());
    }
    dirty_ = true;
}

void FDebugOverlay::TickOnScreenMessages(float deltaTime) {
    if (onScreenMessages_.empty()) {
        return;
    }
    bool changed = false;
    for (FOnScreenMessage& msg : onScreenMessages_) {
        msg.timeRemaining -= deltaTime;
        changed = true;
    }
    const auto eraseIt =
        std::remove_if(onScreenMessages_.begin(), onScreenMessages_.end(),
                       [](const FOnScreenMessage& msg) { return msg.timeRemaining <= 0.0f; });
    if (eraseIt != onScreenMessages_.end()) {
        onScreenMessages_.erase(eraseIt, onScreenMessages_.end());
        changed = true;
    }
    if (changed) {
        dirty_ = true;
    }
}

void FDebugOverlay::ClearScreenGeometry() {
    if (screenLines_.empty() && screenRects_.empty() && screenTexts_.empty()) {
        return;
    }
    screenLines_.clear();
    screenRects_.clear();
    screenTexts_.clear();
    dirty_ = true;
}

void FDebugOverlay::AddScreenLine(float x0, float y0, float x1, float y1, const glm::vec3& color,
                                 float thickness) {
    screenLines_.push_back(FScreenLine{x0, y0, x1, y1, thickness, color});
    dirty_ = true;
}

void FDebugOverlay::AddScreenRect(float x, float y, float w, float h, const glm::vec3& color) {
    screenRects_.push_back(FScreenRect{x, y, w, h, color});
    dirty_ = true;
}

void FDebugOverlay::AddScreenText(std::string text, float x, float y, const glm::vec3& color,
                                 float pixelScale, ETextJustify justify) {
    if (text.empty()) {
        return;
    }
    screenTexts_.push_back(
        FScreenText{std::move(text), x, y, pixelScale, justify, color});
    dirty_ = true;
}

void FDebugOverlay::MeasureText(const std::string& text, float pixelScale, float& outWidth,
                               float& outHeight) {
    float maxRaw = 0.0f;
    int lines = 1;
    MeasureMultilineText(text, maxRaw, lines);
    outWidth = maxRaw * pixelScale;
    outHeight = 14.0f * pixelScale * static_cast<float>(lines);
}

EShaderReloadResult FDebugOverlay::ReloadShader(bool force) {
    return force ? shader_.ForceReloadFromDisk() : shader_.ReloadFromDiskIfChanged();
}

void FDebugOverlay::RebuildMesh(int framebufferWidth, int framebufferHeight) {
    dirty_ = false;
    builtForWidth_ = framebufferWidth;
    builtForHeight_ = framebufferHeight;
    vertexCount_ = 0;
    if (vao_ == 0 ||
        (text_.empty() && bottomLeftText_.empty() && centerText_.empty() && rightText_.empty() &&
         onScreenMessages_.empty() && screenLines_.empty() && screenRects_.empty() &&
         screenTexts_.empty())) {
        return;
    }

    std::vector<FDrawVert> tris;
    constexpr std::array<unsigned char, 4> kLeftColor = {230, 235, 240, 255};
    constexpr std::array<unsigned char, 4> kBottomLeftColor = {200, 210, 220, 255};
    constexpr std::array<unsigned char, 4> kCenterColor = {255, 210, 90, 255};
    constexpr std::array<unsigned char, 4> kRightColor = {240, 240, 245, 255};
    constexpr float kLineStepY = 14.0f * kHudPixelScale;

    // Top-left HUD block (FPS / tools).
    appendTextMesh(tris, text_, kMarginX, kMarginY, kHudPixelScale, kLeftColor);

    if (!bottomLeftText_.empty()) {
        int lineCount = 1;
        for (char c : bottomLeftText_) {
            if (c == '\n') {
                ++lineCount;
            }
        }
        const float originY = static_cast<float>(framebufferHeight) - kMarginY -
                              (kLineStepY * static_cast<float>(lineCount));
        appendTextMesh(tris, bottomLeftText_, kMarginX, originY, kHudPixelScale, kBottomLeftColor);
    }

    if (!centerText_.empty()) {
        float maxRawWidth = 0.0f;
        int lineCount = 1;
        MeasureMultilineText(centerText_, maxRawWidth, lineCount);
        const float blockH = 14.0f * kHudPixelScale * static_cast<float>(lineCount);
        // Vertically center; clamp so short windows still keep the block on-screen.
        float originY = (static_cast<float>(framebufferHeight) - blockH) * 0.5f;
        originY = std::clamp(originY, kMarginY,
                             std::max(kMarginY, static_cast<float>(framebufferHeight) - blockH -
                                                    kMarginY));
        // Each line centered — long Main Menu hints must not left-bias short rows.
        appendCenterAlignedLines(tris, centerText_, framebufferWidth, originY, kHudPixelScale,
                                 kCenterColor);
    }

    // Right-aligned block (stats top-right / level chrome bottom-right).
    appendRightAlignedLines(tris, rightText_, framebufferWidth, rightTextOriginY_, kHudPixelScale,
                            kRightColor);

    // Top-left debug console: newest at the fixed top slot; older lines shift down (+Y).
    float y = kMarginY;
    for (auto it = onScreenMessages_.rbegin(); it != onScreenMessages_.rend(); ++it) {
        float alpha = 1.0f;
        if (it->timeRemaining < kFadeTailSeconds) {
            alpha = std::clamp(it->timeRemaining / kFadeTailSeconds, 0.0f, 1.0f);
        }
        appendTextMesh(tris, it->text, kMarginX, y, kMessagePixelScale,
                       colorWithAlpha(it->color, alpha));
        y += kMessageLineStepY;
        if (y > static_cast<float>(framebufferHeight) - kMarginY) {
            break;
        }
    }

    // Screen widgets: panels/buttons first, then lines, then labels on top.
    // (Texts before rects hid VerticalBox labels under Button fills and under ImageWidget.)
    for (const FScreenRect& rect : screenRects_) {
        appendScreenQuad(tris, rect.x, rect.y, rect.x + rect.w, rect.y, rect.x + rect.w,
                         rect.y + rect.h, rect.x, rect.y + rect.h, rect.color);
    }
    for (const FScreenLine& line : screenLines_) {
        appendThickScreenLine(tris, line.x0, line.y0, line.x1, line.y1, line.thickness, line.color);
    }
    for (const FScreenText& entry : screenTexts_) {
        appendJustifiedLines(tris, entry.text, entry.x, entry.y, entry.pixelScale, entry.justify,
                             colorWithAlpha(entry.color, 1.0f));
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(tris.size() * sizeof(FDrawVert)),
                 tris.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    vertexCount_ = static_cast<int>(tris.size());
}

void FDebugOverlay::Draw(int framebufferWidth, int framebufferHeight) {
    if (!IsValid() || framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    if (dirty_ || builtForWidth_ != framebufferWidth || builtForHeight_ != framebufferHeight) {
        RebuildMesh(framebufferWidth, framebufferHeight);
    }
    if (vertexCount_ <= 0) {
        return;
    }

    const glm::mat4 projection =
        glm::ortho(0.0f, static_cast<float>(framebufferWidth),
                   static_cast<float>(framebufferHeight), 0.0f, -1.0f, 1.0f);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.Bind();
    shader_.SetMat4("uProjection", glm::value_ptr(projection));
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

