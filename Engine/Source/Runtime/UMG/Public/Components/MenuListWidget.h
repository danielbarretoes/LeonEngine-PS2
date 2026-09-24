#pragma once

#include <glm/vec3.hpp>
#include "Fonts/TextLayout.h"
#include "Blueprint/UserWidget.h"
#include <string>
#include <vector>

class FGenericWindow;

/// Unreal-like vertical text menu (UMG ListView lite): arrows / Enter / click.
/// Add via HUD::AddWidget; call TickInput each frame from GameMode.
class UMG_API UMenuListWidget : public UUserWidget {
public:
    struct FItem {
        std::string Id;
        std::string Label;
    };

    void SetTitle(std::string InTitle) { Title = std::move(InTitle); }
    void SetHint(std::string InHint) { Hint = std::move(InHint); }
    void SetItems(std::vector<FItem> InItems);
    void SetColor(const glm::vec3& InColor) { Color = InColor; }

    [[nodiscard]] int SelectedIndex() const { return Selected; }
    void SetSelectedIndex(int Index);

    /// Seed edges as pressed + short activate lockout (safe after travel).
    void ResetEdges();

    /// Returns activated item id this frame (empty if none).
    [[nodiscard]] std::string TickInput(FGenericWindow& Window, bool bCursorCaptured, float DeltaTime);

    void NativePaint(FPaintContext& Ctx) override;

private:
    [[nodiscard]] std::string BuildPaintText() const;
    [[nodiscard]] int CountLines(const std::string& Text) const;
    void CacheLayout(int ViewportW, int InViewportH);

    std::string Title = "Menu";
    std::string Hint;
    std::vector<FItem> Items;
    int Selected = 0;
    glm::vec3 Color{1.0f, 0.82f, 0.35f};

    bool bUpWasDown = false;
    bool bDownWasDown = false;
    bool bEnterWasDown = false;
    bool bMouseWasDown = false;
    /// Suppresses keyboard activate only (ghost Enter after travel); mouse stays live.
    float IgnoreActivateSeconds = 0.0f;

    // Layout cached for hit-testing (Paint + TickInput).
    float ItemsTopPx = 0.0f;
    float LineH = HudLineHeight;
    int ViewportH = 0;
};

