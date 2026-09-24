#include <algorithm>
#include "InputCoreTypes.h"
#include "GameFramework/InputActions.h"
#include "GameFramework/InputMapping.h"

void InputMappingContext::BindAxisKey(std::string_view action, int key, float scale) {
    if (action.empty() || key == 0) {
        return;
    }
    axes_[std::string(action)].push_back(InputAxisKey{key, scale});
}

void InputMappingContext::BindActionKey(std::string_view action, int key) {
    if (action.empty() || key == 0) {
        return;
    }
    actions_[std::string(action)].push_back(key);
}

void InputMappingContext::BindAxisKey(std::string_view action, EKeys key, float scale) {
    BindAxisKey(action, ToKeyCode(key), scale);
}

void InputMappingContext::BindActionKey(std::string_view action, EKeys key) {
    BindActionKey(action, ToKeyCode(key));
}

InputMappingContext InputMappingContext::MakeDefault() {
    InputMappingContext ctx;
    using namespace Leon::InputActions;

    ctx.BindAxisKey(MoveForward, EKeys::W, 1.0f);
    ctx.BindAxisKey(MoveForward, EKeys::Up, 1.0f);
    ctx.BindAxisKey(MoveForward, EKeys::S, -1.0f);
    ctx.BindAxisKey(MoveForward, EKeys::Down, -1.0f);

    ctx.BindAxisKey(MoveRight, EKeys::D, 1.0f);
    ctx.BindAxisKey(MoveRight, EKeys::Right, 1.0f);
    ctx.BindAxisKey(MoveRight, EKeys::A, -1.0f);
    ctx.BindAxisKey(MoveRight, EKeys::Left, -1.0f);

    ctx.BindAxisKey(MoveUp, EKeys::E, 1.0f);
    ctx.BindAxisKey(MoveUp, EKeys::Q, -1.0f);

    ctx.BindActionKey(Jump, EKeys::SpaceBar);
    return ctx;
}

void PlayerInput::ClearContexts() {
    contexts_.clear();
    mapsDirty_ = true;
}

void PlayerInput::AddMappingContext(InputMappingContext context, int priority) {
    contexts_.push_back(ContextEntry{.priority = priority, .context = std::move(context)});
    std::stable_sort(
        contexts_.begin(), contexts_.end(),
        [](const ContextEntry& a, const ContextEntry& b) { return a.priority < b.priority; });
    mapsDirty_ = true;
}

void PlayerInput::rebuildEffectiveMaps() {
    effectiveAxes_.clear();
    effectiveActions_.clear();
    for (const ContextEntry& entry : contexts_) {
        for (const auto& [name, keys] : entry.context.Axes()) {
            auto& dst = effectiveAxes_[name];
            dst.insert(dst.end(), keys.begin(), keys.end());
        }
        for (const auto& [name, keys] : entry.context.Actions()) {
            auto& dst = effectiveActions_[name];
            dst.insert(dst.end(), keys.begin(), keys.end());
        }
    }
    mapsDirty_ = false;
}

void PlayerInput::Update(const FGenericWindow& window) {
    if (mapsDirty_) {
        rebuildEffectiveMaps();
    }

    actionPressedPrev_ = actionPressed_;
    axisValues_.clear();
    actionPressed_.clear();

    for (const auto& [name, keys] : effectiveAxes_) {
        float value = 0.0f;
        for (const InputAxisKey& binding : keys) {
            if (window.IsKeyPressed(static_cast<EKeys>(binding.key))) {
                value += binding.scale;
            }
        }
        value = std::clamp(value, -1.0f, 1.0f);
        axisValues_[name] = value;
    }

    for (const auto& [name, keys] : effectiveActions_) {
        bool pressed = false;
        for (int key : keys) {
            if (window.IsKeyPressed(static_cast<EKeys>(key))) {
                pressed = true;
                break;
            }
        }
        actionPressed_[name] = pressed;
    }
}

float PlayerInput::GetAxisValue(std::string_view action) const {
    const auto it = axisValues_.find(std::string(action));
    return it != axisValues_.end() ? it->second : 0.0f;
}

bool PlayerInput::IsActionPressed(std::string_view action) const {
    const auto it = actionPressed_.find(std::string(action));
    return it != actionPressed_.end() && it->second;
}

bool PlayerInput::WasActionJustPressed(std::string_view action) const {
    const std::string key(action);
    const auto cur = actionPressed_.find(key);
    const bool now = cur != actionPressed_.end() && cur->second;
    if (!now) {
        return false;
    }
    const auto prev = actionPressedPrev_.find(key);
    const bool was = prev != actionPressedPrev_.end() && prev->second;
    return !was;
}

bool PlayerInput::WasActionJustReleased(std::string_view action) const {
    const std::string key(action);
    const auto cur = actionPressed_.find(key);
    const bool now = cur != actionPressed_.end() && cur->second;
    if (now) {
        return false;
    }
    const auto prev = actionPressedPrev_.find(key);
    return prev != actionPressedPrev_.end() && prev->second;
}

MoveAxes2D PlayerInput::GetMoveAxes2D() const {
    MoveAxes2D axes{};
    axes.x = GetAxisValue(Leon::InputActions::MoveRight);
    axes.z = GetAxisValue(Leon::InputActions::MoveForward);
    return axes;
}

