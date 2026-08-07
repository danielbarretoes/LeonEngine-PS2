#!/usr/bin/env bash
# Scripts/build-project.sh — cmake-build a Leon game pack (Linux / macOS host).
# Intermediate:  <project>/build-linux/
# Portable ship: <project>/Shipping/
# Usage:
#   Scripts/build-project.sh <projectDir> [cmakeTarget] [--with-server]
#   Scripts/build-project.sh Projects/MyGame
#   Scripts/build-project.sh Projects/MyGame leon-MyGame --with-server
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ $# -lt 1 ]]; then
  echo "Usage: Scripts/build-project.sh <projectDir> [cmakeTarget] [--with-server]"
  echo "LEON_BUILD_EXIT=1"
  exit 1
fi

PROJ="$(cd "$1" && pwd)"
if [[ ! -f "$PROJ/CMakeLists.txt" ]]; then
  echo "ERROR: No CMakeLists.txt in \"$PROJ\""
  echo "LEON_BUILD_EXIT=1"
  exit 1
fi

PROJ_NAME="$(basename "$PROJ")"
TARGET=""
WITH_SERVER=0
shift
for arg in "$@"; do
  if [[ "$arg" == "--with-server" ]]; then
    WITH_SERVER=1
  elif [[ -z "$TARGET" ]]; then
    TARGET="$arg"
  fi
done
if [[ -z "$TARGET" ]]; then
  TARGET="leon-${PROJ_NAME}"
fi
SERVER_TARGET="leon-${PROJ_NAME}-server"

echo "Building game project: $PROJ"
echo "CMake target: $TARGET"
if [[ "$WITH_SERVER" == "1" ]]; then
  echo "Also building: $SERVER_TARGET"
fi
echo "Leon root: $ROOT"

BUILD_DIR="$PROJ/build-linux"
cmake -S "$PROJ" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLEON_REPO_ROOT="$ROOT" \
  -DLEON_ENGINE_ASSETS="$ROOT/Engine/Assets" \
  -DLEON_PROJECTS_ROOT="$ROOT/Projects"
cmake --build "$BUILD_DIR" --target "$TARGET"
if [[ "$WITH_SERVER" == "1" ]]; then
  cmake --build "$BUILD_DIR" --target "$SERVER_TARGET"
fi

EXE_PATH="$BUILD_DIR/$TARGET"
SERVER_EXE_PATH="$BUILD_DIR/$SERVER_TARGET"
if [[ ! -x "$EXE_PATH" ]]; then
  echo "ERROR: expected binary missing: $EXE_PATH"
  echo "LEON_BUILD_EXIT=1"
  exit 1
fi
if [[ "$WITH_SERVER" == "1" && ! -x "$SERVER_EXE_PATH" ]]; then
  echo "ERROR: expected server binary missing: $SERVER_EXE_PATH"
  echo "LEON_BUILD_EXIT=1"
  exit 1
fi

SHIP="$PROJ/Shipping"
echo
echo "Exporting portable game into the project folder:"
echo "  $SHIP"
rm -rf "$SHIP"
mkdir -p "$SHIP"
cp -f "$EXE_PATH" "$SHIP/$TARGET"
if [[ "$WITH_SERVER" == "1" ]]; then
  cp -f "$SERVER_EXE_PATH" "$SHIP/$SERVER_TARGET"
  cat > "$SHIP/run-dedicated.sh" <<EOF
#!/usr/bin/env bash
cd "\$(dirname "\$0")"
exec "./$SERVER_TARGET" --dedicated --port 7777 --tick 60 "\$@"
EOF
  chmod +x "$SHIP/run-dedicated.sh" "$SHIP/$SERVER_TARGET"
fi
chmod +x "$SHIP/$TARGET"
if [[ -d "$BUILD_DIR/assets" ]]; then
  cp -a "$BUILD_DIR/assets" "$SHIP/"
fi
if [[ -d "$BUILD_DIR/Projects" ]]; then
  cp -a "$BUILD_DIR/Projects" "$SHIP/"
fi

{
  echo "$PROJ_NAME — Leon game (portable)"
  echo
  echo "Run client:  ./$TARGET"
  if [[ "$WITH_SERVER" == "1" ]]; then
    echo "Run dedicated:  ./$SERVER_TARGET"
    echo "  or ./run-dedicated.sh"
    echo "Clients: Join Dedicated from Main Menu (same LAN IP:7777)"
  fi
  echo "This folder is inside your project: Shipping/"
  echo "Copy this entire Shipping folder to another machine to play."
  echo
  echo "Windows: build with Scripts/build-project.bat (no Linux→Win cross-compile)."
} > "$SHIP/README.txt"

echo
echo "========================================"
echo "Build OK"
echo "  Intermediate: $EXE_PATH"
if [[ "$WITH_SERVER" == "1" ]]; then
  echo "  Server:       $SERVER_EXE_PATH"
fi
echo "  Ship / play:  $SHIP/$TARGET"
if [[ "$WITH_SERVER" == "1" ]]; then
  echo "  Ship server:  $SHIP/$SERVER_TARGET"
fi
echo "========================================"
echo "LEON_BUILD_EXIT=0"
exit 0
