#define CGLTF_IMPLEMENTATION
#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable : 4996) // cgltf uses fopen/strncpy/strcpy
#endif
#include <cgltf.h>
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

#include "Containers/StringConv.h"
#include "GltfImport.h"
#include "ImportCoordinateConversion.h"
#include "MeshData.h"
#include "Misc/CString.h"
#include "Misc/Paths.h"

namespace
{

	[[nodiscard]] uint32 ReadIndex(const cgltf_accessor* Acc, cgltf_size I)
	{
		cgltf_uint Out = 0;
		cgltf_accessor_read_uint(Acc, I, &Out, 1);
		return static_cast<uint32>(Out);
	}

	/** The absolute path of an external image (a file URI next to the glTF), or empty (none, or embedded). */
	[[nodiscard]] FString GetImagePath(const cgltf_texture_view& View, const FString& GltfDir)
	{
		const cgltf_image* Image = View.texture != nullptr ? View.texture->image : nullptr;
		if (Image == nullptr || Image->uri == nullptr || FCStringAnsi::Strlen(Image->uri) == 0 ||
			FCStringAnsi::Strncmp(Image->uri, "data:", 5) == 0)
		{
			return FString();
		}
		FString ImagePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(GltfDir, FString(Image->uri)));
		FPaths::NormalizeFilename(ImagePath);
		return ImagePath;
	}

	/** A slot's material from a glTF material (metallic-roughness PBR; null: glTF's default material). */
	void AddMaterialSlot(const cgltf_material* Mat, const FString& GltfDir, FMeshData& Mesh)
	{
		FMaterial M{};
		M.Albedo = FVector(0.8f, 0.8f, 0.8f);
		M.Metallic = 0.0f;
		M.Roughness = 0.5f;
		FString BaseMap;
		FString NormalMap;
		if (Mat != nullptr && Mat->has_pbr_metallic_roughness)
		{
			const cgltf_pbr_metallic_roughness& Pbr = Mat->pbr_metallic_roughness;
			M.Albedo = FVector(Pbr.base_color_factor[0], Pbr.base_color_factor[1], Pbr.base_color_factor[2]);
			M.Alpha = Pbr.base_color_factor[3];
			M.Metallic = Pbr.metallic_factor;
			M.Roughness = FMath::Clamp(Pbr.roughness_factor, 0.04f, 1.0f);
			BaseMap = GetImagePath(Pbr.base_color_texture, GltfDir);
		}
		if (Mat != nullptr)
		{
			NormalMap = GetImagePath(Mat->normal_texture, GltfDir);
		}
		Mesh.Materials.Add(M);
		Mesh.MaterialSlotNames.Add(Mat != nullptr ? FString(Mat->name != nullptr ? Mat->name : "Material") : FString());
		Mesh.AlbedoMapPaths.Add(BaseMap);
		Mesh.NormalMapPaths.Add(NormalMap);
	}

} // namespace

bool LoadStaticMeshFromGltf(const FString& Path, FMeshData& Out, FString& OutError)
{
	Out = FMeshData();
	OutError.Empty();

	cgltf_options Options{};
	cgltf_data* Data = nullptr;
	cgltf_result Result = cgltf_parse_file(&Options, TCHAR_TO_UTF8(*Path), &Data);
	if (Result != cgltf_result_success)
	{
		OutError = "cgltf_parse_file failed for " + Path;
		return false;
	}
	Result = cgltf_load_buffers(&Options, Data, TCHAR_TO_UTF8(*Path));
	if (Result != cgltf_result_success)
	{
		OutError = "cgltf_load_buffers failed for " + Path;
		cgltf_free(Data);
		return false;
	}

	const FString GltfDir = FPaths::GetPath(Path);

	FMeshData Mesh;
	uint32 BaseVertex = 0;

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
					V.Position = FVector(Tmp[0], Tmp[1], Tmp[2]);
				}
				if (Nrm != nullptr && cgltf_accessor_read_float(Nrm, Vi, Tmp, 3))
				{
					V.Normal = FVector(Tmp[0], Tmp[1], Tmp[2]);
				}
				else
				{
					V.Normal = FVector(0.0f, 1.0f, 0.0f);
				}
				if (Uv != nullptr && cgltf_accessor_read_float(Uv, Vi, Tmp, 2))
				{
					V.TexCoord = FVector2D(Tmp[0], Tmp[1]);
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

			AddMaterialSlot(Prim.material, GltfDir, Mesh);

			BaseVertex += static_cast<uint32>(Vcount);
		}
	}

	cgltf_free(Data);

	if (Mesh.IsEmpty())
	{
		OutError = "glTF contained no triangle mesh data";
		return false;
	}
	// glTF 2.0 is right-handed, Y up, in metres.
	FImportCoordinateConversion(EImportAxes::RightHandedYUp, FImportCoordinateConversion::CmPerMetre)
		.ConvertMeshData(Mesh);
	ComputeTangents(Mesh, EMeshDataBasis::Engine);
	Out = MoveTemp(Mesh);
	return true;
}
