#include "Ps2LabDemo.h"

#include <leon/core/Window.h>

#include <cstdio>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    leon::Window window;
    if (!window.Create(640, 448, "Leon Ps2Lab")) {
        std::printf("Ps2Lab: Window::Create failed\n");
        return 1;
    }

    const int code = leon::ps2lab::RunPs2LabDemo(window);
    window.Destroy();
    return code;
}
