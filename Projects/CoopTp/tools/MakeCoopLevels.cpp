// Stamp CoopTp front-end + two gameplay arenas from an arena source .llev.
// Menu/Lobby are synthesized (plane + PlayerStart) — do not clone an empty Blank.
#include <leon/level/LeonLevelFormat.h>

#include <algorithm>
#include <iostream>
#include <string>

namespace {

using leon::ELevelActorClass;
using leon::LevelActorRecord;
using leon::LevelDocument;

int CountPlayerStarts(const LevelDocument& doc) {
    int n = 0;
    for (const LevelActorRecord& actor : doc.actors) {
        if (actor.actorClass == ELevelActorClass::PlayerStart) {
            ++n;
        }
    }
    return n;
}

const LevelActorRecord* FindFirstPlayerStart(const LevelDocument& doc) {
    for (const LevelActorRecord& actor : doc.actors) {
        if (actor.actorClass == ELevelActorClass::PlayerStart) {
            return &actor;
        }
    }
    return nullptr;
}

[[nodiscard]] float FindFloorPlaneY(const LevelDocument& doc) {
    float bestArea = -1.0f;
    float floorY = 0.0f;
    for (const LevelActorRecord& actor : doc.actors) {
        if (actor.actorClass != ELevelActorClass::Plane) {
            continue;
        }
        const float area = std::abs(actor.scale.x) * std::abs(actor.scale.z);
        if (area > bestArea) {
            bestArea = area;
            floorY = actor.position.y;
        }
    }
    return floorY;
}

void EnsureTwoPlayerStarts(LevelDocument& doc) {
    const float floorY = FindFloorPlaneY(doc);
    constexpr float kFeetClearance = 0.05f;

    auto snap = [floorY](LevelActorRecord& ps) {
        ps.position.y = floorY + kFeetClearance;
    };

    if (CountPlayerStarts(doc) >= 2) {
        for (LevelActorRecord& actor : doc.actors) {
            if (actor.actorClass == ELevelActorClass::PlayerStart) {
                snap(actor);
            }
        }
        return;
    }
    if (const LevelActorRecord* first = FindFirstPlayerStart(doc)) {
        LevelActorRecord second = *first;
        second.position.x += 3.0f;
        second.rotationDegrees.y = first->rotationDegrees.y + 180.0f;
        snap(second);
        for (LevelActorRecord& actor : doc.actors) {
            if (actor.actorClass == ELevelActorClass::PlayerStart) {
                snap(actor);
            }
        }
        doc.actors.push_back(second);
        return;
    }
    LevelActorRecord a{};
    a.actorClass = ELevelActorClass::PlayerStart;
    a.position = {0.0f, floorY + kFeetClearance, 0.0f};
    a.rotationDegrees.y = 180.0f;
    doc.actors.push_back(a);
    LevelActorRecord b = a;
    b.position.x = 3.0f;
    b.rotationDegrees.y = 0.0f;
    doc.actors.push_back(b);
}

/// Inclined walkable slabs (Cube + pitch). CMC walks via TriangleMesh; tag for tools/nav.
void EnsureArenaRamps(LevelDocument& doc) {
    for (const LevelActorRecord& actor : doc.actors) {
        if (actor.tag == "NavWalkable") {
            return;
        }
    }

    const float floorY = FindFloorPlaneY(doc);

    auto makeRamp = [&](glm::vec3 position, float yawDegrees, float pitchDegrees) {
        LevelActorRecord ramp{};
        ramp.actorClass = ELevelActorClass::Cube;
        ramp.mobility = leon::EComponentMobility::Static;
        ramp.collisionEnabled = true;
        ramp.simulatePhysics = false;
        ramp.enableGravity = false;
        ramp.tag = "NavWalkable";
        ramp.materialPath = "materials/M_PlatformGray.lmat";
        // Long thin slab; roll ~20° so the top rises along local +X (cos20≈0.94 walkable).
        ramp.scale = {5.0f, 0.2f, 2.2f};
        ramp.position = position;
        ramp.position.y = floorY + 0.85f;
        ramp.rotationDegrees = {0.0f, yawDegrees, pitchDegrees};
        doc.actors.push_back(ramp);
    };

    // Two ramps: rise along +X and along local +X after 90° yaw.
    makeRamp({-7.0f, 0.0f, 7.0f}, 0.0f, -20.0f);
    makeRamp({7.0f, 0.0f, -6.0f}, 90.0f, -20.0f);
}

[[nodiscard]] LevelDocument MakeFrontEndLevel() {
    LevelDocument doc;
    doc.environmentPath = "Hdr/AutumnFieldPuresky1k.hdr";

    LevelActorRecord floor{};
    floor.actorClass = ELevelActorClass::Plane;
    floor.position = {0.0f, 0.0f, 0.0f};
    floor.scale = {20.0f, 1.0f, 20.0f};
    floor.materialPath = "materials/M_WorldGrid.lmat";
    doc.actors.push_back(floor);

    LevelActorRecord start{};
    start.actorClass = ELevelActorClass::PlayerStart;
    start.position = {0.0f, 0.05f, 4.0f};
    start.rotationDegrees.y = 180.0f;
    doc.actors.push_back(start);
    return doc;
}

[[nodiscard]] bool WriteLevel(const std::string& path, LevelDocument& doc, const char* name,
                              const char* gameMode) {
    doc.name = name;
    doc.gameMode = gameMode;
    if (doc.environmentPath.empty() ||
        doc.environmentPath.find("autumn_field") != std::string::npos) {
        doc.environmentPath = "Hdr/AutumnFieldPuresky1k.hdr";
    }
    if (!leon::SaveLeonLevelFile(path, doc)) {
        std::cerr << "Failed to save " << path << '\n';
        return false;
    }
    std::cout << "Wrote " << path << " name=" << doc.name << " gameMode=" << doc.gameMode
              << " actors=" << doc.actors.size() << " playerStarts=" << CountPlayerStarts(doc)
              << '\n';
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: make-coop-levels <ArenaSource.llev> <out_dir>\n"
                  << "  (optional legacy) make-coop-levels <Blank.llev> <ArenaSource.llev> <out_dir>\n";
        return 1;
    }

    std::string arenaPath;
    std::string outDir;
    if (argc >= 4) {
        // Legacy: Blank is ignored for Menu/Lobby (synthesized).
        arenaPath = argv[2];
        outDir = argv[3];
    } else {
        arenaPath = argv[1];
        outDir = argv[2];
    }

    LevelDocument menu = MakeFrontEndLevel();
    if (!WriteLevel(outDir + "/MainMenu.llev", menu, "MainMenu", "coop-menu")) {
        return 1;
    }
    LevelDocument lobby = MakeFrontEndLevel();
    if (!WriteLevel(outDir + "/Lobby.llev", lobby, "Lobby", "coop-lobby")) {
        return 1;
    }

    LevelDocument arena;
    if (!leon::LoadLeonLevelFile(arenaPath, arena)) {
        std::cerr << "Failed to load " << arenaPath << '\n';
        return 1;
    }
    EnsureTwoPlayerStarts(arena);
    EnsureArenaRamps(arena);

    LevelDocument courtyard = arena;
    if (!WriteLevel(outDir + "/Courtyard.llev", courtyard, "Courtyard", "coop-tp")) {
        return 1;
    }

    // Rooftops: same layout raised + PlayerStarts shifted so travel feels distinct.
    LevelDocument rooftops = arena;
    for (LevelActorRecord& actor : rooftops.actors) {
        actor.position.y += 4.0f;
        if (actor.actorClass == ELevelActorClass::PlayerStart) {
            actor.position.x += 1.5f;
            actor.position.z -= 1.0f;
        }
    }
    EnsureTwoPlayerStarts(rooftops);
    if (!WriteLevel(outDir + "/Rooftops.llev", rooftops, "Rooftops", "coop-tp")) {
        return 1;
    }
    return 0;
}
