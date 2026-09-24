#!/usr/bin/env sh
# Cross-build Leon PS2 targets with ps2dev (WSL2 / Linux / Docker Alpine).
# Usage:
#   Scripts/build-ps2.sh              # Projects/Ps2ThirdPerson (gameplay)
#   Scripts/build-ps2.sh tp           # Projects/Ps2ThirdPerson (gameplay)
set -eu
ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

TARGET="${1:-tp}"

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
  tp|thirdperson)
    SRC="Projects/Ps2ThirdPerson"
    BIN="Projects/Ps2ThirdPerson/build-ps2"
    EXE="leon-Ps2ThirdPerson.elf"
    ;;
  *)
    echo "ERROR: unknown target '$TARGET' (only: tp)"
    exit 1
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
