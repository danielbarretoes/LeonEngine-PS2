#pragma once

#include <glm/mat4x4.hpp>

#include <leon/core/Transform.h>
#include <leon/editor/EditorContext.h>

namespace leon::editor {

/// Thin ImGuizmo wrapper: Translate / Rotate / Scale a `Transform` in the Viewport.
class TransformGizmo {
public:
    /// Call while the Viewport ImGui window is current (typically after `ImGui::Image`).
    /// Returns true when the matrix was edited this frame.
    [[nodiscard]] bool Manipulate(EditorContext& ctx, const glm::mat4& view,
                                  const glm::mat4& projection, leon::Transform& transform);
};

} // namespace leon::editor
