#!/bin/sh
# Entry point inside the ps2dev Docker image (Alpine, no bash). Called by LeonBuildTool:
#   docker run ... -v <root>:/leon -w /leon <image> sh /leon/Engine/Platforms/PS2/Build/BatchFiles/DockerEntry.sh <LeonBuildTool args>
set -eu
export PS2DEV="${PS2DEV:-/usr/local/ps2dev}"
export PS2SDK="${PS2SDK:-$PS2DEV/ps2sdk}"
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PATH"
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
	apk add --no-cache cmake ninja make >/dev/null
fi
exec cmake -P /leon/Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake -- "$@"
