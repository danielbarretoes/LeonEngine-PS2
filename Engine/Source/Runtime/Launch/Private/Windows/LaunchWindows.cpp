#include "Modules/ModuleManager.h"
#include "RunLeonGame.h"

#include <cstring>

// Transitional Win64 entry point (Phase 3 turns this into GuardedMain + FEngineLoop).
// LeonGame runs a project pack: LeonGame.exe --pack <Name> [game flags].
int main(int argc, char** argv) {
    const char* packName = LEON_PROJECT_NAME;
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::strcmp(argv[index], "--pack") == 0) {
            packName = argv[index + 1];
        }
    }

    FModuleManager::Get().StartupStaticallyLinkedModules();
    const int code = leon::runtime::RunLeonGame(argc, argv, packName, [](leon::Engine&, leon::GameplayRouter&) {});
    FModuleManager::Get().ShutdownModules();
    return code;
}
