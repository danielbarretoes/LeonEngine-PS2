#include <leon/packs/ThirdPerson/RegisterModes.h>
#include <leon/runtime/RunLeonGame.h>

// Pack folder name comes from CMake (LEON_PACK_NAME). Defaults keep the template runnable
// from Templates/ThirdPerson before New Project copies it.
#ifndef LEON_PACK_NAME
#define LEON_PACK_NAME "ThirdPerson"
#endif

int main(int argc, char** argv) {
    return leon::runtime::RunLeonGame(argc, argv, LEON_PACK_NAME,
                                      &leon::packs::third_person::RegisterModes);
}
