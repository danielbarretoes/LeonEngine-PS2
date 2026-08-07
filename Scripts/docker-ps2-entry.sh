#!/bin/sh
# Entry used by build-ps2-docker.ps1 / .sh inside ghcr.io/ps2dev/ps2dev
set -eu
TARGET="${1:-lab}"
export PS2DEV="${PS2DEV:-/usr/local/ps2dev}"
export PS2SDK="${PS2SDK:-$PS2DEV/ps2sdk}"
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PATH"
if ! command -v cmake >/dev/null 2>&1; then
  apk add --no-cache cmake ninja make git
fi
chmod +x Scripts/build-ps2.sh Scripts/docker-ps2-entry.sh
exec Scripts/build-ps2.sh "$TARGET"
