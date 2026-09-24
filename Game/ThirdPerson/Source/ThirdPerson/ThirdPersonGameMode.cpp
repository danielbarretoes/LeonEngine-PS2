#include "ThirdPersonGameMode.h"

#include "Stats/DebugOverlay.h"
#include "InputPad.h"
#include "Window.h"
#include "Ps2RHI.h"

#include <cstdio>

namespace leon::ps2thirdperson {
namespace {

struct PrimitiveActor {
    float LocationX = 0.0f;
    float LocationY = 0.0f;
    float LocationZ = 0.0f;
    unsigned Yaw256 = 0;
    unsigned Pitch256 = 0;
    float ScaleX = 1.0f;
    float ScaleY = 1.0f;
    float ScaleZ = 1.0f;
    const rhi::Ps2Material* Material = nullptr;
};

struct Character {
    float LocationX = 0.0f;
    float LocationY = 2.0f;
    float LocationZ = 0.0f;
    unsigned Yaw256 = 0;
    float VelocityX = 0.0f;
    float VelocityY = 0.0f;
    float VelocityZ = 0.0f;
    bool OnGround = true;
};

/// Orbit follow camera — spherical boom around look-at (typical third-person).
struct SpringArm {
    float TargetArmLength = 28.0f;
    float MinArmLength = 12.0f;
    float SocketOffsetY = 3.0f;
    float ProbeRadius = 4.0f;
    float Yaw256 = 0.0f;
    float Pitch256 = 28.0f; // elevation above look-at (1/256-turn)
};

constexpr float kMoveSpeed = 0.55f;
constexpr float kCamYawRate = 2.8f;
constexpr float kCamPitchRate = 1.6f;
constexpr float kPitchMin = 14.0f; // avoid grazing the floor (near smear)
constexpr float kPitchMax = 72.0f;
constexpr float kGravity = 0.045f;
constexpr float kJumpSpeed = 0.95f;
constexpr float kCharHalfW = 1.6f;
constexpr float kCharHalfH = 2.4f;
constexpr float kGroundSkin = 0.08f;
constexpr float kArenaHalf = 70.0f;
constexpr float kGroundTopY = 0.0f;
// Tile the ground so no single quad straddles the camera (avoids stretch / total cull).
constexpr int kGroundTilesPerSide = 18;
constexpr int kGroundTileCount = kGroundTilesPerSide * kGroundTilesPerSide;
constexpr int kPropCount = 17;
constexpr int kLevelActorCount = kGroundTileCount + kPropCount;

constexpr float kNoSupport = -10000.0f;
constexpr float kMaxStepUp = 0.9f;
constexpr float kMaxFallCatch = 2.5f;

void PrintBanner(bool padOk) {
    std::printf("\n======= Leon Ps2ThirdPerson =======\n");
    std::printf("  Camera follows character + orbit\n");
    std::printf("  Large grounded primitive level\n");
    std::printf("  Left stick    move (cam-relative)\n");
    std::printf("  Right stick   camera orbit\n");
    std::printf("  Cross         jump\n");
    std::printf("  Start         quit | Select  debug HUD\n");
    std::printf("Pad=%s\n", padOk ? "ok" : "--");
    std::printf("Draw3D stats: HUD + PCSX2 console every 30 frames\n");
    std::printf("==================================\n\n");
}

[[nodiscard]] bool Edge(bool down, bool& prev) {
    const bool pressed = down && !prev;
    prev = down;
    return pressed;
}

/// Draw3D counters for the engine stats panel: boxes drawn / submitted, GS tris, clipped.
void FormatDrawHud(char* boxes, char* tris, unsigned outSize, const rhi::Ps2Draw3DDebugStats& s) {
    std::snprintf(boxes, outSize, "BOXES %u/%u", s.Boxes - s.CulledBoxes, s.Boxes);
    std::snprintf(tris, outSize, "TRIS %u CLIP %u", s.Emitted, s.Clipped);
}

[[nodiscard]] float MaxF(float a, float b) {
    return a > b ? a : b;
}

[[nodiscard]] float MinF(float a, float b) {
    return a < b ? a : b;
}

/// Box with bottom flush on Y = bottomY (no floating).
void PlaceGrounded(PrimitiveActor& out, float x, float z, float halfX, float halfY, float halfZ,
                   float bottomY, unsigned yaw256, const rhi::Ps2Material* material) {
    out.LocationX = x;
    out.LocationZ = z;
    out.ScaleX = halfX;
    out.ScaleY = halfY;
    out.ScaleZ = halfZ;
    out.LocationY = bottomY + halfY;
    out.Yaw256 = yaw256;
    out.Pitch256 = 0;
    out.Material = material;
}

/// World → box-local XZ (math3d matrix_rotate Y: world = (x·c + z·s, −x·s + z·c)).
void ToLocalXZ(const PrimitiveActor& a, float x, float z, float& lx, float& lz) {
    const float c = rhi::Ps2Cos256(a.Yaw256);
    const float sn = rhi::Ps2Sin256(a.Yaw256);
    const float dx = x - a.LocationX;
    const float dz = z - a.LocationZ;
    lx = dx * c - dz * sn;
    lz = dx * sn + dz * c;
}

/// World-axis half extents of a yawed box footprint (conservative AABB).
void FootprintExtents(const PrimitiveActor& a, float& ex, float& ez) {
    float c = rhi::Ps2Cos256(a.Yaw256);
    float sn = rhi::Ps2Sin256(a.Yaw256);
    c = c < 0.0f ? -c : c;
    sn = sn < 0.0f ? -sn : sn;
    ex = a.ScaleX * c + a.ScaleZ * sn;
    ez = a.ScaleX * sn + a.ScaleZ * c;
}

[[nodiscard]] bool FootprintOverlaps(const PrimitiveActor& a, float x, float z, float halfW) {
    float lx = 0.0f;
    float lz = 0.0f;
    ToLocalXZ(a, x, z, lx, lz);
    lx = lx < 0.0f ? -lx : lx;
    lz = lz < 0.0f ? -lz : lz;
    return lx < a.ScaleX + halfW && lz < a.ScaleZ + halfW;
}

[[nodiscard]] float FindSupportY(const PrimitiveActor* level, int count, float x, float z,
                                 float halfW, float feetY) {
    float best = kNoSupport;
    for (int i = 0; i < count; ++i) {
        const PrimitiveActor& a = level[i];
        if (!FootprintOverlaps(a, x, z, halfW)) {
            continue;
        }
        const float top = a.LocationY + a.ScaleY;
        if (top > feetY + kMaxStepUp) {
            continue;
        }
        if (top < feetY - kMaxFallCatch) {
            continue;
        }
        if (top > best) {
            best = top;
        }
    }
    return best;
}

/// Push the character (square footprint, box-local) out of props it cannot step onto.
void ResolveWallCollisions(const PrimitiveActor* props, int count, float& x, float& z,
                           float halfW, float feetY, float headY) {
    for (int i = 0; i < count; ++i) {
        const PrimitiveActor& a = props[i];
        const float top = a.LocationY + a.ScaleY;
        const float bottom = a.LocationY - a.ScaleY;
        if (top <= feetY + kMaxStepUp || bottom >= headY) {
            continue;
        }
        float lx = 0.0f;
        float lz = 0.0f;
        ToLocalXZ(a, x, z, lx, lz);
        const float penX = a.ScaleX + halfW - (lx < 0.0f ? -lx : lx);
        const float penZ = a.ScaleZ + halfW - (lz < 0.0f ? -lz : lz);
        if (penX <= 0.0f || penZ <= 0.0f) {
            continue;
        }
        // Minimum-penetration axis in box space, then rotate the push back to world.
        float px = 0.0f;
        float pz = 0.0f;
        if (penX < penZ) {
            px = lx < 0.0f ? -penX : penX;
        } else {
            pz = lz < 0.0f ? -penZ : penZ;
        }
        const float c = rhi::Ps2Cos256(a.Yaw256);
        const float sn = rhi::Ps2Sin256(a.Yaw256);
        x += px * c + pz * sn;
        z += -px * sn + pz * c;
    }
}

[[nodiscard]] unsigned WrapYaw256(float yaw) {
    while (yaw < 0.0f) {
        yaw += 256.0f;
    }
    while (yaw >= 256.0f) {
        yaw -= 256.0f;
    }
    return static_cast<unsigned>(yaw) & 255u;
}

/// Ray (origin + t*dir, t in [0,1], dir = full boom vector) vs AABB. Returns true if hit.
[[nodiscard]] bool RayAabbHit(float ox, float oy, float oz, float dx, float dy, float dz, float minX,
                              float minY, float minZ, float maxX, float maxY, float maxZ,
                              float& tHit) {
    float tMin = 0.0f;
    float tMax = 1.0f;
    const float o[3] = {ox, oy, oz};
    const float d[3] = {dx, dy, dz};
    const float bmin[3] = {minX, minY, minZ};
    const float bmax[3] = {maxX, maxY, maxZ};
    for (int i = 0; i < 3; ++i) {
        if (d[i] > -0.00001f && d[i] < 0.00001f) {
            if (o[i] < bmin[i] || o[i] > bmax[i]) {
                return false;
            }
            continue;
        }
        float inv = 1.0f / d[i];
        float t0 = (bmin[i] - o[i]) * inv;
        float t1 = (bmax[i] - o[i]) * inv;
        if (t0 > t1) {
            const float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        if (t0 > tMin) {
            tMin = t0;
        }
        if (t1 < tMax) {
            tMax = t1;
        }
        if (tMin > tMax) {
            return false;
        }
    }
    tHit = tMin;
    return tMin >= 0.0f && tMin <= 1.0f;
}

[[nodiscard]] float ProbeBoomLength(float lookX, float lookY, float lookZ, float camX, float camY,
                                    float camZ, float desiredLen, float minLen, float radius,
                                    const PrimitiveActor* level, int count) {
    const float dx = camX - lookX;
    const float dy = camY - lookY;
    const float dz = camZ - lookZ;
    float bestT = 1.0f;
    for (int i = 0; i < count; ++i) {
        const PrimitiveActor& a = level[i];
        float ex = 0.0f;
        float ez = 0.0f;
        FootprintExtents(a, ex, ez);
        const float minX = a.LocationX - ex - radius;
        const float maxX = a.LocationX + ex + radius;
        const float minY = a.LocationY - a.ScaleY - radius;
        const float maxY = a.LocationY + a.ScaleY + radius;
        const float minZ = a.LocationZ - ez - radius;
        const float maxZ = a.LocationZ + ez + radius;
        float t = 0.0f;
        if (RayAabbHit(lookX, lookY, lookZ, dx, dy, dz, minX, minY, minZ, maxX, maxY, maxZ, t)) {
            if (t < bestT) {
                bestT = t;
            }
        }
    }
    // Pull in slightly before the hit so the near plane stays clear of the surface.
    float len = desiredLen * bestT - radius * 0.35f;
    if (len < minLen) {
        len = minLen;
    }
    if (len > desiredLen) {
        len = desiredLen;
    }
    return len;
}

void UpdateViewTarget(float lookX, float lookY, float lookZ, const SpringArm& arm, float& armLen,
                      const PrimitiveActor* level, int levelCount) {
    constexpr float kTwoPi = 6.28318530718f;
    constexpr float kCamGroundClearance = 3.5f;
    const unsigned yawU = WrapYaw256(arm.Yaw256);
    const unsigned pitchU = static_cast<unsigned>(arm.Pitch256) & 255u;
    const float horizUnit = rhi::Ps2Cos256(pitchU);
    const float elevUnit = rhi::Ps2Sin256(pitchU);
    const float dirX = rhi::Ps2Sin256(yawU) * horizUnit;
    const float dirY = elevUnit;
    const float dirZ = rhi::Ps2Cos256(yawU) * horizUnit;

    const float desired = arm.TargetArmLength;
    const float fullX = lookX + dirX * desired;
    const float fullY = lookY + dirY * desired;
    const float fullZ = lookZ + dirZ * desired;
    const float probed =
        ProbeBoomLength(lookX, lookY, lookZ, fullX, fullY, fullZ, desired, arm.MinArmLength,
                        arm.ProbeRadius, level + kGroundTileCount, levelCount - kGroundTileCount);

    // Snap in fast on collision; ease out when clear.
    if (probed < armLen) {
        armLen = probed;
    } else {
        armLen = armLen * 0.82f + probed * 0.18f;
    }

    rhi::Ps2ViewTarget view{};
    view.LocationX = lookX + dirX * armLen;
    view.LocationY = lookY + dirY * armLen;
    view.LocationZ = lookZ + dirZ * armLen;
    if (view.LocationY < kCamGroundClearance) {
        view.LocationY = kCamGroundClearance;
    }
    view.Pitch = -(static_cast<float>(pitchU) * kTwoPi) / 256.0f;
    view.Yaw = (static_cast<float>(yawU) * kTwoPi) / 256.0f;
    rhi::Ps2SetViewTarget(view);
}

void BuildLevel(PrimitiveActor* out, const rhi::Ps2Material* mGround,
                const rhi::Ps2Material* mPlatform, const rhi::Ps2Material* mCrate) {
    const float tileHalf = kArenaHalf / static_cast<float>(kGroundTilesPerSide);
    int idx = 0;
    for (int gz = 0; gz < kGroundTilesPerSide; ++gz) {
        for (int gx = 0; gx < kGroundTilesPerSide; ++gx) {
            PrimitiveActor& tile = out[idx++];
            tile = {};
            tile.LocationX =
                -kArenaHalf + tileHalf + static_cast<float>(gx) * (tileHalf * 2.0f);
            tile.LocationY = -0.5f;
            tile.LocationZ =
                -kArenaHalf + tileHalf + static_cast<float>(gz) * (tileHalf * 2.0f);
            tile.ScaleX = tileHalf;
            tile.ScaleY = 0.5f;
            tile.ScaleZ = tileHalf;
            tile.Material = mGround;
        }
    }

    // Grounded crates / blocks (bottom on Y=0).
    PlaceGrounded(out[idx++], 14.0f, -10.0f, 2.0f, 2.0f, 2.0f, kGroundTopY, 20, mCrate);
    PlaceGrounded(out[idx++], -16.0f, 8.0f, 2.5f, 2.5f, 2.5f, kGroundTopY, 40, mCrate);
    PlaceGrounded(out[idx++], 8.0f, 18.0f, 2.0f, 2.0f, 2.0f, kGroundTopY, 10, mCrate);
    PlaceGrounded(out[idx++], -22.0f, -14.0f, 3.0f, 1.5f, 3.0f, kGroundTopY, 0, mCrate);
    PlaceGrounded(out[idx++], 28.0f, 6.0f, 2.2f, 2.2f, 2.2f, kGroundTopY, 55, mCrate);
    PlaceGrounded(out[idx++], -8.0f, 28.0f, 2.0f, 2.0f, 2.0f, kGroundTopY, 30, mCrate);
    PlaceGrounded(out[idx++], 20.0f, -28.0f, 2.5f, 1.8f, 2.5f, kGroundTopY, 15, mCrate);

    PlaceGrounded(out[idx++], -32.0f, -24.0f, 6.0f, 5.0f, 6.0f, kGroundTopY, 0, mPlatform);
    PlaceGrounded(out[idx++], 36.0f, 22.0f, 5.0f, 4.0f, 5.0f, kGroundTopY, 0, mPlatform);
    PlaceGrounded(out[idx++], -40.0f, 30.0f, 4.0f, 3.5f, 4.0f, kGroundTopY, 12, mPlatform);

    PlaceGrounded(out[idx++], 0.0f, -40.0f, 6.0f, 1.0f, 4.0f, kGroundTopY, 0, mPlatform);
    PlaceGrounded(out[idx++], 0.0f, -40.0f, 4.5f, 1.0f, 3.0f, kGroundTopY + 2.0f, 0, mPlatform);
    PlaceGrounded(out[idx++], 0.0f, -40.0f, 3.0f, 1.0f, 2.0f, kGroundTopY + 4.0f, 0, mPlatform);

    PlaceGrounded(out[idx++], 45.0f, 0.0f, 1.5f, 3.0f, 12.0f, kGroundTopY, 0, mPlatform);
    PlaceGrounded(out[idx++], -45.0f, -8.0f, 1.5f, 3.0f, 10.0f, kGroundTopY, 0, mPlatform);
    PlaceGrounded(out[idx++], 10.0f, 45.0f, 14.0f, 2.5f, 1.5f, kGroundTopY, 0, mPlatform);
    PlaceGrounded(out[idx++], -12.0f, -48.0f, 10.0f, 2.5f, 1.5f, kGroundTopY, 0, mPlatform);
}

} // namespace

int RunPs2ThirdPersonDemo(Window& window) {
    for (int i = 0; i < 2; ++i) {
        rhi::Ps2ClearColor(0.08f, 0.10f, 0.14f);
        window.SwapBuffers();
    }

    const bool padOk = InitializePad();
    PrintBanner(padOk);

    const rhi::Ps2Texture texGrid = rhi::Ps2Texture::CreateGrid(64);
    const rhi::Ps2Texture texChecker = rhi::Ps2Texture::CreateChecker(64);

    rhi::Ps2Material mGround{};
    mGround.BaseColorR = 0.72f;
    mGround.BaseColorG = 0.76f;
    mGround.BaseColorB = 0.70f;
    mGround.BaseColorMap = texGrid.Valid() ? &texGrid : nullptr;

    rhi::Ps2Material mPlatform{};
    mPlatform.BaseColorR = 0.52f;
    mPlatform.BaseColorG = 0.56f;
    mPlatform.BaseColorB = 0.62f;
    mPlatform.BaseColorMap = texChecker.Valid() ? &texChecker : nullptr;

    rhi::Ps2Material mCrate{};
    mCrate.BaseColorR = 0.85f;
    mCrate.BaseColorG = 0.55f;
    mCrate.BaseColorB = 0.25f;
    mCrate.BaseColorMap = texChecker.Valid() ? &texChecker : nullptr;

    rhi::Ps2Material mCharacter{};
    mCharacter.BaseColorR = 0.25f;
    mCharacter.BaseColorG = 0.55f;
    mCharacter.BaseColorB = 0.95f;

    PrimitiveActor level[kLevelActorCount];
    BuildLevel(level, &mGround, &mPlatform, &mCrate);

    Character character{};
    {
        const float spawnSupport =
            FindSupportY(level, kLevelActorCount, 0.0f, 0.0f, kCharHalfW, 0.0f);
        character.LocationY =
            (spawnSupport > kNoSupport ? spawnSupport : kGroundTopY) + kCharHalfH;
    }

    SpringArm cameraBoom{};
    float armLen = cameraBoom.TargetArmLength;
    // Smoothed look-at so the boom follows the character without hard snaps.
    float lookX = character.LocationX;
    float lookY = character.LocationY + cameraBoom.SocketOffsetY;
    float lookZ = character.LocationZ;

    rhi::DirectionalLight sun{};
    sun.Intensity = 1.05f;
    sun.Yaw256 = 20;
    sun.Pitch256 = 78;
    rhi::Ps2SetDirectionalLight(sun);
    rhi::Ps2SetAmbientLightColor(0.16f, 0.18f, 0.24f);

    UpdateViewTarget(lookX, lookY, lookZ, cameraBoom, armLen, level, kLevelActorCount);

    unsigned frame = 0;
    bool prevCross = false;

    char hudBoxes[32] = "BOXES --";
    char hudTris[32] = "TRIS --";

    for (;;) {
        window.PollEvents();
        if (IsPadButtonPressed(EPadButton::Start)) {
            break;
        }

        const PadStick left = GetPadLeftStick();
        const PadStick right = GetPadRightStick();

        // Right stick: orbit look (negate so stick right / up match expected yaw/pitch).
        cameraBoom.Yaw256 -= right.X * kCamYawRate;
        while (cameraBoom.Yaw256 < 0.0f) {
            cameraBoom.Yaw256 += 256.0f;
        }
        while (cameraBoom.Yaw256 >= 256.0f) {
            cameraBoom.Yaw256 -= 256.0f;
        }
        cameraBoom.Pitch256 -= right.Y * kCamPitchRate;
        if (cameraBoom.Pitch256 < kPitchMin) {
            cameraBoom.Pitch256 = kPitchMin;
        }
        if (cameraBoom.Pitch256 > kPitchMax) {
            cameraBoom.Pitch256 = kPitchMax;
        }

        const unsigned yawU = WrapYaw256(cameraBoom.Yaw256);
        float wishX = 0.0f;
        float wishZ = 0.0f;
        const float fx = -rhi::Ps2Sin256(yawU);
        const float fz = -rhi::Ps2Cos256(yawU);
        const float rx = rhi::Ps2Cos256(yawU);
        const float rz = -rhi::Ps2Sin256(yawU);
        // Left stick: Y = forward/back, X = strafe (camera-relative).
        // Camera basis is orthonormal, so |wish| matches stick magnitude.
        wishX = fx * left.Y + rx * left.X;
        wishZ = fz * left.Y + rz * left.X;
        // Each stick axis reaches ±1 independently: cap diagonals to unit length.
        const float wishLen2 = wishX * wishX + wishZ * wishZ;
        if (wishLen2 > 1.0f) {
            // One Newton step from 1 is enough for |wish| in (1, √2].
            const float invLen = 1.0f / (0.5f * (wishLen2 + 1.0f));
            wishX *= invLen;
            wishZ *= invLen;
        }

        if (wishX != 0.0f || wishZ != 0.0f) {
            character.VelocityX = wishX * kMoveSpeed;
            character.VelocityZ = wishZ * kMoveSpeed;
            character.Yaw256 = yawU;
        } else {
            character.VelocityX *= 0.7f;
            character.VelocityZ *= 0.7f;
            if (character.VelocityX > -0.02f && character.VelocityX < 0.02f) {
                character.VelocityX = 0.0f;
            }
            if (character.VelocityZ > -0.02f && character.VelocityZ < 0.02f) {
                character.VelocityZ = 0.0f;
            }
        }

        if (Edge(IsPadButtonPressed(EPadButton::Cross), prevCross) && character.OnGround) {
            character.VelocityY = kJumpSpeed;
            character.OnGround = false;
        }

        character.VelocityY -= kGravity;
        character.LocationX += character.VelocityX;
        character.LocationY += character.VelocityY;
        character.LocationZ += character.VelocityZ;

        const float bound = kArenaHalf - 2.0f;
        character.LocationX = MinF(MaxF(character.LocationX, -bound), bound);
        character.LocationZ = MinF(MaxF(character.LocationZ, -bound), bound);
        ResolveWallCollisions(level + kGroundTileCount, kPropCount, character.LocationX,
                              character.LocationZ, kCharHalfW, character.LocationY - kCharHalfH,
                              character.LocationY + kCharHalfH);

        const float feet = character.LocationY - kCharHalfH;
        const float support = FindSupportY(level, kLevelActorCount, character.LocationX,
                                           character.LocationZ, kCharHalfW, feet);
        if (support > kNoSupport && character.VelocityY <= 0.0f &&
            feet <= support + kGroundSkin) {
            character.LocationY = support + kCharHalfH;
            character.VelocityY = 0.0f;
            character.OnGround = true;
        } else {
            character.OnGround = false;
        }

        // Follow character: XZ snappy, Y slightly soft (jump/land).
        lookX = character.LocationX;
        lookZ = character.LocationZ;
        const float targetLookY = character.LocationY + cameraBoom.SocketOffsetY;
        lookY = lookY * 0.70f + targetLookY * 0.30f;

        sun.Yaw256 = (20u + (frame / 3u)) & 255u;
        rhi::Ps2SetDirectionalLight(sun);
        UpdateViewTarget(lookX, lookY, lookZ, cameraBoom, armLen, level, kLevelActorCount);

        rhi::Ps2ClearColor(0.12f, 0.16f, 0.22f);

        for (int i = 0; i < kLevelActorCount; ++i) {
            const PrimitiveActor& a = level[i];
            rhi::Ps2BindMaterial(*a.Material);
            (void)rhi::Ps2DrawBox(a.LocationX, a.LocationY, a.LocationZ, a.Yaw256, a.Pitch256,
                                  a.ScaleX, a.ScaleY, a.ScaleZ);
        }

        rhi::Ps2BindMaterial(mCharacter);
        (void)rhi::Ps2DrawBox(character.LocationX, character.LocationY, character.LocationZ,
                              character.Yaw256, 0, kCharHalfW, kCharHalfH, kCharHalfW);
        (void)rhi::Ps2DrawBox(character.LocationX, character.LocationY + kCharHalfH * 0.85f,
                              character.LocationZ, character.Yaw256, 0, kCharHalfW * 0.55f,
                              kCharHalfW * 0.55f, kCharHalfW * 0.55f);

        rhi::Ps2Draw3DDebugStats drawStats{};
        rhi::Ps2Draw3DDebugGetStats(drawStats);
        FormatDrawHud(hudBoxes, hudTris, sizeof(hudBoxes), drawStats);
        SetStatsHudExtraLine(0, hudBoxes);
        SetStatsHudExtraLine(1, hudTris);
        if ((frame % 30u) == 0u) {
            rhi::Ps2Draw3DDebugPrint(drawStats);
        }

        window.SwapBuffers();
        ++frame;
    }

    SetStatsHudExtraLine(0, nullptr);
    SetStatsHudExtraLine(1, nullptr);
    std::printf("Ps2ThirdPerson: quit after %u frames\n", frame);
    return 0;
}

} // namespace leon::ps2thirdperson
