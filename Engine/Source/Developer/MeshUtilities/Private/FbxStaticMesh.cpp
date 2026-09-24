#include "FbxStaticMesh.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <iostream>
#include "MeshData.h"
#include <ufbx.h>
#include <vector>

namespace leon {
namespace {

ufbx_load_opts MakeLoadOpts() {
    ufbx_load_opts opts{};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;
    opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    opts.generate_missing_normals = true;
    return opts;
}

} // namespace

bool LoadStaticMeshFromFbx(const std::string& path, MeshData& out) {
    out = {};
    ufbx_error error{};
    const ufbx_load_opts opts = MakeLoadOpts();
    ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);
    if (scene == nullptr) {
        std::cerr << "FbxStaticMesh: failed to load '" << path << "': " << error.description.data
                  << '\n';
        return false;
    }

    const size_t triIndexCapacity = static_cast<size_t>(16u) * 3u;
    std::vector<uint32_t> tri(triIndexCapacity);

    for (size_t mi = 0; mi < scene->meshes.count; ++mi) {
        ufbx_mesh* mesh = scene->meshes.data[mi];
        if (mesh == nullptr || mesh->num_faces == 0) {
            continue;
        }

        const int indexOffset = static_cast<int>(out.indices.size());
        int indexCount = 0;

        if (mesh->max_face_triangles * 3u > tri.size()) {
            tri.resize(mesh->max_face_triangles * 3u);
        }

        for (size_t fi = 0; fi < mesh->faces.count; ++fi) {
            const ufbx_face face = mesh->faces.data[fi];
            if (face.num_indices < 3) {
                continue;
            }
            const uint32_t numTris = ufbx_triangulate_face(tri.data(), tri.size(), mesh, face);
            for (uint32_t t = 0; t < numTris; ++t) {
                for (int k = 0; k < 3; ++k) {
                    const uint32_t corner =
                        tri[(static_cast<size_t>(t) * 3u) + static_cast<size_t>(k)];

                    Vertex v{};
                    const ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, corner);
                    v.position = {static_cast<float>(pos.x), static_cast<float>(pos.y),
                                  static_cast<float>(pos.z)};

                    if (mesh->vertex_normal.exists) {
                        const ufbx_vec3 n = ufbx_get_vertex_vec3(&mesh->vertex_normal, corner);
                        v.normal = glm::normalize(glm::vec3{
                            static_cast<float>(n.x), static_cast<float>(n.y),
                            static_cast<float>(n.z)});
                    } else {
                        v.normal = {0.0f, 1.0f, 0.0f};
                    }

                    if (mesh->vertex_uv.exists) {
                        const ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, corner);
                        v.texCoord = {static_cast<float>(uv.x), static_cast<float>(uv.y)};
                    }

                    out.indices.push_back(static_cast<std::uint32_t>(out.vertices.size()));
                    out.vertices.push_back(v);
                    ++indexCount;
                }
            }
        }

        if (indexCount > 0) {
            out.submeshes.push_back(
                SubMesh{indexOffset, indexCount, static_cast<int>(out.submeshes.size())});
        }
    }

    ufbx_free_scene(scene);

    if (out.empty()) {
        std::cerr << "FbxStaticMesh: no triangles in '" << path << "'\n";
        return false;
    }

    ComputeTangents(out);
    return true;
}

} // namespace leon
