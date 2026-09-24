#pragma once

#include "SkeletalAnimation.h"

#include <string>

/// Skinned mesh + skeleton (+ the embedded clip) from an FBX file, Y-up (ufbx).
[[nodiscard]] bool LoadSkeletalMeshFromFbx(const std::string& Path, FSkeletalMeshData& Out);

/// Bakes the first animation stack of an FBX file against an existing skeleton (bones matched by name).
[[nodiscard]] bool LoadAnimSequenceFromFbx(const std::string& Path, const USkeleton& InSkeleton, UAnimSequence& Out);
