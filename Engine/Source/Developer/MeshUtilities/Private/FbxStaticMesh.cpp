#include "FbxStaticMesh.h"

#include "Containers/StringConv.h"
#include "MeshData.h"
#include "MeshUtilitiesLog.h"

#include <ufbx.h>

namespace
{

	ufbx_load_opts MakeLoadOpts()
	{
		ufbx_load_opts Opts{};
		Opts.target_axes = ufbx_axes_right_handed_y_up;
		Opts.target_unit_meters = 1.0f;
		Opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
		Opts.generate_missing_normals = true;
		return Opts;
	}

} // namespace

bool LoadStaticMeshFromFbx(const FString& Path, FMeshData& Out)
{
	Out = FMeshData();
	ufbx_error Error{};
	const ufbx_load_opts Opts = MakeLoadOpts();
	ufbx_scene* Scene = ufbx_load_file(TCHAR_TO_UTF8(*Path), &Opts, &Error);
	if (Scene == nullptr)
	{
		UE_LOG(LogMeshUtilities, Error, "FbxStaticMesh: failed to load '%s': %s", *Path, Error.description.data);
		return false;
	}

	TArray<uint32> Tri;
	Tri.SetNumZeroed(16 * 3);

	for (size_t Mi = 0; Mi < Scene->meshes.count; ++Mi)
	{
		ufbx_mesh* Mesh = Scene->meshes.data[Mi];
		if (Mesh == nullptr || Mesh->num_faces == 0)
		{
			continue;
		}

		const int32 IndexOffset = Out.Indices.Num();
		int32 IndexCount = 0;

		if (Mesh->max_face_triangles * 3u > static_cast<size_t>(Tri.Num()))
		{
			Tri.SetNumZeroed(static_cast<int32>(Mesh->max_face_triangles * 3u));
		}

		for (size_t Fi = 0; Fi < Mesh->faces.count; ++Fi)
		{
			const ufbx_face Face = Mesh->faces.data[Fi];
			if (Face.num_indices < 3)
			{
				continue;
			}
			const uint32 NumTris = ufbx_triangulate_face(Tri.GetData(), static_cast<size_t>(Tri.Num()), Mesh, Face);
			for (uint32 T = 0; T < NumTris; ++T)
			{
				for (int32 K = 0; K < 3; ++K)
				{
					const uint32 Corner = Tri[static_cast<int32>(T * 3u) + K];

					FVertex V{};
					const ufbx_vec3 Pos = ufbx_get_vertex_vec3(&Mesh->vertex_position, Corner);
					V.Position =
						FVector(static_cast<float>(Pos.x), static_cast<float>(Pos.y), static_cast<float>(Pos.z));

					if (Mesh->vertex_normal.exists)
					{
						const ufbx_vec3 N = ufbx_get_vertex_vec3(&Mesh->vertex_normal, Corner);
						V.Normal = FVector(static_cast<float>(N.x), static_cast<float>(N.y), static_cast<float>(N.z))
									   .GetUnsafeNormal();
					}
					else
					{
						V.Normal = FVector(0.0f, 1.0f, 0.0f);
					}

					if (Mesh->vertex_uv.exists)
					{
						const ufbx_vec2 Uv = ufbx_get_vertex_vec2(&Mesh->vertex_uv, Corner);
						V.TexCoord = FVector2D(static_cast<float>(Uv.x), static_cast<float>(Uv.y));
					}

					Out.Indices.Add(static_cast<uint32>(Out.Vertices.Num()));
					Out.Vertices.Add(V);
					++IndexCount;
				}
			}
		}

		if (IndexCount > 0)
		{
			Out.Submeshes.Add(FMeshSection{IndexOffset, IndexCount, Out.Submeshes.Num()});
		}
	}

	ufbx_free_scene(Scene);

	if (Out.IsEmpty())
	{
		UE_LOG(LogMeshUtilities, Error, "FbxStaticMesh: no triangles in '%s'", *Path);
		return false;
	}

	ComputeTangents(Out, EMeshDataBasis::LegacyYUp);
	return true;
}
