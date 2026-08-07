// One-shot: stamp Main.llev as Coop arena (gameMode coop-tp, 2 PlayerStarts).
#include <leon/level/LeonLevelFormat.h>

#include <iostream>
#include <string>

namespace {

using leon::ELevelActorClass;
using leon::LevelActorRecord;
using leon::LevelDocument;

const LevelActorRecord* FindFirstPlayerStart(const LevelDocument& doc) {
    for (const LevelActorRecord& actor : doc.actors) {
        if (actor.actorClass == ELevelActorClass::PlayerStart) {
            return &actor;
        }
    }
    return nullptr;
}

int CountPlayerStarts(const LevelDocument& doc) {
    int n = 0;
    for (const LevelActorRecord& actor : doc.actors) {
        if (actor.actorClass == ELevelActorClass::PlayerStart) {
            ++n;
        }
    }
    return n;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: make-coop-arena <Main.llev> [out.llev]\n";
        return 1;
    }
    const std::string inPath = argv[1];
    const std::string outPath = argc >= 3 ? argv[2] : inPath;

    LevelDocument doc;
    if (!leon::LoadLeonLevelFile(inPath, doc)) {
        std::cerr << "Failed to load " << inPath << '\n';
        return 1;
    }

    doc.name = "Arena";
    doc.gameMode = "coop-tp";

    if (CountPlayerStarts(doc) < 2) {
        if (const LevelActorRecord* first = FindFirstPlayerStart(doc)) {
            LevelActorRecord second = *first;
            second.position.x += 3.0f;
            second.rotationDegrees.y = first->rotationDegrees.y + 180.0f;
            doc.actors.push_back(second);
        } else {
            LevelActorRecord a{};
            a.actorClass = ELevelActorClass::PlayerStart;
            a.position = {0.0f, 0.0f, 0.0f};
            doc.actors.push_back(a);
            LevelActorRecord b = a;
            b.position.x = 3.0f;
            b.rotationDegrees.y = 180.0f;
            doc.actors.push_back(b);
        }
    }

    if (!leon::SaveLeonLevelFile(outPath, doc)) {
        std::cerr << "Failed to save " << outPath << '\n';
        return 1;
    }

    std::cout << "Wrote " << outPath << " name=" << doc.name << " gameMode=" << doc.gameMode
              << " playerStarts=" << CountPlayerStarts(doc) << " actors=" << doc.actors.size()
              << '\n';
    return 0;
}
