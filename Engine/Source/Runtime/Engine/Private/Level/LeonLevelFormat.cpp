#include "Level/LeonLevelFormat.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/GameEngine.h"
#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Level/LevelAnimation.h"
#include "Level/LevelLoader.h"
#include "Level/Light.h"
#include "Level/LightmapIO.h"
#include <unordered_map>
#include <utility>

namespace {
namespace fs = std::filesystem;

// Caps on file-controlled allocations (corrupt / hostile .llev).
constexpr std::uint32_t kMaxLeonLevelStrings = 65536u;
constexpr std::uint32_t kMaxLeonLevelActors = 100000u;
constexpr std::uint32_t kMaxLeonLevelLights = 16384u;
constexpr std::uint32_t kMaxLeonStringBytes = 1u << 20; // 1 MiB per string

// --- Little-endian primitive writers ---

void WriteU8(std::vector<std::uint8_t>& out, std::uint8_t value) {
    out.push_back(value);
}

void WriteU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void WriteU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

void WriteI32(std::vector<std::uint8_t>& out, std::int32_t value) {
    WriteU32(out, static_cast<std::uint32_t>(value));
}

void WriteF32(std::vector<std::uint8_t>& out, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    WriteU32(out, bits);
}

void WriteVec3(std::vector<std::uint8_t>& out, const glm::vec3& value) {
    WriteF32(out, value.x);
    WriteF32(out, value.y);
    WriteF32(out, value.z);
}

/// Deduplicating string table; index 0 is always the empty string.
class StringTableBuilder {
public:
    StringTableBuilder() { (void)Add(std::string{}); }

    std::uint32_t Add(const std::string& value) {
        const auto it = lookup_.find(value);
        if (it != lookup_.end()) {
            return it->second;
        }
        const auto index = static_cast<std::uint32_t>(strings_.size());
        strings_.push_back(value);
        lookup_.emplace(value, index);
        return index;
    }

    void WriteTo(std::vector<std::uint8_t>& out) const {
        WriteU32(out, static_cast<std::uint32_t>(strings_.size()));
        for (const std::string& value : strings_) {
            WriteU32(out, static_cast<std::uint32_t>(value.size()));
            out.insert(out.end(), value.begin(), value.end());
        }
    }

private:
    std::vector<std::string> strings_;
    std::unordered_map<std::string, std::uint32_t> lookup_;
};

/// Bounds-checked little-endian cursor; any overrun latches `failed_`.
class ByteReader {
public:
    explicit ByteReader(const std::vector<std::uint8_t>& bytes)
        : bytes_(bytes.data()), size_(bytes.size()) {}

    [[nodiscard]] bool Failed() const { return failed_; }

    std::uint8_t ReadU8() {
        if (!Require(1)) {
            return 0;
        }
        return bytes_[cursor_++];
    }

    std::uint16_t ReadU16() {
        if (!Require(2)) {
            return 0;
        }
        const auto value = static_cast<std::uint16_t>(bytes_[cursor_] |
                                                      (bytes_[cursor_ + 1] << 8));
        cursor_ += 2;
        return value;
    }

    std::uint32_t ReadU32() {
        if (!Require(4)) {
            return 0;
        }
        const std::uint32_t value = static_cast<std::uint32_t>(bytes_[cursor_]) |
                                    (static_cast<std::uint32_t>(bytes_[cursor_ + 1]) << 8) |
                                    (static_cast<std::uint32_t>(bytes_[cursor_ + 2]) << 16) |
                                    (static_cast<std::uint32_t>(bytes_[cursor_ + 3]) << 24);
        cursor_ += 4;
        return value;
    }

    std::int32_t ReadI32() { return static_cast<std::int32_t>(ReadU32()); }

    float ReadF32() {
        const std::uint32_t bits = ReadU32();
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    glm::vec3 ReadVec3() {
        glm::vec3 value{0.0f};
        value.x = ReadF32();
        value.y = ReadF32();
        value.z = ReadF32();
        return value;
    }

    std::string ReadBytes(std::size_t count) {
        if (!Require(count)) {
            return {};
        }
        std::string value(reinterpret_cast<const char*>(bytes_ + cursor_), count);
        cursor_ += count;
        return value;
    }

    void Skip(std::size_t count) {
        if (Require(count)) {
            cursor_ += count;
        }
    }

private:
    bool Require(std::size_t count) {
        if (failed_ || cursor_ + count > size_) {
            failed_ = true;
            return false;
        }
        return true;
    }

    const std::uint8_t* bytes_ = nullptr;
    std::size_t size_ = 0;
    std::size_t cursor_ = 0;
    bool failed_ = false;
};

} // namespace (helpers continue after ResolveLevelAssetPath)

std::string ResolveLevelAssetPath(const std::string& levelPath, const std::string& relativeOrKey) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (relativeOrKey.empty()) {
        return {};
    }
    const fs::path key(relativeOrKey);
    if (key.is_absolute() && fs::exists(key, ec) && !ec) {
        return key.lexically_normal().string();
    }

    // `<pack>/Content/Levels/Main.llev` → content root `<pack>/Content`
    // (legacy `<pack>/Levels/Main.llev` → `<pack>/`).
    const fs::path contentRoot = fs::path(levelPath).parent_path().parent_path();
    const fs::path inPack = (contentRoot / key).lexically_normal();
    if (fs::exists(inPack, ec) && !ec) {
        return inPack.string();
    }

    // Legacy `Projects/<name>/Materials/...` after the pack was copied elsewhere.
    const auto matPos = relativeOrKey.find("Materials/");
    if (matPos != std::string::npos) {
        const fs::path legacy = (contentRoot / relativeOrKey.substr(matPos)).lexically_normal();
        if (fs::exists(legacy, ec) && !ec) {
            return legacy.string();
        }
    }

    return FPaths::ResolveAssetPath(relativeOrKey);
}

namespace {

[[nodiscard]] ELevelActorClass ActorClassFromEditorClass(const std::string& editorClass) {
    if (editorClass == "Cube") {
        return ELevelActorClass::Cube;
    }
    if (editorClass == "Sphere") {
        return ELevelActorClass::Sphere;
    }
    if (editorClass == "Plane") {
        return ELevelActorClass::Plane;
    }
    if (editorClass == "BlockingVolume") {
        return ELevelActorClass::BlockingVolume;
    }
    if (editorClass == "TriggerVolume") {
        return ELevelActorClass::TriggerVolume;
    }
    if (editorClass == "PainCausingVolume") {
        return ELevelActorClass::PainCausingVolume;
    }
    if (editorClass == "AISpawnPoint") {
        return ELevelActorClass::AISpawnPoint;
    }
    if (editorClass == "PlayerStart") {
        return ELevelActorClass::PlayerStart;
    }
    return ELevelActorClass::StaticMesh;
}

[[nodiscard]] const char* EditorClassFromActorClass(ELevelActorClass actorClass) {
    switch (actorClass) {
    case ELevelActorClass::Cube:
        return "Cube";
    case ELevelActorClass::Sphere:
        return "Sphere";
    case ELevelActorClass::Plane:
        return "Plane";
    case ELevelActorClass::BlockingVolume:
        return "BlockingVolume";
    case ELevelActorClass::TriggerVolume:
        return "TriggerVolume";
    case ELevelActorClass::PainCausingVolume:
        return "PainCausingVolume";
    case ELevelActorClass::AISpawnPoint:
        return "AISpawnPoint";
    case ELevelActorClass::PlayerStart:
        return "PlayerStart";
    case ELevelActorClass::StaticMesh:
        break;
    }
    return "StaticMesh";
}

/// Basic shape backing a stored actor class; false for PlayerStart / AISpawnPoint / UStaticMesh.
/// TriggerVolume / PainCausingVolume map to Cube (editor debug mesh); runtime apply uses PODs only.
[[nodiscard]] bool BasicShapeForActorClass(ELevelActorClass actorClass, EBasicShape& outShape) {
    switch (actorClass) {
    case ELevelActorClass::Cube:
    case ELevelActorClass::BlockingVolume:
    case ELevelActorClass::TriggerVolume:
    case ELevelActorClass::PainCausingVolume:
        outShape = EBasicShape::Cube;
        return true;
    case ELevelActorClass::Sphere:
        outShape = EBasicShape::Sphere;
        return true;
    case ELevelActorClass::Plane:
        outShape = EBasicShape::Plane;
        return true;
    default:
        return false;
    }
}

void ApplyDocumentLights(const LevelDocument& doc, Level& staged, LevelAnimation& anim) {
    for (const LevelLightRecord& record : doc.lights) {
        BasicLight light;
        light.type = record.lightClass == ELevelLightClass::PointLight ? EBasicLight::Point
                                                                       : EBasicLight::Directional;
        light.transform.Position = record.position;
        light.transform.RotationDegrees = record.rotationDegrees;
        light.lightColor = record.lightColor;
        light.intensity = record.intensity;
        light.castShadows = record.castShadows;
        light.sourceAngle = record.sourceAngle;
        light.range = record.range;

        if (light.type != EBasicLight::Point) {
            light.addTo(staged);
            continue;
        }

        const std::size_t lightIndex = staged.PointLights().size();
        light.addTo(staged);
        if (!record.hasOrbit || lightIndex >= staged.PointLights().size()) {
            continue;
        }
        anim.orbits.push_back(LevelAnimation::PointOrbit{
            .lightIndex = lightIndex,
            .radius = record.orbitRadius,
            .height = record.orbitHeight,
            .heightAmp = record.orbitHeightAmp,
            .speed = record.orbitSpeed,
        });
        PointLight& live = staged.PointLights()[lightIndex];
        live.hasOrbit = true;
        live.orbitRadius = record.orbitRadius;
        live.orbitHeight = record.orbitHeight;
        live.orbitHeightAmp = record.orbitHeightAmp;
        live.orbitSpeed = record.orbitSpeed;
    }

    if (staged.DirectionalLights().empty()) {
        BasicLight::directional().addTo(staged);
    }
    if (staged.DirectionalLights().size() > static_cast<std::size_t>(kMaxDirectionalLights)) {
        std::cerr << "LeonLevelFormat: truncating directional lights from "
                  << staged.DirectionalLights().size() << " to " << kMaxDirectionalLights << '\n';
        staged.DirectionalLights().resize(static_cast<std::size_t>(kMaxDirectionalLights));
    }
    if (staged.PointLights().size() > static_cast<std::size_t>(kMaxPointLights)) {
        std::cerr << "LeonLevelFormat: truncating point lights from " << staged.PointLights().size()
                  << " to " << kMaxPointLights << '\n';
        staged.PointLights().resize(static_cast<std::size_t>(kMaxPointLights));
        anim.orbits.erase(std::remove_if(anim.orbits.begin(), anim.orbits.end(),
                                         [](const LevelAnimation::PointOrbit& orbit) {
                                             return orbit.lightIndex >=
                                                    static_cast<std::size_t>(kMaxPointLights);
                                         }),
                          anim.orbits.end());
    }
}

} // namespace

LevelDocument BuildLevelDocument(const Level& level, const Camera& camera) {
    LevelDocument doc;
    doc.name = level.Name();
    doc.gameMode = level.GameMode();
    doc.environmentPath = level.EnvironmentPath();
    doc.environmentExposure = level.EnvironmentExposure();

    doc.camera.mode = camera.Mode();
    doc.camera.target = camera.Target();
    doc.camera.eye = camera.EyeLocation();
    doc.camera.distance = camera.Distance();
    doc.camera.yaw = camera.YawDegrees();
    doc.camera.pitch = camera.PitchDegrees();

    for (const PlayerStart& start : level.PlayerStarts()) {
        LevelActorRecord record;
        record.actorClass = ELevelActorClass::PlayerStart;
        record.position = start.transform.Position;
        record.rotationDegrees = start.transform.RotationDegrees;
        record.scale = start.transform.Scale;
        record.enableGravity = false;
        doc.actors.push_back(std::move(record));
    }

    for (const AISpawnPoint& spawn : level.AISpawnPoints()) {
        LevelActorRecord record;
        record.actorClass = ELevelActorClass::AISpawnPoint;
        record.position = spawn.transform.Position;
        record.rotationDegrees = spawn.transform.RotationDegrees;
        record.scale = spawn.transform.Scale;
        record.tag = spawn.tag;
        record.enableGravity = false;
        doc.actors.push_back(std::move(record));
    }

    for (const TriggerVolume& volume : level.TriggerVolumes()) {
        LevelActorRecord record;
        record.actorClass = ELevelActorClass::TriggerVolume;
        record.position = volume.transform.Position;
        record.rotationDegrees = volume.transform.RotationDegrees;
        record.scale = volume.transform.Scale;
        record.tag = volume.tag;
        record.interactCost = volume.interactCost;
        record.interactRadius = volume.interactRadius;
        record.payload = volume.payload;
        record.bConsumeOnUse = volume.bConsumeOnUse;
        record.enableGravity = false;
        doc.actors.push_back(std::move(record));
    }

    for (const PainCausingVolume& volume : level.PainCausingVolumes()) {
        LevelActorRecord record;
        record.actorClass = ELevelActorClass::PainCausingVolume;
        record.position = volume.transform.Position;
        record.rotationDegrees = volume.transform.RotationDegrees;
        record.scale = volume.transform.Scale;
        record.tag = volume.tag;
        record.damagePerSecond = volume.damagePerSecond;
        record.damageInterval = volume.damageInterval;
        record.enableGravity = false;
        doc.actors.push_back(std::move(record));
    }

    for (const StaticMeshComponent& mesh : level.StaticMeshes()) {
        LevelActorRecord record;
        record.actorClass = ActorClassFromEditorClass(mesh.editorClass);
        // Imported meshes are only reloadable through their path.
        if (record.actorClass == ELevelActorClass::StaticMesh && mesh.meshPath.empty()) {
            std::cerr << "LeonLevelFormat: skipping StaticMesh actor without mesh path\n";
            continue;
        }

        record.mobility = mesh.mobility;
        record.collisionEnabled = mesh.collisionEnabled;
        record.simulatePhysics = mesh.simulatePhysics;
        record.enableGravity = mesh.enableGravity;
        record.hidden = mesh.hidden;

        record.position = mesh.transform.Position;
        record.rotationDegrees = mesh.transform.RotationDegrees;
        record.scale = mesh.transform.Scale;

        record.tag = mesh.tag;
        record.materialPath = mesh.materialPath;
        if (record.actorClass == ELevelActorClass::StaticMesh) {
            record.meshPath = mesh.meshPath;
        }
        record.lightmapId = mesh.lightmapId;
        record.lightmapPath = mesh.lightmapPath;
        record.lightmapResolution = static_cast<std::uint32_t>(std::max(0, mesh.lightmapResolution));

        record.sphereSegments = mesh.sphereSegments;
        record.sphereRings = mesh.sphereRings;

        record.hasSpinYaw = mesh.spinYaw != 0.0f;
        record.spinYaw = mesh.spinYaw;

        record.hasBob = mesh.hasBob;
        record.bobBaseY = mesh.bobBaseY;
        record.bobAmplitude = mesh.bobAmplitude;
        record.bobSpeed = mesh.bobSpeed;

        doc.actors.push_back(std::move(record));
    }

    for (const DirectionalLight& light : level.DirectionalLights()) {
        LevelLightRecord record;
        record.lightClass = ELevelLightClass::DirectionalLight;
        record.castShadows = light.castShadows;
        record.position = light.transform.Position;
        record.rotationDegrees = light.transform.RotationDegrees;
        record.lightColor = light.lightColor;
        record.intensity = light.intensity;
        record.sourceAngle = light.sourceAngle;
        doc.lights.push_back(record);
    }
    for (const PointLight& light : level.PointLights()) {
        LevelLightRecord record;
        record.lightClass = ELevelLightClass::PointLight;
        record.castShadows = light.castShadows;
        record.position = light.transform.Position;
        record.rotationDegrees = light.transform.RotationDegrees;
        record.lightColor = light.lightColor;
        record.intensity = light.intensity;
        record.range = light.range;
        record.hasOrbit = light.hasOrbit;
        record.orbitRadius = light.orbitRadius;
        record.orbitHeight = light.orbitHeight;
        record.orbitHeightAmp = light.orbitHeightAmp;
        record.orbitSpeed = light.orbitSpeed;
        doc.lights.push_back(record);
    }

    return doc;
}

std::vector<std::uint8_t> SerializeLeonLevel(const LevelDocument& doc) {
    StringTableBuilder strings;
    const std::uint32_t nameIdx = strings.Add(doc.name);
    const std::uint32_t gameModeIdx = strings.Add(doc.gameMode);
    const std::uint32_t environmentIdx = strings.Add(doc.environmentPath);

    struct ActorIndices {
        std::uint32_t flags = 0;
        std::uint32_t tag = 0;
        std::uint32_t material = 0;
        std::uint32_t mesh = 0;
        std::uint32_t lightmapId = 0;
        std::uint32_t lightmapPath = 0;
        std::uint32_t payload = 0;
    };
    std::vector<ActorIndices> actorIndices;
    actorIndices.reserve(doc.actors.size());
    for (const LevelActorRecord& actor : doc.actors) {
        ActorIndices indices;
        if (actor.collisionEnabled) {
            indices.flags |= kLevelActorFlagCollisionEnabled;
        }
        if (actor.simulatePhysics) {
            indices.flags |= kLevelActorFlagSimulatePhysics;
        }
        if (actor.enableGravity) {
            indices.flags |= kLevelActorFlagEnableGravity;
        }
        if (actor.hidden) {
            indices.flags |= kLevelActorFlagHidden;
        }
        if (actor.hasBob) {
            indices.flags |= kLevelActorFlagHasBob;
        }
        if (actor.hasSpinYaw) {
            indices.flags |= kLevelActorFlagHasSpinYaw;
        }
        if (actor.hasFitHeight) {
            indices.flags |= kLevelActorFlagHasFitHeight;
        }
        if (!actor.tag.empty()) {
            indices.flags |= kLevelActorFlagHasTag;
            indices.tag = strings.Add(actor.tag);
        }
        if (!actor.materialPath.empty()) {
            indices.flags |= kLevelActorFlagHasMaterial;
            indices.material = strings.Add(actor.materialPath);
        }
        if (!actor.meshPath.empty()) {
            indices.flags |= kLevelActorFlagHasMesh;
            indices.mesh = strings.Add(actor.meshPath);
        }
        if (!actor.lightmapId.empty()) {
            indices.flags |= kLevelActorFlagHasLightmapId;
            indices.lightmapId = strings.Add(actor.lightmapId);
        }
        if (!actor.lightmapPath.empty()) {
            indices.flags |= kLevelActorFlagHasLightmapPath;
            indices.lightmapPath = strings.Add(actor.lightmapPath);
        }
        if (actor.actorClass == ELevelActorClass::TriggerVolume || actor.interactCost != 0 ||
            actor.interactRadius != 2.0f) {
            indices.flags |= kLevelActorFlagHasInteractCost;
        }
        if (actor.actorClass == ELevelActorClass::PainCausingVolume ||
            actor.damagePerSecond != 12.0f || actor.damageInterval != 0.35f) {
            indices.flags |= kLevelActorFlagHasPainData;
        }
        if (!actor.payload.empty()) {
            indices.flags |= kLevelActorFlagHasPayload;
            indices.payload = strings.Add(actor.payload);
        }
        if (actor.bConsumeOnUse) {
            indices.flags |= kLevelActorFlagConsumeOnUse;
        }
        actorIndices.push_back(indices);
    }

    std::vector<std::uint8_t> out;
    WriteU32(out, kLeonLevelMagic);
    WriteU32(out, kLeonLevelVersion);
    WriteU32(out, 0u); // flags (reserved)

    strings.WriteTo(out);

    WriteU32(out, nameIdx);
    WriteU32(out, gameModeIdx);
    WriteU32(out, environmentIdx);
    WriteF32(out, doc.environmentExposure);

    WriteU8(out, static_cast<std::uint8_t>(doc.camera.mode));
    WriteU8(out, 0u);
    WriteU8(out, 0u);
    WriteU8(out, 0u);
    WriteVec3(out, doc.camera.target);
    WriteVec3(out, doc.camera.eye);
    WriteF32(out, doc.camera.distance);
    WriteF32(out, doc.camera.yaw);
    WriteF32(out, doc.camera.pitch);

    WriteU32(out, static_cast<std::uint32_t>(doc.actors.size()));
    for (std::size_t i = 0; i < doc.actors.size(); ++i) {
        const LevelActorRecord& actor = doc.actors[i];
        const ActorIndices& indices = actorIndices[i];

        WriteU8(out, static_cast<std::uint8_t>(actor.actorClass));
        WriteU8(out, static_cast<std::uint8_t>(actor.mobility));
        WriteU16(out, 0u);
        WriteU32(out, indices.flags);
        WriteVec3(out, actor.position);
        WriteVec3(out, actor.rotationDegrees);
        WriteVec3(out, actor.scale);

        if ((indices.flags & kLevelActorFlagHasTag) != 0u) {
            WriteU32(out, indices.tag);
        }
        if ((indices.flags & kLevelActorFlagHasMaterial) != 0u) {
            WriteU32(out, indices.material);
        }
        if ((indices.flags & kLevelActorFlagHasMesh) != 0u) {
            WriteU32(out, indices.mesh);
        }
        if ((indices.flags & kLevelActorFlagHasLightmapId) != 0u) {
            WriteU32(out, indices.lightmapId);
        }
        if ((indices.flags & kLevelActorFlagHasLightmapPath) != 0u) {
            WriteU32(out, indices.lightmapPath);
        }
        WriteU32(out, actor.lightmapResolution);

        if (actor.actorClass == ELevelActorClass::Sphere) {
            WriteI32(out, actor.sphereSegments);
            WriteI32(out, actor.sphereRings);
        }
        if ((indices.flags & kLevelActorFlagHasSpinYaw) != 0u) {
            WriteF32(out, actor.spinYaw);
        }
        if ((indices.flags & kLevelActorFlagHasBob) != 0u) {
            WriteF32(out, actor.bobBaseY);
            WriteF32(out, actor.bobAmplitude);
            WriteF32(out, actor.bobSpeed);
        }
        if ((indices.flags & kLevelActorFlagHasFitHeight) != 0u) {
            WriteF32(out, actor.fitHeight);
        }
        if ((indices.flags & kLevelActorFlagHasInteractCost) != 0u) {
            WriteI32(out, actor.interactCost);
            WriteF32(out, actor.interactRadius);
        }
        if ((indices.flags & kLevelActorFlagHasPainData) != 0u) {
            WriteF32(out, actor.damagePerSecond);
            WriteF32(out, actor.damageInterval);
        }
        if ((indices.flags & kLevelActorFlagHasPayload) != 0u) {
            WriteU32(out, indices.payload);
        }
    }

    WriteU32(out, static_cast<std::uint32_t>(doc.lights.size()));
    for (const LevelLightRecord& light : doc.lights) {
        std::uint32_t flags = 0;
        if (light.castShadows) {
            flags |= kLevelLightFlagCastShadows;
        }
        if (light.hasOrbit) {
            flags |= kLevelLightFlagHasOrbit;
        }

        WriteU8(out, static_cast<std::uint8_t>(light.lightClass));
        WriteU8(out, 0u);
        WriteU8(out, 0u);
        WriteU8(out, 0u);
        WriteU32(out, flags);
        WriteVec3(out, light.position);
        WriteVec3(out, light.rotationDegrees);
        WriteVec3(out, light.lightColor);
        WriteF32(out, light.intensity);
        WriteF32(out, light.range);
        WriteF32(out, light.sourceAngle);
        if ((flags & kLevelLightFlagHasOrbit) != 0u) {
            WriteF32(out, light.orbitRadius);
            WriteF32(out, light.orbitHeight);
            WriteF32(out, light.orbitHeightAmp);
            WriteF32(out, light.orbitSpeed);
        }
    }

    return out;
}

bool DeserializeLeonLevel(const std::vector<std::uint8_t>& bytes, LevelDocument& out) {
    out = LevelDocument{};

    ByteReader reader(bytes);
    if (reader.ReadU32() != kLeonLevelMagic) {
        std::cerr << "LeonLevelFormat: bad magic (expected 'LLEV')\n";
        return false;
    }
    const std::uint32_t version = reader.ReadU32();
    // Flow: .llev load — accept v1 (legacy) and v2 (typed volumes); reject unknown.
    if (version != 1u && version != 2u) {
        std::cerr << "LeonLevelFormat: unsupported version " << version
                  << " (expected 1 or 2)\n";
        return false;
    }
    (void)reader.ReadU32(); // flags (reserved)

    const std::uint32_t stringCount = reader.ReadU32();
    if (reader.Failed() || stringCount > kMaxLeonLevelStrings) {
        return false;
    }
    std::vector<std::string> strings;
    strings.reserve(stringCount);
    for (std::uint32_t i = 0; i < stringCount; ++i) {
        const std::uint32_t length = reader.ReadU32();
        if (reader.Failed() || length > kMaxLeonStringBytes) {
            return false;
        }
        strings.push_back(reader.ReadBytes(length));
        if (reader.Failed()) {
            return false;
        }
    }
    // Out-of-range indices resolve to the empty string instead of failing the load.
    const auto stringAt = [&strings](std::uint32_t index) -> std::string {
        return index < strings.size() ? strings[index] : std::string{};
    };

    out.name = stringAt(reader.ReadU32());
    out.gameMode = stringAt(reader.ReadU32());
    out.environmentPath = stringAt(reader.ReadU32());
    out.environmentExposure = reader.ReadF32();

    const std::uint8_t cameraMode = reader.ReadU8();
    reader.Skip(3);
    out.camera.mode = cameraMode == 1 ? ECameraMode::FreeLook : ECameraMode::Orbit;
    out.camera.target = reader.ReadVec3();
    out.camera.eye = reader.ReadVec3();
    out.camera.distance = reader.ReadF32();
    out.camera.yaw = reader.ReadF32();
    out.camera.pitch = reader.ReadF32();
    if (reader.Failed()) {
        return false;
    }

    const std::uint32_t actorCount = reader.ReadU32();
    if (reader.Failed() || actorCount > kMaxLeonLevelActors) {
        return false;
    }
    out.actors.reserve(actorCount);
    for (std::uint32_t i = 0; i < actorCount; ++i) {
        LevelActorRecord actor;
        const std::uint8_t actorClass = reader.ReadU8();
        const std::uint8_t mobility = reader.ReadU8();
        (void)reader.ReadU16();
        const std::uint32_t flags = reader.ReadU32();
        if (reader.Failed()) {
            return false;
        }
        if (actorClass > static_cast<std::uint8_t>(ELevelActorClass::AISpawnPoint)) {
            std::cerr << "LeonLevelFormat: unknown actor class " << static_cast<int>(actorClass)
                      << '\n';
            return false;
        }
        actor.actorClass = static_cast<ELevelActorClass>(actorClass);
        actor.mobility =
            mobility == 1 ? EComponentMobility::Movable : EComponentMobility::Static;
        actor.collisionEnabled = (flags & kLevelActorFlagCollisionEnabled) != 0u;
        actor.simulatePhysics = (flags & kLevelActorFlagSimulatePhysics) != 0u;
        actor.enableGravity = (flags & kLevelActorFlagEnableGravity) != 0u;
        actor.hidden = (flags & kLevelActorFlagHidden) != 0u;
        actor.hasBob = (flags & kLevelActorFlagHasBob) != 0u;
        actor.hasSpinYaw = (flags & kLevelActorFlagHasSpinYaw) != 0u;
        actor.hasFitHeight = (flags & kLevelActorFlagHasFitHeight) != 0u;
        actor.bConsumeOnUse = (flags & kLevelActorFlagConsumeOnUse) != 0u;

        actor.position = reader.ReadVec3();
        actor.rotationDegrees = reader.ReadVec3();
        actor.scale = reader.ReadVec3();

        if ((flags & kLevelActorFlagHasTag) != 0u) {
            actor.tag = stringAt(reader.ReadU32());
        }
        if ((flags & kLevelActorFlagHasMaterial) != 0u) {
            actor.materialPath = stringAt(reader.ReadU32());
        }
        if ((flags & kLevelActorFlagHasMesh) != 0u) {
            actor.meshPath = stringAt(reader.ReadU32());
        }
        if ((flags & kLevelActorFlagHasLightmapId) != 0u) {
            actor.lightmapId = stringAt(reader.ReadU32());
        }
        if ((flags & kLevelActorFlagHasLightmapPath) != 0u) {
            actor.lightmapPath = stringAt(reader.ReadU32());
        }
        actor.lightmapResolution = reader.ReadU32();

        if (actor.actorClass == ELevelActorClass::Sphere) {
            actor.sphereSegments = reader.ReadI32();
            actor.sphereRings = reader.ReadI32();
        }
        if (actor.hasSpinYaw) {
            actor.spinYaw = reader.ReadF32();
        }
        if (actor.hasBob) {
            actor.bobBaseY = reader.ReadF32();
            actor.bobAmplitude = reader.ReadF32();
            actor.bobSpeed = reader.ReadF32();
        }
        if (actor.hasFitHeight) {
            actor.fitHeight = reader.ReadF32();
        }
        if ((flags & kLevelActorFlagHasInteractCost) != 0u) {
            actor.interactCost = reader.ReadI32();
            actor.interactRadius = reader.ReadF32();
        }
        if ((flags & kLevelActorFlagHasPainData) != 0u) {
            actor.damagePerSecond = reader.ReadF32();
            actor.damageInterval = reader.ReadF32();
        }
        if ((flags & kLevelActorFlagHasPayload) != 0u) {
            actor.payload = stringAt(reader.ReadU32());
        }
        if (reader.Failed()) {
            return false;
        }
        out.actors.push_back(std::move(actor));
    }

    const std::uint32_t lightCount = reader.ReadU32();
    if (reader.Failed() || lightCount > kMaxLeonLevelLights) {
        return false;
    }
    out.lights.reserve(lightCount);
    for (std::uint32_t i = 0; i < lightCount; ++i) {
        LevelLightRecord light;
        const std::uint8_t lightClass = reader.ReadU8();
        reader.Skip(3);
        const std::uint32_t flags = reader.ReadU32();
        if (reader.Failed()) {
            return false;
        }
        if (lightClass > static_cast<std::uint8_t>(ELevelLightClass::PointLight)) {
            std::cerr << "LeonLevelFormat: unknown light class " << static_cast<int>(lightClass)
                      << '\n';
            return false;
        }
        light.lightClass = static_cast<ELevelLightClass>(lightClass);
        light.castShadows = (flags & kLevelLightFlagCastShadows) != 0u;
        light.hasOrbit = (flags & kLevelLightFlagHasOrbit) != 0u;

        light.position = reader.ReadVec3();
        light.rotationDegrees = reader.ReadVec3();
        light.lightColor = reader.ReadVec3();
        light.intensity = reader.ReadF32();
        light.range = reader.ReadF32();
        light.sourceAngle = reader.ReadF32();
        if (light.hasOrbit) {
            light.orbitRadius = reader.ReadF32();
            light.orbitHeight = reader.ReadF32();
            light.orbitHeightAmp = reader.ReadF32();
            light.orbitSpeed = reader.ReadF32();
        }
        if (reader.Failed()) {
            return false;
        }
        out.lights.push_back(light);
    }

    return !reader.Failed();
}

bool SaveLeonLevelFile(const std::string& path, const LevelDocument& doc) {
    const std::vector<std::uint8_t> bytes = SerializeLeonLevel(doc);
    if (!FFileHelper::WriteFileAtomic(path, bytes)) {
        std::cerr << "LeonLevelFormat: cannot write: " << path << '\n';
        return false;
    }
    return true;
}

bool LoadLeonLevelFile(const std::string& path, LevelDocument& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "LeonLevelFormat: cannot open " << path << '\n';
        return false;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    if (!DeserializeLeonLevel(bytes, out)) {
        std::cerr << "LeonLevelFormat: failed to parse " << path << '\n';
        return false;
    }
    return true;
}

bool ApplyLevelDocument(Engine& engine, const LevelDocument& doc, const std::string& sourcePath,
                        LevelAnimation* outAnim) {
    Level staged;
    staged.Clear();
    LevelAnimation anim;
    FResourceCache& resources = engine.GetResources();

    try {
        if (!doc.environmentPath.empty()) {
            staged.SetEnvironmentPath(doc.environmentPath);
            staged.SetEnvironment(resources.LoadEnvMap(FPaths::ResolveAssetPath(doc.environmentPath)));
        }
        staged.SetEnvironmentExposure(doc.environmentExposure);
        staged.SetName(doc.name);
        staged.SetGameMode(doc.gameMode);

        int failedMeshes = 0;
        for (const LevelActorRecord& record : doc.actors) {
            FTransform transform;
            transform.Position = record.position;
            transform.RotationDegrees = record.rotationDegrees;
            transform.Scale = record.scale;

            if (record.actorClass == ELevelActorClass::PlayerStart) {
                PlayerStart start{};
                start.transform = transform;
                staged.AddPlayerStart(start);
                continue;
            }
            if (record.actorClass == ELevelActorClass::AISpawnPoint) {
                AISpawnPoint spawn{};
                spawn.transform = transform;
                spawn.tag = record.tag;
                staged.AddAISpawnPoint(std::move(spawn));
                continue;
            }
            if (record.actorClass == ELevelActorClass::TriggerVolume) {
                TriggerVolume volume{};
                volume.transform = transform;
                volume.interactRadius = record.interactRadius;
                volume.interactCost = record.interactCost;
                volume.payload = record.payload;
                volume.tag = record.tag;
                volume.bConsumeOnUse = record.bConsumeOnUse;
                staged.AddTriggerVolume(std::move(volume));
                continue;
            }
            if (record.actorClass == ELevelActorClass::PainCausingVolume) {
                PainCausingVolume volume{};
                volume.transform = transform;
                volume.damagePerSecond = record.damagePerSecond;
                volume.damageInterval = record.damageInterval;
                volume.tag = record.tag;
                staged.AddPainCausingVolume(std::move(volume));
                continue;
            }

            StaticMeshComponent actor;
            EBasicShape shapeType{};
            const bool isBasicShape = BasicShapeForActorClass(record.actorClass, shapeType);
            if (isBasicShape) {
                BasicShape shape;
                shape.type = shapeType;
                shape.transform = transform;
                shape.sphereSegments = record.sphereSegments;
                shape.sphereRings = record.sphereRings;
                actor = shape.MakeStaticMesh(resources);
                actor.sphereSegments = record.sphereSegments;
                actor.sphereRings = record.sphereRings;
            } else {
                actor.mesh = resources.LoadStaticMesh(
                    ResolveLevelAssetPath(sourcePath, record.meshPath));
                actor.transform = transform;
                actor.meshPath = record.meshPath;
            }
            actor.editorClass = EditorClassFromActorClass(record.actorClass);

            if (actor.mesh == nullptr) {
                std::cerr << "LeonLevelFormat: failed mesh for actor in " << sourcePath << '\n';
                ++failedMeshes;
                continue;
            }

            if (record.hasFitHeight && record.fitHeight > 0.0f) {
                ApplyFitHeight(actor, record.fitHeight);
            }

            actor.tag = record.tag;
            actor.simulatePhysics = record.simulatePhysics;
            actor.collisionEnabled = record.collisionEnabled || record.simulatePhysics;
            actor.enableGravity = record.enableGravity;
            actor.hidden = record.hidden;
            actor.mobility = record.mobility;
            actor.lightmapResolution = static_cast<int>(record.lightmapResolution);
            actor.lightmapId = record.lightmapId;
            actor.lightmapPath = record.lightmapPath;
            actor.spinYaw = record.hasSpinYaw ? record.spinYaw : 0.0f;
            actor.materialPath = record.materialPath;

            if (!record.materialPath.empty()) {
                FMaterial base =
                    resources.LoadMaterial(ResolveLevelAssetPath(sourcePath, record.materialPath));
                if (actor.mesh->HasMaterials()) {
                    actor.materials.assign(actor.mesh->Materials().size(), base);
                } else {
                    actor.materialOverride = true;
                    actor.material = std::move(base);
                }
            } else if (!actor.mesh->HasMaterials()) {
                actor.materialOverride = true;
                actor.material = resources.DefaultMaterial();
            }

            // BlockingVolume: invisible collision box, never a shadow caster.
            if (record.actorClass == ELevelActorClass::BlockingVolume) {
                actor.material.castsShadows = false;
                for (FMaterial& material : actor.materials) {
                    material.castsShadows = false;
                }
            }

            const std::size_t actorIndex = staged.StaticMeshes().size();
            staged.AddStaticMesh(std::move(actor));

            if (record.hasSpinYaw) {
                anim.spins.push_back(LevelAnimation::StaticMeshSpin{
                    .meshIndex = actorIndex,
                    .yawDegreesPerSec = record.spinYaw,
                });
            }
            if (record.hasBob) {
                StaticMeshComponent& live = staged.StaticMeshes()[actorIndex];
                live.hasBob = true;
                live.bobBaseY = record.bobBaseY;
                live.bobAmplitude = record.bobAmplitude;
                live.bobSpeed = record.bobSpeed;
                anim.bobs.push_back(LevelAnimation::StaticMeshBob{
                    .meshIndex = actorIndex,
                    .baseY = record.bobBaseY,
                    .amplitude = record.bobAmplitude,
                    .speed = record.bobSpeed,
                });
            }
        }

        if (failedMeshes > 0) {
            std::cerr << "LeonLevelFormat: aborting '" << sourcePath << "' (" << failedMeshes
                      << " mesh failure(s); refusing partial load)\n";
            return false;
        }

        ApplyDocumentLights(doc, staged, anim);
    } catch (const std::exception& ex) {
        std::cerr << "LeonLevelFormat: failed while building " << sourcePath << ": " << ex.what()
                  << '\n';
        return false;
    }

    // Blank / lights-only levels are valid (editor New Level → Blank).
    if (staged.StaticMeshes().empty() && staged.PlayerStarts().empty() &&
        staged.TriggerVolumes().empty() && staged.PainCausingVolumes().empty() &&
        staged.AISpawnPoints().empty() && staged.DirectionalLights().empty() &&
        staged.PointLights().empty() && staged.Environment() == nullptr) {
        std::cerr << "LeonLevelFormat: completely empty level in " << sourcePath << '\n';
        return false;
    }

    engine.GetLevel() = std::move(staged);

    Camera& camera = engine.GetCamera();
    camera.SetTarget(doc.camera.target);
    camera.SetDistance(doc.camera.distance);
    camera.SetYawPitch(doc.camera.yaw, doc.camera.pitch);
    camera.SetEyeLocation(doc.camera.eye);
    camera.SetMode(doc.camera.mode);

    // Runtime + Editor: hydrate GPU textures from persisted `.lm` paths.
    (void)LoadLevelLightmaps(engine.GetLevel(), sourcePath, nullptr);

    if (outAnim != nullptr) {
        *outAnim = std::move(anim);
    }

    const std::string label = doc.name.empty() ? sourcePath : doc.name;
    std::cout << "LevelLoader: loaded '" << label << "' (" << engine.GetLevel().StaticMeshes().size()
              << " actors)\n";
    return true;
}

