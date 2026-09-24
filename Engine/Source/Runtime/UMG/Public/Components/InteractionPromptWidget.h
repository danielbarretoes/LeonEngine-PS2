#pragma once

#include <glm/vec3.hpp>
#include "Fonts/TextLayout.h"
#include "Blueprint/UserWidget.h"
#include <string>


/// Centered outlined interact hint ("[F] Open Door [750]"). Empty Prompt skips paint.
class InteractionPromptWidget : public UserWidget {
public:
    std::string Prompt;

    glm::vec3 Color{0.95f, 0.9f, 0.45f};
    float Scale = 2.4f;
    /// Vertical placement as a fraction of viewport height (0 = top, 1 = bottom).
    float NormalizedY = 0.62f;
    ETextJustify Justify = ETextJustify::Center;

    void NativePaint(WidgetPaintContext& ctx) override;
};

