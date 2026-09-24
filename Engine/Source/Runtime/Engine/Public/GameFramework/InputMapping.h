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
struct FInputAxisKeyMapping {
    int Key = 0;        // EKeys underlying code (Host matches GLFW)
    float Scale = 1.0f; // typically +1 or -1
};

/// Maps action names → keys (Unreal-like Input Mapping Context).
class UInputMappingContext {
public:
    /// Bind a key that contributes `scale` to a named axis while held.
    void BindAxisKey(std::string_view Action, int InKey, float InScale = 1.0f);
    void BindAxisKey(std::string_view Action, EKeys InKey, float InScale = 1.0f);

    /// Bind a digital action key (pressed / just-pressed queries).
    void BindActionKey(std::string_view Action, int InKey);
    void BindActionKey(std::string_view Action, EKeys InKey);

    [[nodiscard]] const std::unordered_map<std::string, std::vector<FInputAxisKeyMapping>>& GetAxes() const {
        return Axes;
    }
    [[nodiscard]] const std::unordered_map<std::string, std::vector<int>>& GetActions() const {
        return Actions;
    }

    /// Default Leon gameplay map: WASD+arrows move, Q/E up, Space jump.
    [[nodiscard]] static UInputMappingContext MakeDefault();

private:
    std::unordered_map<std::string, std::vector<FInputAxisKeyMapping>> Axes;
    std::unordered_map<std::string, std::vector<int>> Actions;
};

/// Samples mapped input once per frame (Unreal-like UPlayerInput).
class UPlayerInput {
public:
    void ClearContexts();
    /// Higher priority is merged later (same key can appear in multiple contexts).
    void AddMappingContext(UInputMappingContext InContext, int InPriority = 0);

    /// Rebuild effective binds + sample Window state. Call once per frame after pollEvents.
    void Update(const FGenericWindow& Window);

    [[nodiscard]] float GetAxisValue(std::string_view Action) const;
    [[nodiscard]] bool IsActionPressed(std::string_view Action) const;
    [[nodiscard]] bool WasActionJustPressed(std::string_view Action) const;
    [[nodiscard]] bool WasActionJustReleased(std::string_view Action) const;

    /// Convenience: MoveRight (x) + MoveForward (z) from the active map.
    [[nodiscard]] FMoveAxes2D GetMoveAxes2D() const;

private:
    struct FContextEntry {
        int Priority = 0;
        UInputMappingContext Context;
    };

    void RebuildEffectiveMaps();

    std::vector<FContextEntry> Contexts;
    std::unordered_map<std::string, std::vector<FInputAxisKeyMapping>> EffectiveAxes;
    std::unordered_map<std::string, std::vector<int>> EffectiveActions;

    std::unordered_map<std::string, float> AxisValues;
    std::unordered_map<std::string, bool> ActionPressed;
    std::unordered_map<std::string, bool> ActionPressedPrev;
    bool bMapsDirty = true;
};

