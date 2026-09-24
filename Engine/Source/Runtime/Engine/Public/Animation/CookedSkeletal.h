#pragma once

#include <cstdint>
#include "SkeletalAnimation.h"
#include <string>
#include <vector>


/// Unreal-like cooked skeletal content (micro-engine files, not UObject).
/// Sample pack: Templates/ThirdPerson/Content/assets/characters/bot/
///   Bot.lskel
///   Bot.lskm
///   Materials/M_Bot.lmat
///   Anims/<Clip>.lanim
///   Bot_Locomotion.blendspace1d.json
///   Bot.lchar  (Leon character package — mesh + anim refs + fitHeight)

inline constexpr std::uint32_t kLeonSkeletonMagic = 0x314B534Cu;  // 'LSK1'
inline constexpr std::uint32_t kLeonSkelMeshMagic = 0x314D4B4Cu;  // 'LKM1'
inline constexpr std::uint32_t kLeonAnimMagic = 0x314E414Cu;      // 'LAN1'
inline constexpr int kCookedFormatVersion = 1;

[[nodiscard]] bool SaveSkeletonLeon(const std::string& path, const USkeleton& skeleton,
                                  const std::string& name);
[[nodiscard]] bool LoadSkeleton(const std::string& path, USkeleton& out,
                                std::string* outName = nullptr);

[[nodiscard]] bool SaveSkeletalMeshLeon(const std::string& path, const FSkeletalMeshData& data,
                                        const std::string& skeletonRelPath,
                                        const std::string& materialRelPath,
                                        const std::string& assetName);
[[nodiscard]] bool LoadSkeletalMesh(const std::string& path, FSkeletalMeshData& out,
                                    USkeleton* skeletonOverride = nullptr,
                                    std::string* outMaterialRelPath = nullptr);

[[nodiscard]] bool SaveAnimSequenceLeon(const std::string& path, const UAnimSequence& anim,
                                        int boneCount, const std::string& skeletonRelPath);
[[nodiscard]] bool LoadAnimSequence(const std::string& path, UAnimSequence& out);

/// UBlendSpace1D descriptor; `samples[].anim` are paths relative to the blendspace file.
struct BlendSpace1DAssetDesc {
    std::string name = "BlendSpace1D";
    float axisMin = 0.0f;
    float axisMax = 1.0f;
    struct Sample {
        std::string animRelPath;
        float position = 0.0f;
    };
    std::vector<Sample> samples;
};

[[nodiscard]] bool SaveBlendSpace1DJson(const std::string& path, const BlendSpace1DAssetDesc& desc);
[[nodiscard]] bool LoadBlendSpace1DJson(const std::string& path, BlendSpace1DAssetDesc& out);

struct CharacterVisualDesc {
    std::string name = "Character";
    std::string skeletalMeshRel; // *.lskm
    std::string blendSpaceRel;   // *.blendspace1d.json
    float fitHeight = 1.85f;
    /// Optional jump state-machine clips (paths relative to the `.lchar`).
    std::string jumpStartAnimRel; // Jumping Up (one-shot)
    std::string fallLoopAnimRel;  // Falling Idle (loop)
    std::string landAnimRel;      // Falling To Landing (one-shot)
};

/// Leon Character package (`.lchar` — INI-style, like `.lmat`).
[[nodiscard]] bool SaveCharacterVisualLchar(const std::string& path,
                                            const CharacterVisualDesc& desc);
[[nodiscard]] bool LoadCharacterVisualLchar(const std::string& path, CharacterVisualDesc& out);
/// Load `.lchar` (preferred) or legacy `.character.json`.
[[nodiscard]] bool LoadCharacterVisual(const std::string& path, CharacterVisualDesc& out);

/// Optional Mixamo jump / fall / land FBX paths for UAnimInstance jump SM.
struct CookJumpAnimPaths {
    std::string jumpStartFbx; // Jumping Up
    std::string fallLoopFbx;  // Falling Idle
    std::string landFbx;      // Falling To Landing
};

/// Cook a single UAnimSequence FBX into Anims/<name>.lanim (reuses existing skeleton).
[[nodiscard]] bool CookAnimSequenceFromFbx(const std::string& fbxPath,
                                           const std::string& skeletonPath,
                                           const std::string& outAnimPath,
                                           const std::string& animName, bool looping = true);

/// Cook Mixamo-style sources into an Unreal-like character folder (skeleton/mesh/mat/Anims/BS).
[[nodiscard]] bool CookCharacterFromFbx(const std::string& characterName,
                                        const std::string& meshFbxPath,
                                        const std::string& runFbxPath,
                                        const std::string& outDirectory,
                                        const CookJumpAnimPaths& jumpAnims = {});

