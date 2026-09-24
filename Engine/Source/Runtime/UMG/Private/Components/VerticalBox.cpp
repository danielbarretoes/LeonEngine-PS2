#include "Components/VerticalBox.h"
#include "InputCoreTypes.h"

#include <algorithm>
#include <cmath>

#include "GenericPlatform/GenericWindow.h"
#include "Debug/DebugOverlay.h"

void UVerticalBox::ClearChildren() {
    Buttons.clear();
    Selected = 0;
}

UButton* UVerticalBox::AddButton(std::string Id, std::string Label) {
    auto Button = std::make_unique<UButton>();
    Button->SetId(std::move(Id));
    Button->SetLabel(std::move(Label));
    UButton* Raw = Button.get();
    Buttons.push_back(std::move(Button));
    SnapSelectionToSelectable();
    ApplySelectionVisuals();
    return Raw;
}

UButton* UVerticalBox::GetButton(int Index) {
    if (Index < 0 || Index >= static_cast<int>(Buttons.size())) {
        return nullptr;
    }
    return Buttons[static_cast<std::size_t>(Index)].get();
}

const UButton* UVerticalBox::GetButton(int Index) const {
    if (Index < 0 || Index >= static_cast<int>(Buttons.size())) {
        return nullptr;
    }
    return Buttons[static_cast<std::size_t>(Index)].get();
}

void UVerticalBox::SetSelectedIndex(int Index) {
    if (Buttons.empty()) {
        Selected = 0;
        ApplySelectionVisuals();
        return;
    }
    Selected = std::clamp(Index, 0, static_cast<int>(Buttons.size()) - 1);
    ApplySelectionVisuals();
}

void UVerticalBox::ResetEdges() {
    bUpWasDown = bDownWasDown = bEnterWasDown = bMouseWasDown = true;
    // Keyboard only — mouse must stay usable on the first click after travel.
    IgnoreActivateSeconds = 0.35f;
}

void UVerticalBox::SnapSelectionToSelectable() {
    if (Buttons.empty()) {
        Selected = 0;
        return;
    }
    Selected = std::clamp(Selected, 0, static_cast<int>(Buttons.size()) - 1);
    if (!Buttons[static_cast<std::size_t>(Selected)]->GetId().empty() &&
        Buttons[static_cast<std::size_t>(Selected)]->IsEnabled()) {
        return;
    }
    for (int I = 0; I < static_cast<int>(Buttons.size()); ++I) {
        if (!Buttons[static_cast<std::size_t>(I)]->GetId().empty() &&
            Buttons[static_cast<std::size_t>(I)]->IsEnabled()) {
            Selected = I;
            return;
        }
    }
}

void UVerticalBox::StepSelectable(int Delta) {
    const int N = static_cast<int>(Buttons.size());
    if (N <= 0) {
        return;
    }
    int Idx = Selected;
    for (int Guard = 0; Guard < N; ++Guard) {
        Idx = (Idx + Delta + N) % N;
        UButton* Button = Buttons[static_cast<std::size_t>(Idx)].get();
        if (!Button->GetId().empty() && Button->IsEnabled()) {
            Selected = Idx;
            ApplySelectionVisuals();
            return;
        }
    }
}

void UVerticalBox::ApplySelectionVisuals() {
    for (int I = 0; I < static_cast<int>(Buttons.size()); ++I) {
        Buttons[static_cast<std::size_t>(I)]->SetSelected(I == Selected);
    }
}

void UVerticalBox::CacheLayout(int ViewportW, int ViewportH) {
    if (ViewportW <= 0 || ViewportH <= 0) {
        return;
    }

    float MaxButtonW = MinButtonW;
    float ButtonsH = 0.0f;
    for (std::size_t I = 0; I < Buttons.size(); ++I) {
        float Dw = 0.0f;
        float Dh = 0.0f;
        Buttons[I]->MeasureDesiredSize(Dw, Dh);
        MaxButtonW = std::max(MaxButtonW, Dw);
        ButtonsH += Dh;
        if (I + 1 < Buttons.size()) {
            ButtonsH += ButtonGap;
        }
    }

    TitleH = 0.0f;
    if (!Title.empty()) {
        float Tw = 0.0f;
        FDebugOverlay::MeasureText(Title, HudFontScale, Tw, TitleH);
        TitleH += HudLineHeight; // blank separator under title
    }

    HintH = 0.0f;
    if (!Hint.empty()) {
        float Hw = 0.0f;
        FDebugOverlay::MeasureText(Hint, HudFontScale, Hw, HintH);
        HintH += 12.0f; // gap above hint
    }

    BoxW = MaxButtonW;
    const float TotalH = TitleH + ButtonsH + HintH;
    BoxX = (static_cast<float>(ViewportW) - BoxW) * 0.5f;
    BoxY = std::clamp((static_cast<float>(ViewportH) - TotalH) * 0.5f, 10.0f,
                       std::max(10.0f, static_cast<float>(ViewportH) - TotalH - 10.0f));

    float Y = BoxY + TitleH;
    for (std::unique_ptr<UButton>& Button : Buttons) {
        float Dw = 0.0f;
        float Dh = 0.0f;
        Button->MeasureDesiredSize(Dw, Dh);
        Button->SetSize(BoxW, Dh);
        Button->SetPosition(BoxX, Y);
        Y += Dh + ButtonGap;
    }
}

void UVerticalBox::NativePaint(FPaintContext& Ctx) {
    if (!IsVisible()) {
        return;
    }
    CacheLayout(Ctx.GetWidth(), Ctx.GetHeight());

    if (!Title.empty()) {
        Ctx.DrawText(Title, static_cast<float>(Ctx.GetWidth()) * 0.5f, BoxY,
                     glm::vec3{1.0f, 0.82f, 0.35f}, HudFontScale, ETextJustify::Center);
    }

    for (std::unique_ptr<UButton>& Button : Buttons) {
        Button->NativePaint(Ctx);
    }

    if (!Hint.empty()) {
        float ButtonsBottom = BoxY + TitleH;
        for (const std::unique_ptr<UButton>& Button : Buttons) {
            ButtonsBottom = Button->GetY() + Button->GetHeight();
        }
        const float HintY = ButtonsBottom + 12.0f;
        Ctx.DrawText(Hint, static_cast<float>(Ctx.GetWidth()) * 0.5f, HintY,
                     glm::vec3{0.65f, 0.65f, 0.60f}, HudFontScale, ETextJustify::Center);
    }
}

std::string UVerticalBox::TickInput(FGenericWindow& Window, bool bCursorCaptured, float DeltaTime) {
    if (Buttons.empty()) {
        return {};
    }
    if (IgnoreActivateSeconds > 0.0f) {
        IgnoreActivateSeconds = std::max(0.0f, IgnoreActivateSeconds - DeltaTime);
    }

    int FbW = 0;
    int FbH = 0;
    Window.GetFramebufferSize(FbW, FbH);
    if (FbW > 0 && FbH > 0) {
        CacheLayout(FbW, FbH);
    }

    const bool bUp = Window.IsKeyPressed(EKeys::Up) || Window.IsKeyPressed(EKeys::W);
    const bool bDown = Window.IsKeyPressed(EKeys::Down) || Window.IsKeyPressed(EKeys::S);
    const bool bEnter = Window.IsKeyPressed(EKeys::Enter) ||
                       Window.IsKeyPressed(EKeys::NumPadEnter) ||
                       Window.IsKeyPressed(EKeys::SpaceBar);
    const bool bMouse = Window.IsMouseButtonDown(EMouseButtons::Left);

    if (bUp && !bUpWasDown) {
        StepSelectable(-1);
    }
    if (bDown && !bDownWasDown) {
        StepSelectable(1);
    }

    const bool bAllowKeyboardActivate = IgnoreActivateSeconds <= 0.0f;
    std::string Activated;
    if (bAllowKeyboardActivate && bEnter && !bEnterWasDown) {
        UButton* Button = GetButton(Selected);
        if (Button != nullptr && Button->IsEnabled() && !Button->GetId().empty()) {
            Activated = Button->GetId();
        }
    }

    // Hover + click (never gated by travel lockout).
    if (!bCursorCaptured && FbW > 0 && FbH > 0) {
        double Mx = 0.0;
        double My = 0.0;
        Window.GetCursorPos(Mx, My);
        int WinW = 0;
        int WinH = 0;
        Window.GetWindowSize(WinW, WinH);
        WinW = std::max(WinW, 1);
        WinH = std::max(WinH, 1);
        const float FbX =
            static_cast<float>(Mx) * static_cast<float>(FbW) / static_cast<float>(WinW);
        const float FbY =
            static_cast<float>(My) * static_cast<float>(FbH) / static_cast<float>(WinH);

        for (std::unique_ptr<UButton>& Button : Buttons) {
            Button->SetHovered(Button->Contains(FbX, FbY));
        }

        if (bMouse && !bMouseWasDown) {
            for (int I = 0; I < static_cast<int>(Buttons.size()); ++I) {
                UButton* Button = Buttons[static_cast<std::size_t>(I)].get();
                if (!Button->Contains(FbX, FbY)) {
                    continue;
                }
                Selected = I;
                ApplySelectionVisuals();
                if (Button->IsEnabled() && !Button->GetId().empty()) {
                    Activated = Button->GetId();
                }
                break;
            }
        }
    } else {
        for (std::unique_ptr<UButton>& Button : Buttons) {
            Button->SetHovered(false);
        }
    }

    bUpWasDown = bUp;
    bDownWasDown = bDown;
    bEnterWasDown = bEnter;
    bMouseWasDown = bMouse;
    return Activated;
}

