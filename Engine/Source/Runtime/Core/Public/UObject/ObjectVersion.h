#pragma once

#include "CoreTypes.h"

/**
 * Versions of the package format, `.lasset` / `.lmap` (UE: EUnrealEngineObjectUE4Version in UObject/ObjectVersion.h).
 * A package records the version it was saved with (FPackageFileSummary::FileVersionUE) and its linker hands it to the
 * serialization code through FArchive::UEVer(), so a newer engine can still read older data:
 * `if (Ar.UEVer() >= VER_LEON_SOME_CHANGE) { Ar << NewMember; }`. Add one value before
 * VER_LEON_AUTOMATIC_VERSION_PLUS_ONE for each format change; never reorder or remove a value.
 */
enum ELeonPackageVersion : int32
{
	/** 0.15.0 (P11): the first package layout (Docs/ASSET_FORMATS.md). */
	VER_LEON_INITIAL_PACKAGE_FORMAT = 1,

	/**
	 * ps2-shipping N4: the static and skeletal mesh vertices lost their tangent (FVertex is 32 bytes, FSkeletalVertex
	 * 64). Older packages are not loadable: the content was saved again.
	 */
	VER_LEON_REMOVE_VERTEX_TANGENT = 2,

	/**
	 * ps2-shipping N12: a static mesh's render data is LPS2 v2 (quantized triangle strips in VU1's batches) and its
	 * collision triangles are saved beside it. Older packages are not loadable: the content was saved again.
	 */
	VER_LEON_LPS2_MESH = 3,

	/**
	 * ps2-shipping N21: glTF skeletal import. A skeleton keeps its reference pose, a skeletal mesh's render data is a
	 * skinned LPS2 v2 blob (two bones a vertex, palettes of 24 bones) with its bones' bounds, and an animation's keys
	 * are compressed local-space tracks (48-bit rotations, int16 translations). Older packages are not loadable: the
	 * content was saved again (it had no skeletal assets).
	 */
	VER_LEON_SKELETAL_LPS2_ANIM_TRACKS = 4,

	/**
	 * ps2-shipping N22: a static mesh component saves its baked vertex colours (the lighting LeonEd bakes into a map's
	 * static meshes, FLPS2ColorStreams) after its relative transform. Older packages are not loadable: the content was
	 * saved again.
	 */
	VER_LEON_BAKED_VERTEX_COLORS = 5,

	/**
	 * ps2-shipping N30f: a static mesh's collision triangles carry each one's material slot (FTriMeshCollisionData::
	 * MaterialIndices), so a hit on a triangle has its material's physical material. Older packages are not loadable:
	 * the content was saved again.
	 */
	VER_LEON_COLLISION_MATERIAL_INDICES = 6,

	// New versions go here.

	VER_LEON_AUTOMATIC_VERSION_PLUS_ONE,
	/** The newest version (UE: VER_UE4_AUTOMATIC_VERSION). */
	VER_LEON_AUTOMATIC_VERSION = VER_LEON_AUTOMATIC_VERSION_PLUS_ONE - 1,

	/** The version new packages are saved with (UE: VER_LATEST_ENGINE_UE4). */
	VER_LEON_LATEST = VER_LEON_AUTOMATIC_VERSION,
	/** Packages older than this are rejected by the loader (UE: VER_UE4_OLDEST_LOADABLE_PACKAGE). */
	VER_LEON_OLDEST_LOADABLE_PACKAGE = VER_LEON_COLLISION_MATERIAL_INDICES,
};

/** The licensee version Leon saves: always 0 (UE: VER_LATEST_ENGINE_LICENSEEUE4). */
#define VER_LEON_LATEST_LICENSEE 0
