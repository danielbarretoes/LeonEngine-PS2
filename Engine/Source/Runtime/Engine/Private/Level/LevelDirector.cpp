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

namespace {

constexpr float PixelScale = 2.0f;
constexpr float Margin = 10.0f;
constexpr float HitPad = 8.0f;
constexpr float LineHeight = 14.0f * PixelScale;

} // namespace

bool FLevelDirector::Initialize(const std::string& ShaderDirectory) {
    return Chrome.Initialize(ShaderDirectory);
}

EShaderReloadResult FLevelDirector::ReloadShaders(bool bForce) {
    return Chrome.ReloadShader(bForce);
}

void FLevelDirector::Shutdown() {
    Chrome.Shutdown();
    Catalog = {};
    Animation.Clear();
    CurrentIndex = 0;
    Elapsed = 0.0f;
    PrevMinX = PrevMaxX = NextMinX = NextMaxX = 0.0f;
    ChromeMinY = ChromeMaxY = 0.0f;
    LayoutFbWidth = 0;
    LayoutFbHeight = 0;
    bMouseWasDown = false;
    bIgnoreDrag = false;
    bKeyPrevDown = false;
    bKeyNextDown = false;
    for (bool& bDown : DigitWasDown) {
        bDown = false;
    }
}

bool FLevelDirector::ScanAndLoad(UGameEngine& Engine, const std::string& ProjectsDirectory) {
    if (!Catalog.ScanProjectPacks(ProjectsDirectory)) {
        return false;
    }
    return LoadIndex(Engine, 0);
}

bool FLevelDirector::ScanPackAndLoad(UGameEngine& Engine, const std::string& PackDirectory,
                                    std::string_view PreferredLevelKey) {
    if (!Catalog.ScanPack(PackDirectory)) {
        return false;
    }
    // Flow: leon.game.json defaultLevel → preferred key; else catalog[0] (path-sorted).
    if (!PreferredLevelKey.empty()) {
        if (LoadByKey(Engine, PreferredLevelKey)) {
            return true;
        }
        std::cerr << "LevelDirector: defaultLevel '" << PreferredLevelKey
                  << "' not in catalog; loading first entry\n";
    }
    return LoadIndex(Engine, 0);
}

bool FLevelDirector::LoadIndex(UGameEngine& Engine, std::size_t Index) {
    if (Catalog.IsEmpty()) {
        return false;
    }

    const std::size_t Target = Index % Catalog.NumEntries();
    FLevelAnimation NextAnim;
    const FLevelEntry& Entry = Catalog.GetEntries()[Target];
    if (!LoadLevelFile(Engine, Entry.Path, &NextAnim)) {
        std::cerr << "LevelDirector: failed to load " << Entry.Path << '\n';
        return false;
    }

    CurrentIndex = Target;
    Elapsed = 0.0f;
    Animation = std::move(NextAnim);
    RefreshChrome(LayoutFbWidth, LayoutFbHeight);
    return true;
}

bool FLevelDirector::LoadByKey(UGameEngine& Engine, std::string_view LevelKey) {
    const std::size_t Index = Catalog.FindIndexByLevelKey(LevelKey);
    if (Index >= Catalog.NumEntries()) {
        std::cerr << "LevelDirector: unknown level key '" << LevelKey << "'\n";
        return false;
    }
    return LoadIndex(Engine, Index);
}

bool FLevelDirector::Next(UGameEngine& Engine) {
    if (Catalog.NumEntries() < 2) {
        return false;
    }
    return LoadIndex(Engine, (CurrentIndex + 1) % Catalog.NumEntries());
}

bool FLevelDirector::Previous(UGameEngine& Engine) {
    if (Catalog.NumEntries() < 2) {
        return false;
    }
    const std::size_t Idx = (CurrentIndex + Catalog.NumEntries() - 1) % Catalog.NumEntries();
    return LoadIndex(Engine, Idx);
}

void FLevelDirector::Update(UGameEngine& Engine, float DeltaTime) {
    Elapsed += DeltaTime;
    auto& Objects = Engine.GetLevel().GetStaticMeshes();
    for (const auto& Spin : Animation.Spins) {
        if (Spin.MeshIndex < Objects.size()) {
            Objects[Spin.MeshIndex].Transform.RotationDegrees.y +=
                Spin.YawDegreesPerSec * DeltaTime;
        }
    }
    for (const auto& Bob : Animation.Bobs) {
        if (Bob.MeshIndex < Objects.size()) {
            Objects[Bob.MeshIndex].Transform.Position.y =
                Bob.BaseY + (Bob.Amplitude * (0.5f + (0.5f * std::sin(Elapsed * Bob.Speed))));
        }
    }
    auto& Points = Engine.GetLevel().GetPointLights();
    for (const auto& Orbit : Animation.Orbits) {
        if (Orbit.LightIndex < Points.size()) {
            Points[Orbit.LightIndex].Transform.Position.x =
                std::cos(Elapsed * Orbit.Speed) * Orbit.Radius;
            Points[Orbit.LightIndex].Transform.Position.z =
                std::sin(Elapsed * Orbit.Speed) * Orbit.Radius;
            Points[Orbit.LightIndex].Transform.Position.y =
                Orbit.Height + (Orbit.HeightAmp * std::sin((Elapsed * Orbit.Speed) * 2.0f));
        }
    }
}

void FLevelDirector::LayoutChrome(int FramebufferWidth, int FramebufferHeight) {
    if (Catalog.IsEmpty()) {
        return;
    }

    const FLevelEntry& Entry = Catalog.GetEntries()[CurrentIndex];
    std::array<char, 128> Label{};
    if (std::snprintf(Label.data(), Label.size(), "<  %s/%s  (%zu/%zu)  >",
                      Entry.Pack.empty() ? "-" : Entry.Pack.c_str(), Entry.Name.c_str(),
                      CurrentIndex + 1, Catalog.NumEntries()) < 0) {
        Label[0] = '\0';
    }

    std::vector<char> MutableLabel(Label.data(), Label.data() + std::strlen(Label.data()) + 1);
    const auto RawWidth = static_cast<float>(stb_easy_font_width(MutableLabel.data()));
    const float TotalW = RawWidth * PixelScale;
    const float OriginX = static_cast<float>(FramebufferWidth) - TotalW - Margin;
    const float OriginY =
        static_cast<float>(std::max(FramebufferHeight, 1)) - Margin - LineHeight;

    // Approximate hit boxes: first glyph cluster "<" and last ">".
    const float ArrowW = 14.0f * PixelScale;
    PrevMinX = OriginX - HitPad;
    PrevMaxX = OriginX + ArrowW + HitPad;
    NextMaxX = OriginX + TotalW + HitPad;
    NextMinX = NextMaxX - ArrowW - HitPad;
    ChromeMinY = OriginY - HitPad;
    ChromeMaxY = OriginY + LineHeight + HitPad;
    LayoutFbWidth = FramebufferWidth;
    LayoutFbHeight = FramebufferHeight;

    Chrome.SetRightTextOriginY(OriginY);
    Chrome.SetRightText(Label.data());
}

void FLevelDirector::RefreshChrome(int FramebufferWidth, int FramebufferHeight) {
    int FbW = FramebufferWidth;
    int FbH = FramebufferHeight;
    if (FbW <= 0) {
        FbW = LayoutFbWidth > 0 ? LayoutFbWidth : 1280;
    }
    if (FbH <= 0) {
        FbH = LayoutFbHeight > 0 ? LayoutFbHeight : 720;
    }
    LayoutChrome(FbW, FbH);
}

void FLevelDirector::DrawUi(int FramebufferWidth, int FramebufferHeight) {
    // Single-level packs: no level-switcher chrome (Shipping then matches PIE visuals).
    if (!bBrowserVisible || Catalog.NumEntries() <= 1) {
        return;
    }
    if (FramebufferWidth != LayoutFbWidth || FramebufferHeight != LayoutFbHeight) {
        LayoutChrome(FramebufferWidth, FramebufferHeight);
    }
    Chrome.Draw(FramebufferWidth, FramebufferHeight);
}

void FLevelDirector::CursorFramebuffer(UGameEngine& Engine, float& OutX, float& OutY) const {
    double Mx = 0.0;
    double My = 0.0;
    Engine.GetWindow().GetCursorPos(Mx, My);

    // Cursor is in window (screen) coordinates; chrome hit boxes are in framebuffer pixels.
    int WinW = 0;
    int WinH = 0;
    Engine.GetWindow().GetWindowSize(WinW, WinH);
    WinW = std::max(WinW, 1);
    WinH = std::max(WinH, 1);

    int FbW = 0;
    int FbH = 0;
    Engine.GetWindow().GetFramebufferSize(FbW, FbH);
    if (FbW <= 0 || FbH <= 0) {
        OutX = static_cast<float>(Mx);
        OutY = static_cast<float>(My);
        return;
    }
    OutX = static_cast<float>(Mx) * static_cast<float>(FbW) / static_cast<float>(WinW);
    OutY = static_cast<float>(My) * static_cast<float>(FbH) / static_cast<float>(WinH);
}

bool FLevelDirector::HitPrev(float X, float Y) const {
    return X >= PrevMinX && X <= PrevMaxX && Y >= ChromeMinY && Y <= ChromeMaxY;
}

bool FLevelDirector::HitNext(float X, float Y) const {
    return X >= NextMinX && X <= NextMaxX && Y >= ChromeMinY && Y <= ChromeMaxY;
}

bool FLevelDirector::HandleUiInput(UGameEngine& Engine) {
    if (!bBrowserVisible || Catalog.IsEmpty()) {
        return false;
    }

    bool bSwitched = false;

    const bool bPrevKey = Engine.GetWindow().IsKeyPressed(EKeys::LeftBracket);
    const bool bNextKey = Engine.GetWindow().IsKeyPressed(EKeys::RightBracket);
    if (bPrevKey && !bKeyPrevDown) {
        bSwitched = Previous(Engine) || bSwitched;
    }
    if (bNextKey && !bKeyNextDown) {
        bSwitched = Next(Engine) || bSwitched;
    }
    bKeyPrevDown = bPrevKey;
    bKeyNextDown = bNextKey;

    // Digit keys 1–9 (and keypad) jump to catalog slot (1-based → index 0–8).
    for (int Digit = 0; Digit < 9; ++Digit) {
        const bool bDown =
            Engine.GetWindow().IsKeyPressed(static_cast<EKeys>(ToKeyCode(EKeys::One) + Digit)) ||
            Engine.GetWindow().IsKeyPressed(static_cast<EKeys>(ToKeyCode(EKeys::NumPadOne) + Digit));
        if (bDown && !DigitWasDown[Digit]) {
            const auto Index = static_cast<std::size_t>(Digit);
            if (Index < Catalog.NumEntries() && Index != CurrentIndex) {
                bSwitched = LoadIndex(Engine, Index) || bSwitched;
            }
        }
        DigitWasDown[Digit] = bDown;
    }

    const bool bMouseDown = Engine.GetWindow().IsMouseButtonDown(EMouseButtons::Left);
    // Captured cursor uses relative motion; chrome hit-testing needs a visible cursor.
    if (bMouseDown && !bMouseWasDown && !Engine.IsCursorCaptured()) {
        float X = 0.0f;
        float Y = 0.0f;
        CursorFramebuffer(Engine, X, Y);
        if (HitPrev(X, Y)) {
            bSwitched = Previous(Engine) || bSwitched;
            bIgnoreDrag = true;
        } else if (HitNext(X, Y)) {
            bSwitched = Next(Engine) || bSwitched;
            bIgnoreDrag = true;
        } else {
            bIgnoreDrag = false;
        }
    }
    if (!bMouseDown) {
        bIgnoreDrag = false;
    }
    bMouseWasDown = bMouseDown;

    return bSwitched || bIgnoreDrag;
}

