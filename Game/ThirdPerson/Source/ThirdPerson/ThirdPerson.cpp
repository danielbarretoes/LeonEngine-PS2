#include "ThirdPersonGameMode.h"

#include "Modules/ModuleManager.h"
#include "Window.h"

#include <cstdio>

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ThirdPerson, "ThirdPerson")

// Transitional entry point: moves to Launch (LaunchPS2.cpp + FEngineLoop) in Phase 3.
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    FModuleManager::Get().StartupStaticallyLinkedModules();

    leon::Window window;
    if (!window.Create(640, 448, "Leon Ps2ThirdPerson")) {
        std::printf("Ps2ThirdPerson: Window::Create failed\n");
        return 1;
    }

    const int code = leon::ps2thirdperson::RunPs2ThirdPersonDemo(window);
    window.Destroy();
    FModuleManager::Get().ShutdownModules();
    return code;
}
