#include "FbxStaticMesh.h"

#include "MeshData.h"
#include "Migration/GlmInterop.h"

#include <glm/geometric.hpp>
#include <ufbx.h>

#include <algorithm>
#include <iostream>
#include <vector>

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

bool LoadStaticMeshFromFbx(const std::string& Path, FMeshData& Out)
{
	Out = {};
	ufbx_error Error{};
	const ufbx_load_opts Opts = MakeLoadOpts();
	ufbx_scene* Scene = ufbx_load_file(Path.c_str(), &Opts, &Error);
	if (Scene == nullptr)
	{
		std::cerr << "FbxStaticMesh: failed to load '" << Path << "': " << Error.description.data << '\n';
		return false;
	}

	const size_t TriIndexCapacity = static_cast<size_t>(16u) * 3u;
	std::vector<uint32_t> Tri(TriIndexCapacity);

	for (size_t Mi = 0; Mi < Scene->meshes.count; ++Mi)
	{
		ufbx_mesh* Mesh = Scene->meshes.data[Mi];
		if (Mesh == nullptr || Mesh->num_faces == 0)
		{
			continue;
		}

		const int IndexOffset = Out.Indices.Num();
		int IndexCount = 0;

		if (Mesh->max_face_triangles * 3u > Tri.size())
		{
			Tri.resize(Mesh->max_face_triangles * 3u);
		}

		for (size_t Fi = 0; Fi < Mesh->faces.count; ++Fi)
		{
			const ufbx_face Face = Mesh->faces.data[Fi];
			if (Face.num_indices < 3)
			{
				continue;
			}
			const uint32_t NumTris = ufbx_triangulate_face(Tri.data(), Tri.size(), Mesh, Face);
			for (uint32_t T = 0; T < NumTris; ++T)
			{
				for (int K = 0; K < 3; ++K)
				{
					const uint32_t Corner = Tri[(static_cast<size_t>(T) * 3u) + static_cast<size_t>(K)];

					FVertex V{};
					const ufbx_vec3 Pos = ufbx_get_vertex_vec3(&Mesh->vertex_position, Corner);
					V.Position = {static_cast<float>(Pos.x), static_cast<float>(Pos.y), static_cast<float>(Pos.z)};

					if (Mesh->vertex_normal.exists)
					{
						const ufbx_vec3 N = ufbx_get_vertex_vec3(&Mesh->vertex_normal, Corner);
						V.Normal = FromGlm(glm::normalize(
							glm::vec3{static_cast<float>(N.x), static_cast<float>(N.y), static_cast<float>(N.z)}));
					}
					else
					{
						V.Normal = {0.0f, 1.0f, 0.0f};
					}

					if (Mesh->vertex_uv.exists)
					{
						const ufbx_vec2 Uv = ufbx_get_vertex_vec2(&Mesh->vertex_uv, Corner);
						V.TexCoord = {static_cast<float>(Uv.x), static_cast<float>(Uv.y)};
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
		std::cerr << "FbxStaticMesh: no triangles in '" << Path << "'\n";
		return false;
	}

	ComputeTangents(Out);
	return true;
}
