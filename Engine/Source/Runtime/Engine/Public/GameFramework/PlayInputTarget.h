#pragma once

#include "GenericPlatform/GenericWindow.h"

/// Routes play-mode input to an optional secondary `Window` (Editor PIE New Window).
/// Shipping leaves the override unset — `Resolve` returns the main Engine window.
/// Unreal analogy: focus the play viewport for input without a separate UObject.
class ENGINE_API FPlayInputTarget {
public:
    void SetWindow(FGenericWindow* InWindow) { Window = InWindow; }
    [[nodiscard]] FGenericWindow* GetWindow() const { return Window; }
    [[nodiscard]] bool HasOverride() const { return Window != nullptr; }

    [[nodiscard]] FGenericWindow& Resolve(FGenericWindow& MainWindow) {
        return Window != nullptr ? *Window : MainWindow;
    }
    [[nodiscard]] const FGenericWindow& Resolve(const FGenericWindow& MainWindow) const {
        return Window != nullptr ? *Window : MainWindow;
    }

    /// When cursor is not OS-captured (Selected Viewport PIE), mouse look only while active.
    void SetMouseLookActive(bool bActive) { bMouseLookActive = bActive; }
    [[nodiscard]] bool IsMouseLookActive() const { return bMouseLookActive; }

private:
    FGenericWindow* Window = nullptr;
    bool bMouseLookActive = true;
};

