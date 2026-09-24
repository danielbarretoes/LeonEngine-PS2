#!/bin/sh
# Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh [-Project=<file.leonproject>]
# Host project files (default CMake generator) in <Engine|Project>/Intermediate/ProjectFiles.
set -eu
ROOT="$(CDPATH= cd -- "$(dirname "$0")/../../../.." && pwd)"
exec cmake -P "$ROOT/Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake" -- -Mode=GenerateProjectFiles "$@"
