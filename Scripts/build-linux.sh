#!/usr/bin/env bash
# Build Leon on Linux. Default: Editor + tests.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
GENERATOR=()
if command -v ninja >/dev/null 2>&1; then
  GENERATOR=(-G Ninja)
fi

echo "==> Configure Editor/build-linux (${BUILD_TYPE})"
cmake -S Editor -B Editor/build-linux "${GENERATOR[@]}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DLEON_BUILD_TESTS=ON
cmake --build Editor/build-linux --config "${BUILD_TYPE}" -j"${JOBS}"

echo "Editor: Editor/build-linux/LeonEngine.exe (or ${BUILD_TYPE}/)"
echo "Tests:  ctest --test-dir Editor/build-linux -R '^leon\\.' --output-on-failure"
echo "Levels: Docs/LEVELS.md"
