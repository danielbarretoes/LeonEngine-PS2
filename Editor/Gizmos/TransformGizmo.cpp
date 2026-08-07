#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <ImGuizmo.h>
#include <leon/editor/TransformGizmo.h>

namespace leon::editor {

bool TransformGizmo::Manipulate(EditorContext& ctx, const glm::mat4& view,
                                const glm::mat4& projection, Transform& transform) {
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x,
                      ImGui::GetWindowPos().y + ImGui::GetWindowContentRegionMin().y,
                      static_cast<float>(ctx.viewportWidth),
                      static_cast<float>(ctx.viewportHeight));

    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    switch (ctx.gizmoOp) {
    case EGizmoOperation::Translate:
        op = ImGuizmo::TRANSLATE;
        break;
    case EGizmoOperation::Rotate:
        op = ImGuizmo::ROTATE;
        break;
    case EGizmoOperation::Scale:
        op = ImGuizmo::SCALE;
        break;
    }

    const ImGuizmo::MODE mode =
        ctx.gizmoSpace == EGizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    glm::mat4 matrix = transform.modelMatrix();
    float* matrixPtr = glm::value_ptr(matrix);
    const float* viewPtr = glm::value_ptr(view);
    // ImGuizmo expects column-major; GLM is column-major. Projection may need transpose for RH.
    const float* projPtr = glm::value_ptr(projection);

    if (!ImGuizmo::Manipulate(viewPtr, projPtr, op, mode, matrixPtr)) {
        return false;
    }

    float translation[3];
    float rotation[3];
    float scale[3];
    ImGuizmo::DecomposeMatrixToComponents(matrixPtr, translation, rotation, scale);
    transform.position = {translation[0], translation[1], translation[2]};
    transform.rotationDegrees = {rotation[0], rotation[1], rotation[2]};
    transform.scale = {scale[0], scale[1], scale[2]};
    ctx.MarkDirty();
    return true;
}

} // namespace leon::editor
