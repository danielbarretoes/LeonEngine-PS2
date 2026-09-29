#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"
#include "SkeletalAnimation.h"

/**
 * Loads the meshes of a .gltf / .glb (all primitives merged, one section and material slot per primitive) into
 * FMeshData, in the engine world. Each slot takes its glTF material: the name, the base colour factor and its base
 * colour image, an external file (AlbedoMapPaths) or an embedded one (a .glb's buffer view or a `data:` URI:
 * AlbedoMapImages). A node named `SOCKET_<Name>` (UE's FBX convention) becomes a socket <Name>, placed relative to the
 * nearest ancestor node with a mesh (its world transform without one). Edit time only (UGLTFImportFactory).
 */
[[nodiscard]] MESHUTILITIES_API bool LoadStaticMeshFromGltf(const FString& Path, FMeshData& Out, FString& OutError);

/** A socket of a skeleton from its source: a `SOCKET_<Name>` node under a joint (UE: USkeletalMeshSocket). */
struct MESHUTILITIES_API FBoneSocketData
{
	/** The socket's name, without the prefix. */
	FString Name;
	/** The bone it follows: the node's nearest ancestor joint. */
	FName BoneName;
	/** The socket in the bone's space at rest, in the engine's axes and centimetres. */
	FTransform RelativeTransform;
};

/**
 * A skinned mesh as the glTF importer reads it (UE: FSkeletalMeshImportData), in the engine world: the skeleton of
 * the file's skin, the bind-pose geometry with its material slots (FMeshData, as a static mesh's), one FSkinWeightInfo
 * per vertex and the skeleton's sockets.
 */
struct MESHUTILITIES_API FSkeletalMeshImportData
{
	FReferenceSkeleton RefSkeleton;
	FMeshData Mesh;
	TArray<FSkinWeightInfo> SkinWeights;
	TArray<FBoneSocketData> Sockets;
};

/**
 * Loads the skinned mesh of a .gltf / .glb: the first skin a mesh node uses and every mesh node skinned with it
 * (merged: one section and material slot per primitive, the materials as LoadStaticMeshFromGltf reads them).
 *
 * - **Skeleton**: the skin's joints, reordered so that a parent comes before its children (a joint's parent is its
 *   nearest ancestor joint), at most MaxSkinBones; the reference pose is each joint node's local transform (with the
 *   transforms of the nodes between it and its parent joint, or above a root joint), the inverse bind pose the skin's
 *   inverseBindMatrices (the identity without them).
 * - **Weights**: JOINTS_n / WEIGHTS_n of every set, normalized; the two largest are kept and renormalized (the PS2's
 *   two influences: Docs/ASSET_FORMATS.md), quantized to 1/255 steps that add up to 255.
 * - **Sockets**: each `SOCKET_<Name>` node under a joint, relative to the nearest ancestor joint at rest.
 *
 * Positions, normals, transforms and matrices are converted from glTF's right-handed Y-up metres to the engine world
 * (FImportCoordinateConversion: (x, z, y) x 100 cm). False with OutError for a file without a skinned mesh, or a
 * skeleton over the limit. Edit time only.
 */
[[nodiscard]] MESHUTILITIES_API bool LoadSkeletalMeshFromGltf(
	const FString& Path, FSkeletalMeshImportData& Out, FString& OutError);

/**
 * Samples the animations of a .gltf / .glb against InSkeleton (every one, or only the one named AnimationName) into
 * clips of local-space tracks at 30 Hz, one per bone of InSkeleton (UE: FRawAnimSequenceTrack), named after the glTF
 * animations (`Animation_<Index>` for one without a name).
 *
 * - The file must have InSkeleton's bones: a node named after each bone whose nearest ancestor bone is the bone's
 *   parent, and, when the file has a skin, joints with the same names, order and parents.
 * - Channels: translation, rotation and scale with LINEAR (rotations slerped) or STEP interpolation; a bone without a
 *   channel keeps its rest transform. CUBICSPLINE is rejected (re-export with linear sampling: Blender's "Always Sample
 *   Animations"); morph target weights are ignored.
 * - A clip starts at its first key and lasts to its last one, rounded to whole 30 Hz frames; a STEP key lands on the
 *   frame at or after its time. Transforms are converted to the engine world as LoadSkeletalMeshFromGltf's.
 *
 * False with OutError on a mismatch, CUBICSPLINE, a missing AnimationName or a file without animations.
 */
[[nodiscard]] MESHUTILITIES_API bool LoadAnimSequencesFromGltf(const FString& Path,
	const FReferenceSkeleton& InSkeleton, const FString& AnimationName, TArray<FRawAnimSequence>& Out,
	FString& OutError);
