#!/bin/sh
# Setup.sh — download the pinned third-party dependencies (UE: Setup.sh / GitDependencies).
set -eu
ROOT="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
exec cmake -P "$ROOT/Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake" -- -Mode=Setup
