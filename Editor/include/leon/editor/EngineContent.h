#pragma once

#include <glm/vec3.hpp>
#include <leon/editor/EditorContext.h>
#include <leon/level/BasicShape.h>
#include <string>
#include <string_view>
#include <vector>

namespace leon::editor {

/// Virtual Unreal-like `/Engine/...` content shown in the Content Browser.
struct EngineContentItem {
    std::string path; // e.g. "leon:Engine/BasicShapes/Cube"
    std::string name;
    enum class Kind {
        Folder,
        Primitive,
        Material,
        Sky,
    } kind = Kind::Primitive;
};

[[nodiscard]] bool IsEngineContentPath(std::string_view path);
[[nodiscard]] const std::vector<EngineContentItem>& EngineContentCatalog();

/// Place a BasicShape actor with engine material + texture (Unreal-like /Engine/BasicShapes).
/// Cube/Sphere → M_Default; Plane → M_WorldGrid. Persists `materialPath` for level save.
[[nodiscard]] bool PlaceBasicShapeActor(EditorContext& ctx, leon::EBasicShape shape,
                                        const glm::vec3& worldPos, std::string& outError);

/// Place / apply an engine content item at `worldPos` (primitives) or on the level (sky/material).
[[nodiscard]] bool ApplyEngineContent(EditorContext& ctx, std::string_view path,
                                      const glm::vec3& worldPos, std::string& outError);

} // namespace leon::editor
