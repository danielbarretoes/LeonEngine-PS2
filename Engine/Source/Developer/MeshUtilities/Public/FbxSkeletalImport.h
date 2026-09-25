#pragma once

#include "CoreMinimal.h"
#include "SkeletalAnimation.h"

/** Skinned mesh + skeleton (+ the embedded clip) from an FBX file, Y-up (ufbx). */
[[nodiscard]] MESHUTILITIES_API bool LoadSkeletalMeshFromFbx(const FString& Path, FSkeletalMeshData& Out);

/** Bakes the first animation stack of an FBX file against an existing skeleton (bones matched by name). */
[[nodiscard]] MESHUTILITIES_API bool LoadAnimSequenceFromFbx(
	const FString& Path, const USkeleton& InSkeleton, UAnimSequence& Out);
