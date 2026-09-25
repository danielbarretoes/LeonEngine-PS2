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

	// New versions go here.

	VER_LEON_AUTOMATIC_VERSION_PLUS_ONE,
	/** The newest version (UE: VER_UE4_AUTOMATIC_VERSION). */
	VER_LEON_AUTOMATIC_VERSION = VER_LEON_AUTOMATIC_VERSION_PLUS_ONE - 1,

	/** The version new packages are saved with (UE: VER_LATEST_ENGINE_UE4). */
	VER_LEON_LATEST = VER_LEON_AUTOMATIC_VERSION,
	/** Packages older than this are rejected by the loader (UE: VER_UE4_OLDEST_LOADABLE_PACKAGE). */
	VER_LEON_OLDEST_LOADABLE_PACKAGE = VER_LEON_INITIAL_PACKAGE_FORMAT,
};

/** The licensee version Leon saves: always 0 (UE: VER_LATEST_ENGINE_LICENSEEUE4). */
#define VER_LEON_LATEST_LICENSEE 0
