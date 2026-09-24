#pragma once

#include "Components/Button.h"
#include "Blueprint/UserWidget.h"
#include <memory>
#include <string>
#include <vector>

class FGenericWindow;

/// Unreal-like UVerticalBox (lite): title + stacked ButtonWidgets + hint.
/// Add via HUD::AddWidget; call TickInput each frame from GameMode (same as UMenuListWidget).
class UVerticalBox : public UUserWidget {
public:
    void SetTitle(std::string InTitle) { Title = std::move(InTitle); }
    void SetHint(std::string InHint) { Hint = std::move(InHint); }

    void ClearChildren();

    /// Append a UButton-like child. Empty `id` = non-activatable status row.
    UButton* AddButton(std::string Id, std::string Label);

    [[nodiscard]] int NumButtons() const { return static_cast<int>(Buttons.size()); }
    [[nodiscard]] UButton* GetButton(int Index);
    [[nodiscard]] const UButton* GetButton(int Index) const;

    [[nodiscard]] int SelectedIndex() const { return Selected; }
    void SetSelectedIndex(int Index);

    /// Seed edges as pressed + short keyboard activate lockout (safe after travel).
    void ResetEdges();

    /// Returns activated button id this frame (empty if none).
    [[nodiscard]] std::string TickInput(FGenericWindow& Window, bool bCursorCaptured, float DeltaTime);

    void NativePaint(FPaintContext& Ctx) override;

private:
    void CacheLayout(int ViewportW, int ViewportH);
    void SnapSelectionToSelectable();
    void StepSelectable(int Delta);
    void ApplySelectionVisuals();

    std::string Title = "Menu";
    std::string Hint;
    std::vector<std::unique_ptr<UButton>> Buttons;
    int Selected = 0;

    bool bUpWasDown = false;
    bool bDownWasDown = false;
    bool bEnterWasDown = false;
    bool bMouseWasDown = false;
    /// Suppresses keyboard activate only (ghost Enter after travel); mouse stays live.
    float IgnoreActivateSeconds = 0.0f;

    float BoxX = 0.0f;
    float BoxY = 0.0f;
    float BoxW = 0.0f;
    float TitleH = 0.0f;
    float HintH = 0.0f;
    float ButtonGap = 8.0f;
    float MinButtonW = 220.0f;
};

