#include "FbxImportCommon.h"

ufbx_load_opts FFbxImportCommon::MakeLoadOptions()
{
	ufbx_load_opts Opts{};
	Opts.target_axes = ufbx_axes_right_handed_z_up;
	Opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
	Opts.generate_missing_normals = true;
	return Opts;
}

FImportCoordinateConversion FFbxImportCommon::MakeCoordinateConversion(const ufbx_scene& Scene)
{
	// FBX files default to centimetres (unit_meters 0.01).
	const double UnitMetres = Scene.settings.unit_meters > 0.0 ? Scene.settings.unit_meters : 0.01;
	const EImportAxes Axes =
		ufbx_coordinate_axes_valid(Scene.settings.axes) ? EImportAxes::RightHandedZUp : EImportAxes::RightHandedYUp;
	return FImportCoordinateConversion(
		Axes, static_cast<float>(UnitMetres * static_cast<double>(FImportCoordinateConversion::CmPerMetre)));
}

FMatrix FFbxImportCommon::GetRootAxesMatrix(const ufbx_scene& Scene)
{
	FMatrix Out = FMatrix::Identity;
	for (int32 Row = 0; Row < 3; ++Row)
	{
		ufbx_vec3 Axis{};
		Axis.v[Row] = 1.0;
		const ufbx_vec3 Image = ufbx_quat_rotate_vec3(Scene.metadata.root_rotation, Axis);
		for (int32 Column = 0; Column < 3; ++Column)
		{
			Out.M[Row][Column] = FMath::RoundToFloat(static_cast<float>(Image.v[Column]));
		}
	}
	return Out;
}
