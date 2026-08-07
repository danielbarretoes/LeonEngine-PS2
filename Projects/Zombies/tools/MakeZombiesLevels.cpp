// Stamp Zombies front-end + Town arena (COD Town lite — procedural Cubes/Planes).
// Town also emits typed actors: TriggerVolume, PainCausingVolume, AISpawnPoint.
#include <leon/level/LeonLevelFormat.h>

#include <cmath>
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

[[nodiscard]] LevelActorRecord MakeTrigger(const glm::vec3& pos, float radius, int cost,
                                           const char* payload, bool consumeOnUse) {
    LevelActorRecord trigger{};
    trigger.actorClass = ELevelActorClass::TriggerVolume;
    trigger.position = pos;
    trigger.interactRadius = radius;
    trigger.interactCost = cost;
    trigger.payload = payload ? payload : "";
    trigger.bConsumeOnUse = consumeOnUse;
    return trigger;
}

[[nodiscard]] LevelActorRecord MakePainVolume(const glm::vec3& pos, const glm::vec3& scale,
                                              float damagePerSecond, float damageInterval) {
    LevelActorRecord pain{};
    pain.actorClass = ELevelActorClass::PainCausingVolume;
    pain.position = pos;
    pain.scale = scale;
    pain.damagePerSecond = damagePerSecond;
    pain.damageInterval = damageInterval;
    pain.tag = "Lava";
    return pain;
}

[[nodiscard]] LevelActorRecord MakeAISpawn(const glm::vec3& pos) {
    LevelActorRecord spawn{};
    spawn.actorClass = ELevelActorClass::AISpawnPoint;
    spawn.position = pos;
    spawn.tag = "ZombieSpawn";
    return spawn;
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

/// Compact Town circuit: plaza → bank / bar / house / street → lava → PaP.
[[nodiscard]] LevelDocument MakeTownArena() {
    constexpr float kHalf = 28.0f; // ~56x56 — small, no dead time
    constexpr float kInteractR = 2.4f;
    // Match legacy flat lava tick: amount = dps * interval = 12 every 0.35s.
    constexpr float kLavaTickInterval = 0.35f;
    constexpr float kLavaDamagePerTick = 12.0f;
    constexpr float kLavaDps = kLavaDamagePerTick / kLavaTickInterval;

    LevelDocument doc;
    doc.environmentPath = "Hdr/AutumnFieldPuresky1k.hdr";

    LevelActorRecord floor{};
    floor.actorClass = ELevelActorClass::Plane;
    floor.position = {0.0f, 0.0f, 0.0f};
    floor.scale = {kHalf * 2.0f, 1.0f, kHalf * 2.0f};
    floor.materialPath = "materials/M_WorldGrid.lmat";
    floor.collisionEnabled = true;
    doc.actors.push_back(floor);

    // Outer ring walls (taller for rooms).
    doc.actors.push_back(MakeCube({0.0f, 2.0f, -kHalf}, {kHalf * 2.0f, 4.0f, 0.6f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 2.0f, kHalf}, {kHalf * 2.0f, 4.0f, 0.6f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({-kHalf, 2.0f, 0.0f}, {0.6f, 4.0f, kHalf * 2.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({kHalf, 2.0f, 0.0f}, {0.6f, 4.0f, kHalf * 2.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));

    // --- Plaza (start, always open) ---
    doc.actors.push_back(MakeCube({-3.0f, 0.6f, 2.0f}, {1.6f, 1.2f, 1.6f}, "Crate",
                                  "materials/M_CrateOrange.lmat"));
    doc.actors.push_back(MakeCube({4.0f, 0.6f, -2.0f}, {1.8f, 1.2f, 1.4f}, "Crate",
                                  "materials/M_CrateCyan.lmat"));

    // Starting wall M14 — visual mesh + TriggerVolume (payload WallBuy:M14).
    doc.actors.push_back(MakeCube({0.0f, 1.2f, -8.5f}, {1.2f, 0.8f, 0.35f}, "",
                                  "materials/M_CrateYellow.lmat"));
    doc.actors.push_back(MakeTrigger({0.0f, 1.2f, -8.5f}, kInteractR, 500, "WallBuy:M14", false));

    // --- Bank (east of plaza) — door 750 ---
    doc.actors.push_back(MakeCube({12.0f, 2.0f, -6.0f}, {0.5f, 4.0f, 10.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({12.0f, 2.0f, 8.0f}, {0.5f, 4.0f, 8.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({18.0f, 2.0f, 1.0f}, {0.5f, 4.0f, 18.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({15.0f, 2.0f, -11.0f}, {6.0f, 4.0f, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({15.0f, 2.0f, 12.0f}, {6.0f, 4.0f, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({10.2f, 1.5f, 1.0f}, {0.45f, 3.0f, 2.4f}, "",
                                  "materials/M_BallPurple.lmat"));
    doc.actors.push_back(MakeTrigger({10.2f, 1.5f, 1.0f}, kInteractR, 750, "Door", true));
    doc.actors.push_back(MakeCube({15.0f, 1.2f, 4.0f}, {1.0f, 0.9f, 0.35f}, "",
                                  "materials/M_BallGreen.lmat"));
    doc.actors.push_back(MakeTrigger({15.0f, 1.2f, 4.0f}, kInteractR, 2500, "Perk:Jugg", false));
    doc.actors.push_back(MakeCube({15.0f, 1.2f, -4.0f}, {1.0f, 0.9f, 0.35f}, "",
                                  "materials/M_CrateCyan.lmat"));
    doc.actors.push_back(MakeTrigger({15.0f, 1.2f, -4.0f}, kInteractR, 1000, "WallBuy:MP5", false));

    // --- Bar (west of plaza) — door 750 ---
    doc.actors.push_back(MakeCube({-12.0f, 2.0f, -6.0f}, {0.5f, 4.0f, 10.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({-12.0f, 2.0f, 8.0f}, {0.5f, 4.0f, 8.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({-18.0f, 2.0f, 1.0f}, {0.5f, 4.0f, 18.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({-15.0f, 2.0f, -11.0f}, {6.0f, 4.0f, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({-15.0f, 2.0f, 12.0f}, {6.0f, 4.0f, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({-10.2f, 1.5f, 1.0f}, {0.45f, 3.0f, 2.4f}, "",
                                  "materials/M_BallPurple.lmat"));
    doc.actors.push_back(MakeTrigger({-10.2f, 1.5f, 1.0f}, kInteractR, 750, "Door", true));
    doc.actors.push_back(MakeCube({-15.0f, 1.2f, 4.0f}, {1.0f, 0.9f, 0.35f}, "",
                                  "materials/M_CrateOrange.lmat"));
    doc.actors.push_back(MakeTrigger({-15.0f, 1.2f, 4.0f}, kInteractR, 3000, "Perk:Speed", false));
    doc.actors.push_back(MakeCube({-15.0f, 1.2f, -4.0f}, {1.0f, 0.9f, 0.35f}, "",
                                  "materials/M_PlatformGray.lmat"));
    doc.actors.push_back(MakeTrigger({-15.0f, 1.2f, -4.0f}, kInteractR, 500, "Ammo", false));

    // --- House (south) — door 1000 ---
    doc.actors.push_back(MakeCube({-8.0f, 2.0f, -16.0f}, {10.0f, 4.0f, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({8.0f, 2.0f, -16.0f}, {10.0f, 4.0f, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({-13.0f, 2.0f, -20.0f}, {0.5f, 4.0f, 8.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({13.0f, 2.0f, -20.0f}, {0.5f, 4.0f, 8.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 2.0f, -24.0f}, {26.0f, 4.0f, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 1.5f, -14.2f}, {2.4f, 3.0f, 0.45f}, "",
                                  "materials/M_BallPurple.lmat"));
    doc.actors.push_back(MakeTrigger({0.0f, 1.5f, -14.2f}, kInteractR, 1000, "Door", true));
    doc.actors.push_back(MakeCube({-6.0f, 1.2f, -20.0f}, {1.0f, 0.9f, 0.35f}, "",
                                  "materials/M_CrateYellow.lmat"));
    doc.actors.push_back(
        MakeTrigger({-6.0f, 1.2f, -20.0f}, kInteractR, 2000, "Perk:DoubleTap", false));
    doc.actors.push_back(MakeCube({6.0f, 1.2f, -20.0f}, {1.0f, 0.9f, 0.35f}, "",
                                  "materials/M_CrateOrange.lmat"));
    doc.actors.push_back(
        MakeTrigger({6.0f, 1.2f, -20.0f}, kInteractR, 500, "WallBuy:Olympia", false));
    doc.actors.push_back(MakeCube({0.0f, 1.2f, -22.0f}, {1.0f, 0.9f, 0.35f}, "",
                                  "materials/M_BallGreen.lmat"));
    doc.actors.push_back(
        MakeTrigger({0.0f, 1.2f, -22.0f}, kInteractR, 1500, "Perk:QuickRevive", false));

    // --- Main street north toward PaP — door 1250 ---
    doc.actors.push_back(MakeCube({-6.0f, 2.0f, 14.0f}, {0.5f, 4.0f, 10.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({6.0f, 2.0f, 14.0f}, {0.5f, 4.0f, 10.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 1.5f, 9.2f}, {2.4f, 3.0f, 0.45f}, "",
                                  "materials/M_BallPurple.lmat"));
    doc.actors.push_back(MakeTrigger({0.0f, 1.5f, 9.2f}, kInteractR, 1250, "Door", true));
    doc.actors.push_back(MakeCube({3.5f, 1.2f, 14.0f}, {1.0f, 0.9f, 0.35f}, "",
                                  "materials/M_CrateCyan.lmat"));
    doc.actors.push_back(MakeTrigger({3.5f, 1.2f, 14.0f}, kInteractR, 1200, "WallBuy:M16", false));
    doc.actors.push_back(MakeCube({-3.5f, 1.2f, 14.0f}, {1.0f, 0.9f, 0.35f}, "",
                                  "materials/M_PlatformGray.lmat"));
    doc.actors.push_back(MakeTrigger({-3.5f, 1.2f, 14.0f}, kInteractR, 500, "Ammo", false));

    // Lava visual (no tag) + PainCausingVolume (expanded Y so feet register).
    doc.actors.push_back(MakeCube({0.0f, 0.08f, 20.0f}, {4.5f, 0.12f, 6.0f}, "",
                                  "materials/M_CrateOrange.lmat", false));
    doc.actors.push_back(
        MakePainVolume({0.0f, 0.35f, 20.0f}, {4.5f, 1.0f, 6.0f}, kLavaDps, kLavaTickInterval));
    doc.actors.push_back(MakeCube({-5.0f, 2.0f, 22.0f}, {0.5f, 4.0f, 8.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({5.0f, 2.0f, 22.0f}, {0.5f, 4.0f, 8.0f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 2.0f, 26.0f}, {10.0f, 4.0f, 0.5f}, "Wall",
                                  "materials/M_PlatformDark.lmat"));
    doc.actors.push_back(MakeCube({0.0f, 1.15f, 23.5f}, {1.4f, 1.1f, 1.4f}, "",
                                  "materials/M_BallPurple.lmat"));
    doc.actors.push_back(MakeTrigger({0.0f, 1.15f, 23.5f}, kInteractR, 5000, "PaP", false));

    // AI spawn markers around the outer ring (typed actors — no debug meshes).
    constexpr int kSpawnCount = 14;
    for (int i = 0; i < kSpawnCount; ++i) {
        const float ang = (6.2831853f * static_cast<float>(i)) / static_cast<float>(kSpawnCount);
        const float r = 24.0f;
        const glm::vec3 pos{std::cos(ang) * r, 1.0f, std::sin(ang) * r};
        doc.actors.push_back(MakeAISpawn(pos));
    }

    // PlayerStarts in plaza.
    LevelActorRecord ps0{};
    ps0.actorClass = ELevelActorClass::PlayerStart;
    ps0.position = {0.0f, 0.05f, 0.0f};
    ps0.rotationDegrees.y = 0.0f;
    doc.actors.push_back(ps0);
    LevelActorRecord ps1 = ps0;
    ps1.position = {2.5f, 0.05f, -1.0f};
    doc.actors.push_back(ps1);
    LevelActorRecord ps2 = ps0;
    ps2.position = {-2.5f, 0.05f, -1.0f};
    doc.actors.push_back(ps2);
    LevelActorRecord ps3 = ps0;
    ps3.position = {0.0f, 0.05f, 2.5f};
    doc.actors.push_back(ps3);

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
        std::cerr << "Usage: make-zombies-levels <out_dir>\n";
        return 1;
    }
    const std::string outDir = argv[1];

    LevelDocument menu = MakeFrontEndLevel();
    if (!WriteLevel(outDir + "/MainMenu.llev", menu, "MainMenu", "zombies-menu")) {
        return 1;
    }
    LevelDocument lobby = MakeFrontEndLevel();
    if (!WriteLevel(outDir + "/Lobby.llev", lobby, "Lobby", "zombies-lobby")) {
        return 1;
    }
    LevelDocument town = MakeTownArena();
    if (!WriteLevel(outDir + "/Town.llev", town, "Town", "zombies-match")) {
        return 1;
    }
    // Keep Nacht as alias of Town for older travel keys / bookmarks.
    LevelDocument nacht = MakeTownArena();
    if (!WriteLevel(outDir + "/Nacht.llev", nacht, "Nacht", "zombies-match")) {
        return 1;
    }
    return 0;
}
