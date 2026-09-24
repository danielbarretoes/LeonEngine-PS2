#!/bin/sh
# GenerateProjectFiles.sh [-Project=<file.leonproject>] (UE: root GenerateProjectFiles.sh)
set -eu
exec sh "$(dirname "$0")/Engine/Build/BatchFiles/Linux/GenerateProjectFiles.sh" "$@"
