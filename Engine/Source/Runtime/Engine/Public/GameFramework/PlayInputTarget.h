#pragma once

#include "GenericPlatform/GenericWindow.h"

/// Routes play-mode input to an optional secondary `Window` (Editor PIE New Window).
/// Shipping leaves the override unset — `Resolve` returns the main Engine window.
/// Unreal analogy: focus the play viewport for input without a separate UObject.
class PlayInputTarget {
public:
    void SetWindow(FGenericWindow* window) { window_ = window; }
    [[nodiscard]] FGenericWindow* GetWindow() const { return window_; }
    [[nodiscard]] bool HasOverride() const { return window_ != nullptr; }

    [[nodiscard]] FGenericWindow& Resolve(FGenericWindow& mainWindow) {
        return window_ != nullptr ? *window_ : mainWindow;
    }
    [[nodiscard]] const FGenericWindow& Resolve(const FGenericWindow& mainWindow) const {
        return window_ != nullptr ? *window_ : mainWindow;
    }

    /// When cursor is not OS-captured (Selected Viewport PIE), mouse look only while active.
    void SetMouseLookActive(bool active) { mouseLookActive_ = active; }
    [[nodiscard]] bool IsMouseLookActive() const { return mouseLookActive_; }

private:
    FGenericWindow* window_ = nullptr;
    bool mouseLookActive_ = true;
};

