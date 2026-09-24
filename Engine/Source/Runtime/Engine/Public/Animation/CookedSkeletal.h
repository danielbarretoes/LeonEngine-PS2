#pragma once

#include "SkeletalAnimation.h"

#include <cstdint>
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

inline constexpr std::uint32_t LeonSkeletonMagic = 0x314B534Cu; // 'LSK1'
inline constexpr std::uint32_t LeonSkelMeshMagic = 0x314D4B4Cu; // 'LKM1'
inline constexpr std::uint32_t LeonAnimMagic = 0x314E414Cu; // 'LAN1'
inline constexpr int CookedFormatVersion = 1;

[[nodiscard]] bool SaveSkeletonLeon(const std::string& Path, const USkeleton& Skeleton, const std::string& InName);
[[nodiscard]] bool LoadSkeleton(const std::string& Path, USkeleton& Out, std::string* OutName = nullptr);

[[nodiscard]] bool SaveSkeletalMeshLeon(const std::string& Path, const FSkeletalMeshData& Data,
	const std::string& SkeletonRelPath, const std::string& MaterialRelPath, const std::string& AssetName);
[[nodiscard]] bool LoadSkeletalMesh(const std::string& Path, FSkeletalMeshData& Out,
	USkeleton* SkeletonOverride = nullptr, std::string* OutMaterialRelPath = nullptr);

[[nodiscard]] bool SaveAnimSequenceLeon(
	const std::string& Path, const UAnimSequence& Anim, int BoneCount, const std::string& SkeletonRelPath);
[[nodiscard]] bool LoadAnimSequence(const std::string& Path, UAnimSequence& Out);

/// UBlendSpace1D descriptor; `samples[].anim` are paths relative to the blendspace file.
struct ENGINE_API FBlendSpace1DAssetDesc
{
	std::string Name = "BlendSpace1D";
	float AxisMin = 0.0f;
	float AxisMax = 1.0f;
	struct FSample
	{
		std::string AnimRelPath;
		float Position = 0.0f;
	};
	std::vector<FSample> Samples;
};

[[nodiscard]] bool SaveBlendSpace1DJson(const std::string& Path, const FBlendSpace1DAssetDesc& Desc);
[[nodiscard]] bool LoadBlendSpace1DJson(const std::string& Path, FBlendSpace1DAssetDesc& Out);

struct ENGINE_API FCharacterVisualDesc
{
	std::string Name = "Character";
	std::string SkeletalMeshRel; // *.lskm
	std::string BlendSpaceRel; // *.blendspace1d.json
	float FitHeight = 1.85f;
	/// Optional jump state-machine clips (paths relative to the `.lchar`).
	std::string JumpStartAnimRel; // Jumping Up (one-shot)
	std::string FallLoopAnimRel; // Falling Idle (loop)
	std::string LandAnimRel; // Falling To Landing (one-shot)
};

/// Leon Character package (`.lchar` — INI-style, like `.lmat`).
[[nodiscard]] bool SaveCharacterVisualLchar(const std::string& Path, const FCharacterVisualDesc& Desc);
[[nodiscard]] bool LoadCharacterVisualLchar(const std::string& Path, FCharacterVisualDesc& Out);
/// Load `.lchar` (preferred) or legacy `.character.json`.
[[nodiscard]] bool LoadCharacterVisual(const std::string& Path, FCharacterVisualDesc& Out);

/// Optional Mixamo jump / fall / land FBX paths for UAnimInstance jump SM.
struct ENGINE_API FCookJumpAnimPaths
{
	std::string JumpStartFbx; // Jumping Up
	std::string FallLoopFbx; // Falling Idle
	std::string LandFbx; // Falling To Landing
};

/// Cook a single UAnimSequence FBX into Anims/<name>.lanim (reuses existing skeleton).
[[nodiscard]] bool CookAnimSequenceFromFbx(const std::string& FbxPath, const std::string& SkeletonPath,
	const std::string& OutAnimPath, const std::string& AnimName, bool bLooping = true);

/// Cook Mixamo-style sources into an Unreal-like character folder (skeleton/mesh/mat/Anims/BS).
[[nodiscard]] bool CookCharacterFromFbx(const std::string& CharacterName, const std::string& MeshFbxPath,
	const std::string& RunFbxPath, const std::string& OutDirectory, const FCookJumpAnimPaths& JumpAnims = {});
