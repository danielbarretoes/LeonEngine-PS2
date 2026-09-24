#pragma once

#include "Blueprint/UserWidget.h"
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>


class FDebugOverlay;

/// Unreal-like AHUD: owns UserWidgets painted each frame into screen geometry.
class ENGINE_API AHUD {
public:
    void Clear();

    /// Unreal `CreateWidget` + `AddToViewport` (lite): construct, NativeConstruct, retain.
    template <typename T, typename... ArgsType>
    T* AddWidget(ArgsType&&... Args) {
        static_assert(std::is_base_of_v<UUserWidget, T>, "T must derive from UserWidget");
        auto Owned = std::make_unique<T>(std::forward<ArgsType>(Args)...);
        T* Raw = Owned.get();
        Raw->OwningHud = this;
        Raw->NativeConstruct();
        Widgets.push_back(std::move(Owned));
        return Raw;
    }

    /// Remove first widget of type T (NativeDestruct). Returns true if removed.
    template <typename T>
    bool RemoveWidget() {
        static_assert(std::is_base_of_v<UUserWidget, T>, "T must derive from UserWidget");
        for (auto It = Widgets.begin(); It != Widgets.end(); ++It) {
            if (dynamic_cast<T*>(It->get()) != nullptr) {
                (*It)->NativeDestruct();
                (*It)->OwningHud = nullptr;
                Widgets.erase(It);
                return true;
            }
        }
        return false;
    }

    bool RemoveWidget(UUserWidget* Widget);

    template <typename T>
    [[nodiscard]] T* GetWidgetOfClass() const {
        static_assert(std::is_base_of_v<UUserWidget, T>, "T must derive from UserWidget");
        for (const auto& W : Widgets) {
            if (T* Typed = dynamic_cast<T*>(W.get())) {
                return Typed;
            }
        }
        return nullptr;
    }

    void Tick(float DeltaTime);

    /// Clears prior frame screen geometry, then paints visible widgets.
    void Paint(FDebugOverlay& Overlay, int FramebufferWidth, int FramebufferHeight);

    [[nodiscard]] const std::vector<std::unique_ptr<UUserWidget>>& GetWidgets() const {
        return Widgets;
    }

private:
    std::vector<std::unique_ptr<UUserWidget>> Widgets;
};

