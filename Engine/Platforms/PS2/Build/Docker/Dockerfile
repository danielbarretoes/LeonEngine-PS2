# Optional local PS2 build image: the pinned ps2dev image plus CMake and Ninja preinstalled, so
# LeonBuildTool does not run `apk add` on every build.
#   docker build -t leon/ps2dev Engine/Platforms/PS2/Build/Docker
# Then point LeonBuildPS2.cmake's DOCKER_IMAGE at leon/ps2dev (or keep the default image).
FROM ghcr.io/ps2dev/ps2dev@sha256:79c24d3762f5cfeee0beabeacc94480fe580d379d3e48a0bfbac681a8895daf6
RUN apk add --no-cache cmake ninja make
WORKDIR /leon
ENV PS2DEV=/usr/local/ps2dev
ENV PS2SDK=/usr/local/ps2dev/ps2sdk
ENV PATH="${PS2DEV}/bin:${PS2DEV}/ee/bin:${PS2DEV}/iop/bin:${PS2DEV}/dvp/bin:${PATH}"
