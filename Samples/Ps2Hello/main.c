/* Toolchain smoke: print + sleep. No Leon Engine / GS init. */
#include <kernel.h>
#include <stdio.h>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    printf("Leon Ps2Hello: toolchain OK\n");
    SleepThread();
    return 0;
}
