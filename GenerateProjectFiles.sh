#!/bin/sh
# GenerateProjectFiles.sh [-Project=<file.lproj>] (UE: root GenerateProjectFiles.sh)
set -eu
exec sh "$(dirname "$0")/Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh" "$@"
