#define CGLTF_IMPLEMENTATION
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996) // cgltf uses fopen/strncpy/strcpy
#endif
#include <cgltf.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "GltfImport.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include "LeonMaterialFormat.h"
#include "MeshData.h"
#include <system_error>

namespace fs = std::filesystem;

namespace {

[[nodiscard]] std::uint32_t ReadIndex(const cgltf_accessor* acc, cgltf_size i) {
    cgltf_uint out = 0;
    cgltf_accessor_read_uint(acc, i, &out, 1);
    return static_cast<std::uint32_t>(out);
}

[[nodiscard]] std::string SanitizeName(std::string name) {
    if (name.empty()) {
        name = "Material";
    }
    for (char& c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) {
            c = '_';
        }
    }
    return name;
}

bool CopyTextureUri(const cgltf_image* image, const fs::path& gltfDir, const fs::path& texturesDir,
                    std::string& outRelPath) {
    outRelPath.clear();
    if (image == nullptr) {
        return false;
    }
    std::error_code ec;
    fs::create_directories(texturesDir, ec);
    if (image->uri != nullptr && std::strlen(image->uri) > 0 &&
        std::strncmp(image->uri, "data:", 5) != 0) {
        const fs::path src = gltfDir / image->uri;
        const fs::path dst = texturesDir / fs::path(image->uri).filename();
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
            outRelPath = (fs::path("textures") / dst.filename()).generic_string();
            return true;
        }
    }
    return false;
}

void WriteMaterialFromGltf(const cgltf_material* mat, const std::string& name,
                           const fs::path& materialsDir, const fs::path& gltfDir,
                           GltfImportedMaterial& outDesc) {
    Material m{};
    m.albedo = {0.8f, 0.8f, 0.8f};
    m.metallic = 0.0f;
    m.roughness = 0.5f;
    std::string baseMap;
    std::string normalMap;

    if (mat != nullptr && mat->has_pbr_metallic_roughness) {
        const auto& pbr = mat->pbr_metallic_roughness;
        m.albedo = {pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]};
        m.alpha = pbr.base_color_factor[3];
        m.metallic = pbr.metallic_factor;
        m.roughness = std::clamp(pbr.roughness_factor, 0.04f, 1.0f);
        if (pbr.base_color_texture.texture != nullptr) {
            (void)CopyTextureUri(pbr.base_color_texture.texture->image, gltfDir,
                                 materialsDir / "Textures", baseMap);
        }
    }
    if (mat != nullptr && mat->normal_texture.texture != nullptr) {
        (void)CopyTextureUri(mat->normal_texture.texture->image, gltfDir, materialsDir / "Textures",
                             normalMap);
    }

    const std::string fileName = "M_" + SanitizeName(name) + ".lmat";
    const fs::path outPath = materialsDir / fileName;
    std::error_code ec;
    fs::create_directories(materialsDir, ec);
    (void)SaveLeonMaterialFile(outPath.generic_string(), "M_" + SanitizeName(name), m, baseMap,
                               normalMap);
    outDesc.name = "M_" + SanitizeName(name);
    outDesc.lmatRelativePath = (fs::path("materials") / fileName).generic_string();
}

} // namespace

bool LoadStaticMeshFromGltf(const std::string& path, MeshData& out,
                            const std::string& materialsOutDir,
                            std::vector<GltfImportedMaterial>* outMaterials, std::string& outError) {
    out = {};
    outError.clear();

    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        outError = "cgltf_parse_file failed for " + path;
        return false;
    }
    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        outError = "cgltf_load_buffers failed for " + path;
        cgltf_free(data);
        return false;
    }

    const fs::path gltfDir = fs::path(path).parent_path();
    const fs::path materialsDir =
        materialsOutDir.empty() ? fs::path{} : fs::path(materialsOutDir);

    MeshData mesh;
    std::uint32_t baseVertex = 0;

    for (cgltf_size mi = 0; mi < data->meshes_count; ++mi) {
        const cgltf_mesh& gmesh = data->meshes[mi];
        for (cgltf_size pi = 0; pi < gmesh.primitives_count; ++pi) {
            const cgltf_primitive& prim = gmesh.primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles) {
                continue;
            }

            const cgltf_accessor* pos = nullptr;
            const cgltf_accessor* nrm = nullptr;
            const cgltf_accessor* uv = nullptr;
            for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
                const cgltf_attribute& attr = prim.attributes[ai];
                if (attr.type == cgltf_attribute_type_position) {
                    pos = attr.data;
                } else if (attr.type == cgltf_attribute_type_normal) {
                    nrm = attr.data;
                } else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) {
                    uv = attr.data;
                }
            }
            if (pos == nullptr) {
                continue;
            }

            const cgltf_size vcount = pos->count;
            const std::size_t startIndex = mesh.indices.size();
            for (cgltf_size vi = 0; vi < vcount; ++vi) {
                Vertex v{};
                float tmp[4]{};
                if (cgltf_accessor_read_float(pos, vi, tmp, 3)) {
                    v.position = {tmp[0], tmp[1], tmp[2]};
                }
                if (nrm != nullptr && cgltf_accessor_read_float(nrm, vi, tmp, 3)) {
                    v.normal = {tmp[0], tmp[1], tmp[2]};
                } else {
                    v.normal = {0.0f, 1.0f, 0.0f};
                }
                if (uv != nullptr && cgltf_accessor_read_float(uv, vi, tmp, 2)) {
                    v.texCoord = {tmp[0], tmp[1]};
                }
                mesh.vertices.push_back(v);
            }

            if (prim.indices != nullptr) {
                for (cgltf_size ii = 0; ii < prim.indices->count; ++ii) {
                    mesh.indices.push_back(baseVertex + ReadIndex(prim.indices, ii));
                }
            } else {
                for (cgltf_size ii = 0; ii < vcount; ++ii) {
                    mesh.indices.push_back(baseVertex + static_cast<std::uint32_t>(ii));
                }
            }

            SubMesh sm;
            sm.indexOffset = static_cast<int>(startIndex);
            sm.indexCount = static_cast<int>(mesh.indices.size() - startIndex);
            sm.materialIndex = static_cast<int>(mesh.materials.size());
            mesh.submeshes.push_back(sm);

            Material slot{};
            mesh.materials.push_back(slot);
            mesh.albedoMapPaths.emplace_back();

            if (!materialsDir.empty() && outMaterials != nullptr && prim.material != nullptr) {
                GltfImportedMaterial desc;
                const char* matName =
                    prim.material->name != nullptr ? prim.material->name : "Material";
                WriteMaterialFromGltf(prim.material, matName, materialsDir, gltfDir, desc);
                outMaterials->push_back(desc);
            }

            baseVertex += static_cast<std::uint32_t>(vcount);
        }
    }

    cgltf_free(data);

    if (mesh.empty()) {
        outError = "glTF contained no triangle mesh data";
        return false;
    }
    ComputeTangents(mesh);
    out = std::move(mesh);
    return true;
}

