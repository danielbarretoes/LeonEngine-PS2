#pragma once

#include "InputCoreTypes.h"
#include "GameFramework/Input.h"
#include "GenericPlatform/GenericWindow.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

/// One key contribution to a 1D axis (Unreal-like axis mapping entry).
struct InputAxisKey {
    int key = 0;        // EKeys underlying code (Host matches GLFW)
    float scale = 1.0f; // typically +1 or -1
};

/// Maps action names → keys (Unreal-like Input Mapping Context).
class InputMappingContext {
public:
    /// Bind a key that contributes `scale` to a named axis while held.
    void BindAxisKey(std::string_view action, int key, float scale = 1.0f);
    void BindAxisKey(std::string_view action, EKeys key, float scale = 1.0f);

    /// Bind a digital action key (pressed / just-pressed queries).
    void BindActionKey(std::string_view action, int key);
    void BindActionKey(std::string_view action, EKeys key);

    [[nodiscard]] const std::unordered_map<std::string, std::vector<InputAxisKey>>& Axes() const {
        return axes_;
    }
    [[nodiscard]] const std::unordered_map<std::string, std::vector<int>>& Actions() const {
        return actions_;
    }

    /// Default Leon gameplay map: WASD+arrows move, Q/E up, Space jump.
    [[nodiscard]] static InputMappingContext MakeDefault();

private:
    std::unordered_map<std::string, std::vector<InputAxisKey>> axes_;
    std::unordered_map<std::string, std::vector<int>> actions_;
};

/// Samples mapped input once per frame (Unreal-like PlayerInput).
class PlayerInput {
public:
    void ClearContexts();
    /// Higher priority is merged later (same key can appear in multiple contexts).
    void AddMappingContext(InputMappingContext context, int priority = 0);

    /// Rebuild effective binds + sample Window state. Call once per frame after pollEvents.
    void Update(const FGenericWindow& window);

    [[nodiscard]] float GetAxisValue(std::string_view action) const;
    [[nodiscard]] bool IsActionPressed(std::string_view action) const;
    [[nodiscard]] bool WasActionJustPressed(std::string_view action) const;
    [[nodiscard]] bool WasActionJustReleased(std::string_view action) const;

    /// Convenience: MoveRight (x) + MoveForward (z) from the active map.
    [[nodiscard]] MoveAxes2D GetMoveAxes2D() const;

private:
    struct ContextEntry {
        int priority = 0;
        InputMappingContext context;
    };

    void rebuildEffectiveMaps();

    std::vector<ContextEntry> contexts_;
    std::unordered_map<std::string, std::vector<InputAxisKey>> effectiveAxes_;
    std::unordered_map<std::string, std::vector<int>> effectiveActions_;

    std::unordered_map<std::string, float> axisValues_;
    std::unordered_map<std::string, bool> actionPressed_;
    std::unordered_map<std::string, bool> actionPressedPrev_;
    bool mapsDirty_ = true;
};

