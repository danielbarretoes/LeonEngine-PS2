#pragma once

#include <glm/vec3.hpp>
#include "Fonts/TextLayout.h"
#include "Blueprint/UserWidget.h"
#include <string>

namespace leon {

class Window;

/// Unreal-like UButton (lite): filled rect + label; hover / selected / press.
/// Usually owned by VerticalBoxWidget; can also be a root HUD widget with SetPosition.
class ButtonWidget : public UserWidget {
public:
    void SetId(std::string id) { id_ = std::move(id); }
    [[nodiscard]] const std::string& GetId() const { return id_; }

    void SetLabel(std::string label) { label_ = std::move(label); }
    [[nodiscard]] const std::string& GetLabel() const { return label_; }

    void SetPosition(float x, float y) {
        x_ = x;
        y_ = y;
    }
    void SetSize(float w, float h) {
        w_ = w;
        h_ = h;
    }

    [[nodiscard]] float GetX() const { return x_; }
    [[nodiscard]] float GetY() const { return y_; }
    [[nodiscard]] float GetWidth() const { return w_; }
    [[nodiscard]] float GetHeight() const { return h_; }

    void SetSelected(bool selected) { selected_ = selected; }
    [[nodiscard]] bool IsSelected() const { return selected_; }

    void SetEnabled(bool enabled) { enabled_ = enabled; }
    [[nodiscard]] bool IsEnabled() const { return enabled_; }

    void SetTextColor(const glm::vec3& color) { textColor_ = color; }
    void SetBackgroundColor(const glm::vec3& color) { backgroundColor_ = color; }
    void SetSelectedBackgroundColor(const glm::vec3& color) { selectedBackgroundColor_ = color; }

    /// Preferred size for the current label (padding included).
    void MeasureDesiredSize(float& outW, float& outH) const;

    /// Hit-test in framebuffer pixels (top-left origin).
    [[nodiscard]] bool Contains(float fbX, float fbY) const;

    void NativePaint(WidgetPaintContext& ctx) override;

private:
    std::string id_;
    std::string label_ = "Button";
    float x_ = 0.0f;
    float y_ = 0.0f;
    float w_ = 160.0f;
    float h_ = kHudLineHeight + 16.0f;
    bool selected_ = false;
    bool enabled_ = true;
    bool hovered_ = false;

    glm::vec3 textColor_{1.0f, 0.92f, 0.75f};
    glm::vec3 backgroundColor_{0.12f, 0.12f, 0.14f};
    glm::vec3 selectedBackgroundColor_{0.28f, 0.22f, 0.10f};
    glm::vec3 hoverBackgroundColor_{0.18f, 0.16f, 0.12f};
    glm::vec3 disabledTextColor_{0.45f, 0.45f, 0.45f};

    friend class VerticalBoxWidget;
    void SetHovered(bool hovered) { hovered_ = hovered; }
};

} // namespace leon
