#include "Ps2ThirdPersonDemo.h"

#include <leon/core/Window.h>

#include <cstdio>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    leon::Window window;
    if (!window.Create(640, 448, "Leon Ps2ThirdPerson")) {
        std::printf("Ps2ThirdPerson: Window::Create failed\n");
        return 1;
    }

    const int code = leon::ps2thirdperson::RunPs2ThirdPersonDemo(window);
    window.Destroy();
    return code;
}
