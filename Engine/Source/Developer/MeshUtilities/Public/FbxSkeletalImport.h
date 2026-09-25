#pragma once

#include "CoreMinimal.h"
#include "SkeletalAnimation.h"

/** Skinned mesh + skeleton (+ the embedded clip) from an FBX file (ufbx), in the engine world. */
[[nodiscard]] MESHUTILITIES_API bool LoadSkeletalMeshFromFbx(const FString& Path, FSkeletalMeshData& Out);

/**
 * Bakes the first animation stack of an FBX file against an existing skeleton (bones matched by name), in the engine
 * world: one track per bone of InSkeleton. The UAnimSequence asset is made from it (SetFromRawAnimSequence).
 */
[[nodiscard]] MESHUTILITIES_API bool LoadAnimSequenceFromFbx(
	const FString& Path, const FReferenceSkeleton& InSkeleton, FRawAnimSequence& Out);
