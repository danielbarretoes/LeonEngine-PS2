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
#include "LeonMaterialFormat.h"
#include "MeshData.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace
{

	[[nodiscard]] std::uint32_t ReadIndex(const cgltf_accessor* Acc, cgltf_size I)
	{
		cgltf_uint Out = 0;
		cgltf_accessor_read_uint(Acc, I, &Out, 1);
		return static_cast<std::uint32_t>(Out);
	}

	[[nodiscard]] std::string SanitizeName(std::string InName)
	{
		if (InName.empty())
		{
			InName = "Material";
		}
		for (char& C : InName)
		{
			if (!(std::isalnum(static_cast<unsigned char>(C)) || C == '_' || C == '-'))
			{
				C = '_';
			}
		}
		return InName;
	}

	bool CopyTextureUri(
		const cgltf_image* Image, const fs::path& GltfDir, const fs::path& TexturesDir, std::string& OutRelPath)
	{
		OutRelPath.clear();
		if (Image == nullptr)
		{
			return false;
		}
		std::error_code Ec;
		fs::create_directories(TexturesDir, Ec);
		if (Image->uri != nullptr && std::strlen(Image->uri) > 0 && std::strncmp(Image->uri, "data:", 5) != 0)
		{
			const fs::path Src = GltfDir / Image->uri;
			const fs::path Dst = TexturesDir / fs::path(Image->uri).filename();
			fs::copy_file(Src, Dst, fs::copy_options::overwrite_existing, Ec);
			if (!Ec)
			{
				OutRelPath = (fs::path("textures") / Dst.filename()).generic_string();
				return true;
			}
		}
		return false;
	}

	void WriteMaterialFromGltf(const cgltf_material* Mat, const std::string& InName, const fs::path& MaterialsDir,
		const fs::path& GltfDir, FGltfImportedMaterial& OutDesc)
	{
		FMaterial M{};
		M.Albedo = {0.8f, 0.8f, 0.8f};
		M.Metallic = 0.0f;
		M.Roughness = 0.5f;
		std::string BaseMap;
		std::string NormalMap;

		if (Mat != nullptr && Mat->has_pbr_metallic_roughness)
		{
			const auto& Pbr = Mat->pbr_metallic_roughness;
			M.Albedo = {Pbr.base_color_factor[0], Pbr.base_color_factor[1], Pbr.base_color_factor[2]};
			M.Alpha = Pbr.base_color_factor[3];
			M.Metallic = Pbr.metallic_factor;
			M.Roughness = std::clamp(Pbr.roughness_factor, 0.04f, 1.0f);
			if (Pbr.base_color_texture.texture != nullptr)
			{
				(void)CopyTextureUri(
					Pbr.base_color_texture.texture->image, GltfDir, MaterialsDir / "Textures", BaseMap);
			}
		}
		if (Mat != nullptr && Mat->normal_texture.texture != nullptr)
		{
			(void)CopyTextureUri(Mat->normal_texture.texture->image, GltfDir, MaterialsDir / "Textures", NormalMap);
		}

		const std::string FileName = "M_" + SanitizeName(InName) + ".lmat";
		const fs::path OutPath = MaterialsDir / FileName;
		std::error_code Ec;
		fs::create_directories(MaterialsDir, Ec);
		(void)SaveLeonMaterialFile(OutPath.generic_string(), "M_" + SanitizeName(InName), M, BaseMap, NormalMap);
		OutDesc.Name = "M_" + SanitizeName(InName);
		OutDesc.LmatRelativePath = (fs::path("materials") / FileName).generic_string();
	}

} // namespace

bool LoadStaticMeshFromGltf(const std::string& Path, FMeshData& Out, const std::string& MaterialsOutDir,
	std::vector<FGltfImportedMaterial>* OutMaterials, std::string& OutError)
{
	Out = {};
	OutError.clear();

	cgltf_options Options{};
	cgltf_data* Data = nullptr;
	cgltf_result Result = cgltf_parse_file(&Options, Path.c_str(), &Data);
	if (Result != cgltf_result_success)
	{
		OutError = "cgltf_parse_file failed for " + Path;
		return false;
	}
	Result = cgltf_load_buffers(&Options, Data, Path.c_str());
	if (Result != cgltf_result_success)
	{
		OutError = "cgltf_load_buffers failed for " + Path;
		cgltf_free(Data);
		return false;
	}

	const fs::path GltfDir = fs::path(Path).parent_path();
	const fs::path MaterialsDir = MaterialsOutDir.empty() ? fs::path{} : fs::path(MaterialsOutDir);

	FMeshData Mesh;
	std::uint32_t BaseVertex = 0;

	for (cgltf_size Mi = 0; Mi < Data->meshes_count; ++Mi)
	{
		const cgltf_mesh& Gmesh = Data->meshes[Mi];
		for (cgltf_size Pi = 0; Pi < Gmesh.primitives_count; ++Pi)
		{
			const cgltf_primitive& Prim = Gmesh.primitives[Pi];
			if (Prim.type != cgltf_primitive_type_triangles)
			{
				continue;
			}

			const cgltf_accessor* Pos = nullptr;
			const cgltf_accessor* Nrm = nullptr;
			const cgltf_accessor* Uv = nullptr;
			for (cgltf_size Ai = 0; Ai < Prim.attributes_count; ++Ai)
			{
				const cgltf_attribute& Attr = Prim.attributes[Ai];
				if (Attr.type == cgltf_attribute_type_position)
				{
					Pos = Attr.data;
				}
				else if (Attr.type == cgltf_attribute_type_normal)
				{
					Nrm = Attr.data;
				}
				else if (Attr.type == cgltf_attribute_type_texcoord && Attr.index == 0)
				{
					Uv = Attr.data;
				}
			}
			if (Pos == nullptr)
			{
				continue;
			}

			const cgltf_size Vcount = Pos->count;
			const int32 StartIndex = Mesh.Indices.Num();
			for (cgltf_size Vi = 0; Vi < Vcount; ++Vi)
			{
				FVertex V{};
				float Tmp[4]{};
				if (cgltf_accessor_read_float(Pos, Vi, Tmp, 3))
				{
					V.Position = {Tmp[0], Tmp[1], Tmp[2]};
				}
				if (Nrm != nullptr && cgltf_accessor_read_float(Nrm, Vi, Tmp, 3))
				{
					V.Normal = {Tmp[0], Tmp[1], Tmp[2]};
				}
				else
				{
					V.Normal = {0.0f, 1.0f, 0.0f};
				}
				if (Uv != nullptr && cgltf_accessor_read_float(Uv, Vi, Tmp, 2))
				{
					V.TexCoord = {Tmp[0], Tmp[1]};
				}
				Mesh.Vertices.Add(V);
			}

			if (Prim.indices != nullptr)
			{
				for (cgltf_size Ii = 0; Ii < Prim.indices->count; ++Ii)
				{
					Mesh.Indices.Add(BaseVertex + ReadIndex(Prim.indices, Ii));
				}
			}
			else
			{
				for (cgltf_size Ii = 0; Ii < Vcount; ++Ii)
				{
					Mesh.Indices.Add(BaseVertex + static_cast<uint32>(Ii));
				}
			}

			FMeshSection Sm;
			Sm.IndexOffset = StartIndex;
			Sm.IndexCount = Mesh.Indices.Num() - StartIndex;
			Sm.MaterialIndex = Mesh.Materials.Num();
			Mesh.Submeshes.Add(Sm);

			FMaterial Slot{};
			Mesh.Materials.Add(Slot);
			Mesh.AlbedoMapPaths.AddDefaulted();

			if (!MaterialsDir.empty() && OutMaterials != nullptr && Prim.material != nullptr)
			{
				FGltfImportedMaterial Desc;
				const char* MatName = Prim.material->name != nullptr ? Prim.material->name : "Material";
				WriteMaterialFromGltf(Prim.material, MatName, MaterialsDir, GltfDir, Desc);
				OutMaterials->push_back(Desc);
			}

			BaseVertex += static_cast<std::uint32_t>(Vcount);
		}
	}

	cgltf_free(Data);

	if (Mesh.IsEmpty())
	{
		OutError = "glTF contained no triangle mesh data";
		return false;
	}
	ComputeTangents(Mesh);
	Out = std::move(Mesh);
	return true;
}
