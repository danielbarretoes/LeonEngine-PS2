# The PS2 build image: the pinned ps2dev image (BASE_IMAGE, LeonBuildPS2.cmake's DOCKER_IMAGE) plus CMake, Ninja and the
# host g++ (LeonHeaderTool), so a build does not run `apk add` each time, and xorriso, which BuildCookRun -iso makes the
# disc image with (Docs/PLANS/ps2-shipping.md N23). LeonBuildTool (and BuildCookRun) builds it on first use and tags it
# leon/ps2-build:<hash of this file and the base image>; nothing to do by hand.
ARG BASE_IMAGE
FROM ${BASE_IMAGE}
RUN apk add --no-cache cmake ninja make g++ musl-dev xorriso
WORKDIR /leon
ENV PS2DEV=/usr/local/ps2dev
ENV PS2SDK=/usr/local/ps2dev/ps2sdk
ENV PATH="${PS2DEV}/bin:${PS2DEV}/ee/bin:${PS2DEV}/iop/bin:${PS2DEV}/dvp/bin:${PATH}"
