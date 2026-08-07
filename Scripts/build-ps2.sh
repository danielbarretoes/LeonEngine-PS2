#!/usr/bin/env sh
# Cross-build Leon PS2 targets with ps2dev (WSL2 / Linux / Docker Alpine).
# Usage:
#   Scripts/build-ps2.sh              # Ps2Cube (3D)
#   Scripts/build-ps2.sh hello        # Samples/Ps2Hello
#   Scripts/build-ps2.sh lab          # Projects/Ps2Lab (2D)
#   Scripts/build-ps2.sh cube         # Projects/Ps2Cube (3D)
set -eu
ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

TARGET="${1:-cube}"

if [ -z "${PS2DEV:-}" ] || [ -z "${PS2SDK:-}" ]; then
  echo "ERROR: PS2DEV and PS2SDK must be set (see Docs/SETUP.md § PS2)."
  exit 1
fi
if [ ! -x "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-g++" ]; then
  echo "ERROR: ee g++ not found under PS2DEV=${PS2DEV}"
  exit 1
fi

TOOLCHAIN="${ROOT}/Build/toolchains/ps2-ee.cmake"
export PATH="${PS2DEV}/bin:${PS2DEV}/ee/bin:${PS2DEV}/iop/bin:${PS2DEV}/dvp/bin:${PATH}"

case "$TARGET" in
  hello)
    SRC="Samples/Ps2Hello"
    BIN="Samples/Ps2Hello/build-ps2"
    EXE="leon-Ps2Hello.elf"
    ;;
  lab|smoke)
    SRC="Projects/Ps2Lab"
    BIN="Projects/Ps2Lab/build-ps2"
    EXE="leon-Ps2Lab.elf"
    ;;
  cube|*)
    SRC="Projects/Ps2Cube"
    BIN="Projects/Ps2Cube/build-ps2"
    EXE="leon-Ps2Cube.elf"
    ;;
esac

GEN_ARGS=""
if command -v ninja >/dev/null 2>&1; then
  GEN_ARGS="-G Ninja"
fi

rm -rf "$BIN"
# shellcheck disable=SC2086
cmake -S "$SRC" -B "$BIN" $GEN_ARGS \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLEON_PLATFORM=PS2 \
  -DLEON_RHI=PS2 \
  -DLEON_BUILD_CLIENT=OFF \
  -DLEON_BUILD_TESTS=OFF \
  -DLEON_WITH_JOLT=OFF

cmake --build "$BIN" --parallel

echo ""
echo "Built: ${BIN}/${EXE}"
echo "Run in PCSX2 (File → Run ELF) — see Docs/SETUP.md § PS2."
