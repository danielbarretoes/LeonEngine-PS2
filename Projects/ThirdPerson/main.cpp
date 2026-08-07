#include <leon/packs/ThirdPerson/RegisterModes.h>
#include <leon/runtime/RunLeonGame.h>

int main(int argc, char** argv) {
    return leon::runtime::RunLeonGame(argc, argv, "ThirdPerson",
                                      &leon::packs::third_person::RegisterModes);
}
