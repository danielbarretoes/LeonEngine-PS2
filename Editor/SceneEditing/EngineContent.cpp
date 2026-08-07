#include <leon/editor/EngineContent.h>

#include <leon/core/Paths.h>
#include <leon/level/BasicShape.h>
#include <leon/level/Level.h>

namespace leon::editor {
namespace {

constexpr std::string_view kEnginePrefix = "leon:Engine/";

const char* MaterialPathForShape(EBasicShape shape) {
    return shape == EBasicShape::Plane ? "Materials/M_WorldGrid.lmat" : "Materials/M_Default.lmat";
}

const char* ClassNameForShape(EBasicShape shape) {
    switch (shape) {
    case EBasicShape::Sphere:
        return "Sphere";
    case EBasicShape::Plane:
        return "Plane";
    case EBasicShape::Cube:
    default:
        return "Cube";
    }
}

} // namespace

bool IsEngineContentPath(std::string_view path) {
    return path.starts_with(kEnginePrefix) || path.starts_with("leon:Engine");
}

const std::vector<EngineContentItem>& EngineContentCatalog() {
    static const std::vector<EngineContentItem> kItems = {
        {"leon:Engine/BasicShapes", "Basic Shapes", EngineContentItem::Kind::Folder},
        {"leon:Engine/BasicShapes/Cube", "Cube", EngineContentItem::Kind::Primitive},
        {"leon:Engine/BasicShapes/Sphere", "Sphere", EngineContentItem::Kind::Primitive},
        {"leon:Engine/BasicShapes/Plane", "Plane", EngineContentItem::Kind::Primitive},
        {"leon:Engine/Materials", "Materials", EngineContentItem::Kind::Folder},
        {"leon:Engine/Materials/M_Default", "M_Default", EngineContentItem::Kind::Material},
        {"leon:Engine/Materials/M_WorldGrid", "M_WorldGrid", EngineContentItem::Kind::Material},
        {"leon:Engine/Hdr", "HDR", EngineContentItem::Kind::Folder},
        {"leon:Engine/Hdr/DefaultSky", "DefaultSky", EngineContentItem::Kind::Sky},
    };
    return kItems;
}

bool PlaceBasicShapeActor(EditorContext& ctx, EBasicShape shape, const glm::vec3& worldPos,
                          std::string& outError) {
    outError.clear();
    if (ctx.level == nullptr || ctx.resources == nullptr) {
        outError = "No level open.";
        return false;
    }

    const char* className = ClassNameForShape(shape);
    const char* matPath = MaterialPathForShape(shape);

    BasicShape basic;
    basic.type = shape;
    basic.transform.position = worldPos;
    if (shape == EBasicShape::Plane) {
        basic.transform.scale = {4.0f, 1.0f, 4.0f};
    } else {
        basic.transform.position.y = worldPos.y + 0.5f;
    }

    StaticMeshComponent mesh = basic.MakeStaticMesh(*ctx.resources);
    mesh.editorClass = className;
    mesh.material = ctx.resources->LoadMaterial(ResolveAssetPath(matPath));
    if (!mesh.material.albedoMap) {
        mesh.material = ctx.resources->DefaultMaterial();
    }
    mesh.materialOverride = true;
    mesh.materialPath = matPath;
    mesh.collisionEnabled = true;

    ctx.level->AddStaticMesh(std::move(mesh));
    ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1);
    ctx.MarkDirty();
    return true;
}

bool ApplyEngineContent(EditorContext& ctx, std::string_view path, const glm::vec3& worldPos,
                        std::string& outError) {
    outError.clear();
    if (ctx.level == nullptr || ctx.resources == nullptr) {
        outError = "No level open.";
        return false;
    }
    if (!IsEngineContentPath(path)) {
        outError = "Not an Engine content path.";
        return false;
    }

    const std::string p(path);
    if (p == "leon:Engine/BasicShapes/Cube" || p == "leon:Engine/BasicShapes/Sphere" ||
        p == "leon:Engine/BasicShapes/Plane") {
        EBasicShape shape = EBasicShape::Cube;
        if (p.ends_with("/Sphere")) {
            shape = EBasicShape::Sphere;
        } else if (p.ends_with("/Plane")) {
            shape = EBasicShape::Plane;
        }
        return PlaceBasicShapeActor(ctx, shape, worldPos, outError);
    }

    if (p == "leon:Engine/Materials/M_Default" || p == "leon:Engine/Materials/M_WorldGrid") {
        if (ctx.selection.kind == EEditorSelectionKind::StaticMesh &&
            ctx.selection.index < ctx.level->StaticMeshes().size()) {
            StaticMeshComponent& mesh = ctx.level->StaticMeshes()[ctx.selection.index];
            const char* matPath = p.ends_with("M_WorldGrid") ? "Materials/M_WorldGrid.lmat"
                                                             : "Materials/M_Default.lmat";
            mesh.material = ctx.resources->LoadMaterial(ResolveAssetPath(matPath));
            if (!mesh.material.albedoMap && !p.ends_with("M_WorldGrid")) {
                mesh.material = ctx.resources->DefaultMaterial();
            }
            mesh.materialOverride = true;
            mesh.materialPath = matPath;
            ctx.MarkDirty();
            return true;
        }
        outError = "Select a StaticMesh to apply the material.";
        return false;
    }

    if (p == "leon:Engine/Hdr/DefaultSky") {
        const std::string hdrRel = "Hdr/AutumnFieldPuresky1k.hdr";
        const std::string hdrPath = ResolveAssetPath(hdrRel);
        if (hdrPath.empty()) {
            outError = "Default sky HDR missing.";
            return false;
        }
        auto env = ctx.resources->LoadEnvMap(hdrPath);
        if (env == nullptr) {
            outError = "Failed to load default sky.";
            return false;
        }
        ctx.level->SetEnvironment(std::move(env));
        ctx.level->SetEnvironmentPath(hdrRel);
        ctx.MarkDirty();
        return true;
    }

    outError = "Unsupported Engine content.";
    return false;
}

} // namespace leon::editor
