#!/bin/sh
# Entry point inside the ps2dev Docker image (Alpine, no bash). Called by LeonBuildTool:
#   docker run ... -v <root>:/leon -w /leon <image> sh /leon/Engine/Platforms/PS2/Build/BatchFiles/DockerEntry.sh <LeonBuildTool args>
set -eu
export PS2DEV="${PS2DEV:-/usr/local/ps2dev}"
export PS2SDK="${PS2SDK:-$PS2DEV/ps2sdk}"
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PATH"
# CMake and Ninja drive the build; the host g++ (and musl-dev for its C library headers) builds LeonHeaderTool in
# Engine/Intermediate/Build/HostTools/LinuxMusl before the PS2 configure.
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1 || ! command -v g++ >/dev/null 2>&1; then
	apk add --no-cache cmake ninja make g++ musl-dev >/dev/null
fi
exec cmake -P /leon/Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake -- "$@"
