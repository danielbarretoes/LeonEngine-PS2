# Leon monorepo root — not a build entry point.
#   Editor:   cmake -S Editor -B Editor/build
#             Scripts\build.bat
#   Smoke:    cmake -S Projects/Smoke -B Projects/Smoke/build
#   Tools:    cmake -S Tools -B Tools/build
cmake_minimum_required(VERSION 3.20)
message(FATAL_ERROR
    "Do not configure the repo root.\n"
    "  Editor:  cmake -S Editor -B Editor/build\n"
    "           or run Scripts\\build.bat\n"
    "  Smoke:   cmake -S Projects/Smoke -B Projects/Smoke/build\n"
    "  Tools:   cmake -S Tools -B Tools/build\n")
