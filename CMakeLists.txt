# Leon monorepo root — not a build entry point.
#   Editor:   cmake -S Editor -B Editor/build
#             Scripts\build.bat
#   PS2 Lab:  Scripts/build-ps2.sh  (or Scripts/build-ps2-docker.ps1)
#   Tools:    cmake -S Tools -B Tools/build
cmake_minimum_required(VERSION 3.20)
message(FATAL_ERROR
    "Do not configure the repo root.\n"
    "  Editor (Host):  cmake -S Editor -B Editor/build  (or Scripts\\build.bat)\n"
    "  PS2 Lab:        Scripts/build-ps2.sh  (or Scripts/build-ps2-docker.sh)\n"
    "  Tools (Host):   cmake -S Tools -B Tools/build\n")
