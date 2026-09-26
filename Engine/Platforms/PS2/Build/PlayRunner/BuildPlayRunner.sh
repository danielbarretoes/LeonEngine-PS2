#!/bin/sh
# Builds leonrun (LeonRun.cpp) in a Play! checkout pinned below: a headless EE runner on Play!'s HLE BIOS, for booting a
# PS2 ELF without a console BIOS (the boot smoke test in Docs/TESTING.md). Linux; needs git, cmake, ninja, a C++17
# compiler and the OpenSSL and bzip2 development packages. Prints the runner's path.
#   sh Engine/Platforms/PS2/Build/PlayRunner/BuildPlayRunner.sh
set -eu
PLAY_COMMIT=83700b2c31e593bc94e845b4b31b797be84dda59
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../../.." && pwd)
WORK="$ROOT/Engine/Intermediate/PlayRunner"
SRC="$WORK/Play"
if [ ! -d "$SRC/.git" ]; then
	mkdir -p "$WORK"
	git clone --quiet https://github.com/jpd002/Play-.git "$SRC"
fi
git -C "$SRC" fetch --quiet --depth 1 origin "$PLAY_COMMIT" 2>/dev/null || true
git -C "$SRC" checkout --quiet "$PLAY_COMMIT"
git -C "$SRC" submodule update --quiet --init --recursive --depth 1
cp "$HERE/LeonRun.cpp" "$SRC/tools/AutoTest/LeonRun.cpp"
if ! grep -q "leonrun" "$SRC/tools/AutoTest/CMakeLists.txt"; then
	printf '%s\n' "add_executable(leonrun LeonRun.cpp)" \
		"target_link_libraries(leonrun PlayCore \${AUTOTEST_PROJECT_LIBS})" >> "$SRC/tools/AutoTest/CMakeLists.txt"
fi
cmake -S "$SRC" -B "$WORK/Build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_PLAY=OFF -DBUILD_TESTS=ON >/dev/null
ninja -C "$WORK/Build" leonrun >/dev/null
echo "$WORK/Build/tools/AutoTest/leonrun"
