#!/usr/bin/env sh
# Build PS2 targets inside the official ps2dev Docker image (Alpine; no bash).
# Usage: Scripts/build-ps2-docker.sh [hello|lab]
set -eu
ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
TARGET="${1:-lab}"
IMAGE="${LEON_PS2DEV_IMAGE:-ghcr.io/ps2dev/ps2dev:latest}"

docker run --rm \
  -v "${ROOT}:/src" \
  -w /src \
  "$IMAGE" \
  sh /src/Scripts/docker-ps2-entry.sh "$TARGET"
