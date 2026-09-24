#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "Engine/GameEngine.h"
#include "InputCoreTypes.h"
#include "Level/LevelDirector.h"
#include "Level/LevelLoader.h"
#include <string_view>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505)
#endif
#include <stb_easy_font.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace leon {
namespace {

constexpr float kPixelScale = 2.0f;
constexpr float kMargin = 10.0f;
constexpr float kHitPad = 8.0f;
constexpr float kLineHeight = 14.0f * kPixelScale;

} // namespace

bool LevelDirector::Initialize(const std::string& shaderDirectory) {
    return chrome_.Initialize(shaderDirectory);
}

EShaderReloadResult LevelDirector::ReloadShaders(bool force) {
    return chrome_.ReloadShader(force);
}

void LevelDirector::Shutdown() {
    chrome_.Shutdown();
    catalog_ = {};
    animation_.clear();
    currentIndex_ = 0;
    elapsed_ = 0.0f;
    prevMinX_ = prevMaxX_ = nextMinX_ = nextMaxX_ = 0.0f;
    chromeMinY_ = chromeMaxY_ = 0.0f;
    layoutFbWidth_ = 0;
    layoutFbHeight_ = 0;
    mouseWasDown_ = false;
    ignoreDrag_ = false;
    keyPrevDown_ = false;
    keyNextDown_ = false;
    for (bool& down : digitWasDown_) {
        down = false;
    }
}

bool LevelDirector::ScanAndLoad(Engine& engine, const std::string& projectsDirectory) {
    if (!catalog_.ScanProjectPacks(projectsDirectory)) {
        return false;
    }
    return LoadIndex(engine, 0);
}

bool LevelDirector::ScanPackAndLoad(Engine& engine, const std::string& packDirectory,
                                    std::string_view preferredLevelKey) {
    if (!catalog_.ScanPack(packDirectory)) {
        return false;
    }
    // Flow: leon.game.json defaultLevel → preferred key; else catalog[0] (path-sorted).
    if (!preferredLevelKey.empty()) {
        if (LoadByKey(engine, preferredLevelKey)) {
            return true;
        }
        std::cerr << "LevelDirector: defaultLevel '" << preferredLevelKey
                  << "' not in catalog; loading first entry\n";
    }
    return LoadIndex(engine, 0);
}

bool LevelDirector::LoadIndex(Engine& engine, std::size_t index) {
    if (catalog_.IsEmpty()) {
        return false;
    }

    const std::size_t target = index % catalog_.NumEntries();
    LevelAnimation nextAnim;
    const LevelEntry& entry = catalog_.Entries()[target];
    if (!LoadLevelFile(engine, entry.path, &nextAnim)) {
        std::cerr << "LevelDirector: failed to load " << entry.path << '\n';
        return false;
    }

    currentIndex_ = target;
    elapsed_ = 0.0f;
    animation_ = std::move(nextAnim);
    refreshChrome(layoutFbWidth_, layoutFbHeight_);
    return true;
}

bool LevelDirector::LoadByKey(Engine& engine, std::string_view levelKey) {
    const std::size_t index = catalog_.FindIndexByLevelKey(levelKey);
    if (index >= catalog_.NumEntries()) {
        std::cerr << "LevelDirector: unknown level key '" << levelKey << "'\n";
        return false;
    }
    return LoadIndex(engine, index);
}

bool LevelDirector::Next(Engine& engine) {
    if (catalog_.NumEntries() < 2) {
        return false;
    }
    return LoadIndex(engine, (currentIndex_ + 1) % catalog_.NumEntries());
}

bool LevelDirector::Previous(Engine& engine) {
    if (catalog_.NumEntries() < 2) {
        return false;
    }
    const std::size_t idx = (currentIndex_ + catalog_.NumEntries() - 1) % catalog_.NumEntries();
    return LoadIndex(engine, idx);
}

void LevelDirector::Update(Engine& engine, float deltaTime) {
    elapsed_ += deltaTime;
    auto& objects = engine.GetLevel().StaticMeshes();
    for (const auto& spin : animation_.spins) {
        if (spin.meshIndex < objects.size()) {
            objects[spin.meshIndex].transform.rotationDegrees.y +=
                spin.yawDegreesPerSec * deltaTime;
        }
    }
    for (const auto& bob : animation_.bobs) {
        if (bob.meshIndex < objects.size()) {
            objects[bob.meshIndex].transform.position.y =
                bob.baseY + (bob.amplitude * (0.5f + (0.5f * std::sin(elapsed_ * bob.speed))));
        }
    }
    auto& points = engine.GetLevel().PointLights();
    for (const auto& orbit : animation_.orbits) {
        if (orbit.lightIndex < points.size()) {
            points[orbit.lightIndex].transform.position.x =
                std::cos(elapsed_ * orbit.speed) * orbit.radius;
            points[orbit.lightIndex].transform.position.z =
                std::sin(elapsed_ * orbit.speed) * orbit.radius;
            points[orbit.lightIndex].transform.position.y =
                orbit.height + (orbit.heightAmp * std::sin((elapsed_ * orbit.speed) * 2.0f));
        }
    }
}

void LevelDirector::layoutChrome(int framebufferWidth, int framebufferHeight) {
    if (catalog_.IsEmpty()) {
        return;
    }

    const LevelEntry& entry = catalog_.Entries()[currentIndex_];
    std::array<char, 128> label{};
    if (std::snprintf(label.data(), label.size(), "<  %s/%s  (%zu/%zu)  >",
                      entry.pack.empty() ? "-" : entry.pack.c_str(), entry.name.c_str(),
                      currentIndex_ + 1, catalog_.NumEntries()) < 0) {
        label[0] = '\0';
    }

    std::vector<char> mutableLabel(label.data(), label.data() + std::strlen(label.data()) + 1);
    const auto rawWidth = static_cast<float>(stb_easy_font_width(mutableLabel.data()));
    const float totalW = rawWidth * kPixelScale;
    const float originX = static_cast<float>(framebufferWidth) - totalW - kMargin;
    const float originY =
        static_cast<float>(std::max(framebufferHeight, 1)) - kMargin - kLineHeight;

    // Approximate hit boxes: first glyph cluster "<" and last ">".
    const float arrowW = 14.0f * kPixelScale;
    prevMinX_ = originX - kHitPad;
    prevMaxX_ = originX + arrowW + kHitPad;
    nextMaxX_ = originX + totalW + kHitPad;
    nextMinX_ = nextMaxX_ - arrowW - kHitPad;
    chromeMinY_ = originY - kHitPad;
    chromeMaxY_ = originY + kLineHeight + kHitPad;
    layoutFbWidth_ = framebufferWidth;
    layoutFbHeight_ = framebufferHeight;

    chrome_.SetRightTextOriginY(originY);
    chrome_.SetRightText(label.data());
}

void LevelDirector::refreshChrome(int framebufferWidth, int framebufferHeight) {
    int fbW = framebufferWidth;
    int fbH = framebufferHeight;
    if (fbW <= 0) {
        fbW = layoutFbWidth_ > 0 ? layoutFbWidth_ : 1280;
    }
    if (fbH <= 0) {
        fbH = layoutFbHeight_ > 0 ? layoutFbHeight_ : 720;
    }
    layoutChrome(fbW, fbH);
}

void LevelDirector::DrawUi(int framebufferWidth, int framebufferHeight) {
    // Single-level packs: no level-switcher chrome (Shipping then matches PIE visuals).
    if (!browserVisible_ || catalog_.NumEntries() <= 1) {
        return;
    }
    if (framebufferWidth != layoutFbWidth_ || framebufferHeight != layoutFbHeight_) {
        layoutChrome(framebufferWidth, framebufferHeight);
    }
    chrome_.Draw(framebufferWidth, framebufferHeight);
}

void LevelDirector::cursorFramebuffer(Engine& engine, float& outX, float& outY) const {
    double mx = 0.0;
    double my = 0.0;
    engine.GetWindow().GetCursorPos(mx, my);

    // Cursor is in window (screen) coordinates; chrome hit boxes are in framebuffer pixels.
    int winW = 0;
    int winH = 0;
    engine.GetWindow().GetWindowSize(winW, winH);
    winW = std::max(winW, 1);
    winH = std::max(winH, 1);

    int fbW = 0;
    int fbH = 0;
    engine.GetWindow().GetFramebufferSize(fbW, fbH);
    if (fbW <= 0 || fbH <= 0) {
        outX = static_cast<float>(mx);
        outY = static_cast<float>(my);
        return;
    }
    outX = static_cast<float>(mx) * static_cast<float>(fbW) / static_cast<float>(winW);
    outY = static_cast<float>(my) * static_cast<float>(fbH) / static_cast<float>(winH);
}

bool LevelDirector::hitPrev(float x, float y) const {
    return x >= prevMinX_ && x <= prevMaxX_ && y >= chromeMinY_ && y <= chromeMaxY_;
}

bool LevelDirector::hitNext(float x, float y) const {
    return x >= nextMinX_ && x <= nextMaxX_ && y >= chromeMinY_ && y <= chromeMaxY_;
}

bool LevelDirector::HandleUiInput(Engine& engine) {
    if (!browserVisible_ || catalog_.IsEmpty()) {
        return false;
    }

    bool switched = false;

    const bool prevKey = engine.GetWindow().IsKeyPressed(EKeys::LeftBracket);
    const bool nextKey = engine.GetWindow().IsKeyPressed(EKeys::RightBracket);
    if (prevKey && !keyPrevDown_) {
        switched = Previous(engine) || switched;
    }
    if (nextKey && !keyNextDown_) {
        switched = Next(engine) || switched;
    }
    keyPrevDown_ = prevKey;
    keyNextDown_ = nextKey;

    // Digit keys 1–9 (and keypad) jump to catalog slot (1-based → index 0–8).
    for (int digit = 0; digit < 9; ++digit) {
        const bool down =
            engine.GetWindow().IsKeyPressed(static_cast<EKeys>(ToKeyCode(EKeys::One) + digit)) ||
            engine.GetWindow().IsKeyPressed(static_cast<EKeys>(ToKeyCode(EKeys::NumPadOne) + digit));
        if (down && !digitWasDown_[digit]) {
            const auto index = static_cast<std::size_t>(digit);
            if (index < catalog_.NumEntries() && index != currentIndex_) {
                switched = LoadIndex(engine, index) || switched;
            }
        }
        digitWasDown_[digit] = down;
    }

    const bool mouseDown = engine.GetWindow().IsMouseButtonDown(EMouseButtons::Left);
    // Captured cursor uses relative motion; chrome hit-testing needs a visible cursor.
    if (mouseDown && !mouseWasDown_ && !engine.IsCursorCaptured()) {
        float x = 0.0f;
        float y = 0.0f;
        cursorFramebuffer(engine, x, y);
        if (hitPrev(x, y)) {
            switched = Previous(engine) || switched;
            ignoreDrag_ = true;
        } else if (hitNext(x, y)) {
            switched = Next(engine) || switched;
            ignoreDrag_ = true;
        } else {
            ignoreDrag_ = false;
        }
    }
    if (!mouseDown) {
        ignoreDrag_ = false;
    }
    mouseWasDown_ = mouseDown;

    return switched || ignoreDrag_;
}

} // namespace leon
