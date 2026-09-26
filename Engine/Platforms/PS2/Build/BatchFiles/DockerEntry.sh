#!/bin/sh
# Entry point inside the ps2dev Docker image (Alpine, no bash). Called by LeonBuildTool:
#   docker run ... -v <root>:/leon -w /leon <image> sh /leon/Engine/Platforms/PS2/Build/BatchFiles/DockerEntry.sh <LeonBuildTool args>
set -eu
export PS2DEV="${PS2DEV:-/usr/local/ps2dev}"
export PS2SDK="${PS2SDK:-$PS2DEV/ps2sdk}"
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PATH"
# CMake and Ninja drive the build; the host g++ (and musl-dev for its C library headers) builds LeonHeaderTool in
# Engine/Intermediate/Build/HostTools/LinuxMusl before the PS2 configure (the container runs as root to install them).
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1 || ! command -v g++ >/dev/null 2>&1; then
	apk add --no-cache cmake ninja make g++ musl-dev >/dev/null
fi
status=0
cmake -P /leon/Engine/Source/Programs/LeonBuildTool/LeonBuildTool.cmake -- "$@" || status=$?
# A Unix host's user gets its build outputs back (LeonBuildTool passes its uid and gid; the container runs as root).
if [ -n "${LEON_HOST_UID:-}" ] && [ -n "${LEON_HOST_GID:-}" ]; then
	outputs="/leon/Engine/Intermediate /leon/Engine/Binaries"
	for arg in "$@"; do
		case "$arg" in
			-Project=*)
				project_dir=$(dirname "${arg#-Project=}")
				outputs="$outputs $project_dir/Intermediate $project_dir/Binaries"
				;;
		esac
	done
	for dir in $outputs; do
		if [ -e "$dir" ]; then
			chown -R "$LEON_HOST_UID:$LEON_HOST_GID" "$dir"
		fi
	done
fi
exit "$status"
