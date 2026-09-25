#pragma once

#include "CoreMinimal.h"
#include "ImportCoordinateConversion.h"

#include <ufbx.h>

/**
 * What the FBX importers share. ufbx resolves each file's declared axis system to right-handed Z up (front -Y), the
 * frame UE's FBX importer converts a scene to (FbxAxisSystem::ConvertScene); FImportCoordinateConversion then takes
 * that frame and the file's unit to the engine world, as FFbxDataConverter does. ufbx does not scale: the unit is the
 * conversion's alone.
 */
struct FFbxImportCommon
{
	/** ufbx options: the axes resolved to right-handed Z up, missing normals generated. */
	[[nodiscard]] static ufbx_load_opts MakeLoadOptions();

	/**
	 * The conversion of a loaded scene: RightHandedZUp with UnitsToCm = unit_meters * 100. ufbx leaves a file that
	 * declares no axes as it is; it is taken as FBX's default, right-handed Y up.
	 */
	[[nodiscard]] static FImportCoordinateConversion MakeCoordinateConversion(const ufbx_scene& Scene);

	/**
	 * ufbx resolves the axes in the root nodes' transforms only; mesh-space data (vertex positions and normals) stays
	 * in the file's axes. This is that root rotation as an FMatrix (row vectors) for mesh-space data: an axis
	 * permutation, rounded to exact -1 / 0 / 1 entries.
	 */
	[[nodiscard]] static FMatrix GetRootAxesMatrix(const ufbx_scene& Scene);
};
