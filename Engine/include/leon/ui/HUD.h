#pragma once

#include <leon/ui/UserWidget.h>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace leon {

class DebugOverlay;

/// Unreal-like AHUD: owns UserWidgets painted each frame into screen geometry.
class HUD {
public:
    void Clear();

    /// Unreal `CreateWidget` + `AddToViewport` (lite): construct, NativeConstruct, retain.
    template <typename T, typename... Args>
    T* AddWidget(Args&&... args) {
        static_assert(std::is_base_of_v<UserWidget, T>, "T must derive from leon::UserWidget");
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        raw->owningHud_ = this;
        raw->NativeConstruct();
        widgets_.push_back(std::move(owned));
        return raw;
    }

    /// Remove first widget of type T (NativeDestruct). Returns true if removed.
    template <typename T>
    bool RemoveWidget() {
        static_assert(std::is_base_of_v<UserWidget, T>, "T must derive from leon::UserWidget");
        for (auto it = widgets_.begin(); it != widgets_.end(); ++it) {
            if (dynamic_cast<T*>(it->get()) != nullptr) {
                (*it)->NativeDestruct();
                (*it)->owningHud_ = nullptr;
                widgets_.erase(it);
                return true;
            }
        }
        return false;
    }

    bool RemoveWidget(UserWidget* widget);

    template <typename T>
    [[nodiscard]] T* GetWidgetOfClass() const {
        static_assert(std::is_base_of_v<UserWidget, T>, "T must derive from leon::UserWidget");
        for (const auto& w : widgets_) {
            if (T* typed = dynamic_cast<T*>(w.get())) {
                return typed;
            }
        }
        return nullptr;
    }

    void Tick(float deltaTime);

    /// Clears prior frame screen geometry, then paints visible widgets.
    void Paint(DebugOverlay& overlay, int framebufferWidth, int framebufferHeight);

    [[nodiscard]] const std::vector<std::unique_ptr<UserWidget>>& Widgets() const {
        return widgets_;
    }

private:
    std::vector<std::unique_ptr<UserWidget>> widgets_;
};

} // namespace leon
