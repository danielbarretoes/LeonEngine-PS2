#!/bin/sh
# Engine/Build/BatchFiles/Linux/Build.sh <Target> <Platform> <Configuration> [-Project=<file>] [-Mode=...]
set -eu
ROOT="$(CDPATH= cd -- "$(dirname "$0")/../../../.." && pwd)"
exec cmake -P "$ROOT/Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake" "$@"
