#include "Ps2CubeDemo.h"

#include <leon/core/Window.h>

#include <cstdio>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    leon::Window window;
    if (!window.Create(640, 448, "Leon Ps2Cube")) {
        std::printf("Ps2Cube: Window::Create failed\n");
        return 1;
    }

    const int code = leon::ps2cube::RunPs2CubeDemo(window);
    window.Destroy();
    return code;
}
