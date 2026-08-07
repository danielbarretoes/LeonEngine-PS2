// Stamp Furytoon front-end + Kitchen arena (procedural Cubes/Planes — no cooked meshes).
#include <iostream>
#include <leon/level/LeonLevelFormat.h>
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

[[nodiscard]] LevelActorRecord MakeCube(const glm::vec3& pos, const glm::vec3& scale,
                                        const char* tag, const char* material,
                                        bool collision = true) {
    LevelActorRecord cube{};
    cube.actorClass = ELevelActorClass::Cube;
    cube.mobility = leon::EComponentMobility::Static;
    cube.collisionEnabled = collision;
    cube.simulatePhysics = false;
    cube.enableGravity = false;
    cube.tag = tag ? tag : "";
    cube.materialPath = material ? material : "materials/M_PlatformGray.lmat";
    cube.position = pos;
    cube.scale = scale;
    return cube;
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

[[nodiscard]] LevelDocument MakeKitchenArena() {
    // Camera yaw ~25°: screen-horizontal ≈ world Z. Elongate Z, keep X tighter.
    constexpr float kHalfX = 14.0f; // ~28m deep (into camera)
    constexpr float kHalfZ = 26.0f; // ~52m wide (across camera)
    constexpr float kFloorX = kHalfX * 2.0f;
    constexpr float kFloorZ = kHalfZ * 2.0f;
    constexpr float kWallH = 5.5f;
    constexpr float kWallY = kWallH * 0.5f;

    LevelDocument doc;
    doc.environmentPath = "Hdr/AutumnFieldPuresky1k.hdr";

    LevelActorRecord floor{};
    floor.actorClass = ELevelActorClass::Plane;
    floor.position = {0.0f, 0.0f, 0.0f};
    floor.scale = {kFloorX, 1.0f, kFloorZ};
    floor.materialPath = "materials/M_MirrorFloor.lmat";
    floor.collisionEnabled = true;
    doc.actors.push_back(floor);

    // Perimeter walls (taller to frame balconies).
    doc.actors.push_back(MakeCube({0.0f, kWallY, -kHalfZ}, {kFloorX, kWallH, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({0.0f, kWallY, kHalfZ}, {kFloorX, kWallH, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({-kHalfX, kWallY, 0.0f}, {0.5f, kWallH, kFloorZ}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({kHalfX, kWallY, 0.0f}, {0.5f, kWallH, kFloorZ}, "Wall",
                                  "materials/M_PlatformDark.lmat"));

    // Long-wall balconies (camera-width axis) — staggered mid / high decks.
    doc.actors.push_back(MakeCube({-6.0f, 1.35f, -kHalfZ + 2.2f}, {10.0f, 0.35f, 3.2f}, "Balcony",
                                  "materials/M_PlatformGray.lmat"));
    doc.actors.push_back(MakeCube({6.0f, 2.55f, -kHalfZ + 2.0f}, {8.0f, 0.35f, 2.8f}, "Balcony",
                                  "materials/M_CrateCyan.lmat"));
    doc.actors.push_back(MakeCube({6.0f, 1.35f, kHalfZ - 2.2f}, {10.0f, 0.35f, 3.2f}, "Balcony",
                                  "materials/M_PlatformGray.lmat"));
    doc.actors.push_back(MakeCube({-6.0f, 2.55f, kHalfZ - 2.0f}, {8.0f, 0.35f, 2.8f}, "Balcony",
                                  "materials/M_CrateOrange.lmat"));

    // Short-end lofts (into-camera axis).
    doc.actors.push_back(MakeCube({-kHalfX + 2.4f, 1.9f, -10.0f}, {3.6f, 0.35f, 8.0f}, "Loft",
                                  "materials/M_CrateYellow.lmat"));
    doc.actors.push_back(MakeCube({-kHalfX + 2.4f, 3.4f, 8.0f}, {3.6f, 0.35f, 7.0f}, "Loft",
                                  "materials/M_BallPurple.lmat"));
    doc.actors.push_back(MakeCube({kHalfX - 2.4f, 1.9f, 10.0f}, {3.6f, 0.35f, 8.0f}, "Loft",
                                  "materials/M_BallGreen.lmat"));
    doc.actors.push_back(MakeCube({kHalfX - 2.4f, 3.4f, -8.0f}, {3.6f, 0.35f, 7.0f}, "Loft",
                                  "materials/M_PlatformGray.lmat"));

    // Center island: counter + raised roof / shelf stack for vertical fights.
    doc.actors.push_back(MakeCube({0.0f, 0.55f, 0.0f}, {5.0f, 1.1f, 2.4f}, "Counter",
                                  "materials/M_PlatformGray.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 2.2f, 0.0f}, {4.2f, 0.3f, 2.0f}, "Roof",
                                  "materials/M_CrateCyan.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 3.6f, 0.0f}, {3.0f, 0.28f, 1.6f}, "Roof",
                                  "materials/M_CrateOrange.lmat"));

    // Side counters / cover on the open floor.
    doc.actors.push_back(MakeCube({-5.0f, 0.55f, -8.0f}, {3.5f, 1.1f, 1.5f}, "Counter",
                                  "materials/M_CrateCyan.lmat"));
    doc.actors.push_back(MakeCube({5.0f, 0.55f, 8.0f}, {3.5f, 1.1f, 1.5f}, "Counter",
                                  "materials/M_CrateOrange.lmat"));

    // Climb crates — stepping stones toward balconies / lofts (double-jump friendly).
    doc.actors.push_back(MakeCube({-4.0f, 0.45f, -kHalfZ + 5.5f}, {1.6f, 0.9f, 1.6f}, "Crate",
                                  "materials/M_CrateYellow.lmat"));
    doc.actors.push_back(MakeCube({-2.2f, 0.95f, -kHalfZ + 4.0f}, {1.4f, 0.7f, 1.4f}, "Crate",
                                  "materials/M_CrateOrange.lmat"));
    doc.actors.push_back(MakeCube({4.0f, 0.45f, kHalfZ - 5.5f}, {1.6f, 0.9f, 1.6f}, "Crate",
                                  "materials/M_CrateCyan.lmat"));
    doc.actors.push_back(MakeCube({2.2f, 0.95f, kHalfZ - 4.0f}, {1.4f, 0.7f, 1.4f}, "Crate",
                                  "materials/M_PlatformGray.lmat"));
    doc.actors.push_back(MakeCube({-kHalfX + 5.0f, 0.5f, -6.0f}, {1.5f, 1.0f, 1.5f}, "Crate",
                                  "materials/M_BallPurple.lmat"));
    doc.actors.push_back(MakeCube({-kHalfX + 4.0f, 1.15f, -4.0f}, {1.3f, 0.65f, 1.3f}, "Crate",
                                  "materials/M_CrateYellow.lmat"));
    doc.actors.push_back(MakeCube({kHalfX - 5.0f, 0.5f, 6.0f}, {1.5f, 1.0f, 1.5f}, "Crate",
                                  "materials/M_BallGreen.lmat"));
    doc.actors.push_back(MakeCube({kHalfX - 4.0f, 1.15f, 4.0f}, {1.3f, 0.65f, 1.3f}, "Crate",
                                  "materials/M_CrateCyan.lmat"));

    // Mid-air floating shelves for cross-arena hops.
    doc.actors.push_back(MakeCube({-3.5f, 2.9f, -12.0f}, {2.4f, 0.28f, 2.4f}, "Shelf",
                                  "materials/M_PlatformGray.lmat"));
    doc.actors.push_back(MakeCube({3.5f, 2.9f, 12.0f}, {2.4f, 0.28f, 2.4f}, "Shelf",
                                  "materials/M_PlatformGray.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 4.2f, -16.0f}, {2.0f, 0.28f, 2.0f}, "Shelf",
                                  "materials/M_CrateYellow.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 4.2f, 16.0f}, {2.0f, 0.28f, 2.0f}, "Shelf",
                                  "materials/M_CrateYellow.lmat"));

    // Four PlayerStarts along the long axis, facing center.
    const glm::vec3 starts[4] = {
        {-8.0f, 0.05f, -20.0f},
        {8.0f, 0.05f, -20.0f},
        {-8.0f, 0.05f, 20.0f},
        {8.0f, 0.05f, 20.0f},
    };
    const float yaws[4] = {20.0f, -20.0f, 160.0f, -160.0f};
    for (int i = 0; i < 4; ++i) {
        LevelActorRecord ps{};
        ps.actorClass = ELevelActorClass::PlayerStart;
        ps.position = starts[i];
        ps.rotationDegrees.y = yaws[i];
        doc.actors.push_back(ps);
    }

    return doc;
}

[[nodiscard]] bool WriteLevel(const std::string& path, LevelDocument& doc, const char* name,
                              const char* gameMode) {
    doc.name = name;
    doc.gameMode = gameMode;
    if (doc.environmentPath.empty()) {
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
    if (argc < 2) {
        std::cerr << "Usage: make-furytoon-levels <out_dir>\n";
        return 1;
    }
    const std::string outDir = argv[1];

    LevelDocument menu = MakeFrontEndLevel();
    if (!WriteLevel(outDir + "/MainMenu.llev", menu, "MainMenu", "Furytoon-menu")) {
        return 1;
    }
    LevelDocument lobby = MakeFrontEndLevel();
    if (!WriteLevel(outDir + "/Lobby.llev", lobby, "Lobby", "Furytoon-lobby")) {
        return 1;
    }
    LevelDocument kitchen = MakeKitchenArena();
    if (!WriteLevel(outDir + "/Kitchen.llev", kitchen, "Kitchen", "furytoon-match")) {
        return 1;
    }
    return 0;
}
