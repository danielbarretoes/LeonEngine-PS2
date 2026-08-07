#include <leon/packs/Blank/RegisterModes.h>
#include <leon/runtime/RunLeonGame.h>

#ifndef LEON_PACK_NAME
#define LEON_PACK_NAME "Blank"
#endif

int main(int argc, char** argv) {
    return leon::runtime::RunLeonGame(argc, argv, LEON_PACK_NAME, &leon::packs::blank::RegisterModes);
}
