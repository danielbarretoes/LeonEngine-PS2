#include "ObjImport.h"

#include "Containers/StringConv.h"
#include "MeshUtilitiesLog.h"
#include "Misc/Paths.h"

#include <tiny_obj_loader.h>

namespace
{

	/** A unique OBJ corner: position / normal / texcoord indices. */
	struct FVertexKey
	{
		int32 PositionIndex = 0;
		int32 NormalIndex = 0;
		int32 TexcoordIndex = 0;

		[[nodiscard]] bool operator==(const FVertexKey& Other) const
		{
			return PositionIndex == Other.PositionIndex && NormalIndex == Other.NormalIndex &&
				TexcoordIndex == Other.TexcoordIndex;
		}

		friend uint32 GetTypeHash(const FVertexKey& Key)
		{
			return HashCombine(HashCombine(GetTypeHash(Key.PositionIndex), GetTypeHash(Key.NormalIndex)),
				GetTypeHash(Key.TexcoordIndex));
		}
	};

	void ComputeSmoothNormals(FMeshData& Data)
	{
		for (FVertex& Vertex : Data.Vertices)
		{
			Vertex.Normal = FVector::ZeroVector;
		}

		for (int32 I = 0; I + 2 < Data.Indices.Num(); I += 3)
		{
			const int32 I0 = static_cast<int32>(Data.Indices[I + 0]);
			const int32 I1 = static_cast<int32>(Data.Indices[I + 1]);
			const int32 I2 = static_cast<int32>(Data.Indices[I + 2]);

			const FVector Edge1 = Data.Vertices[I1].Position - Data.Vertices[I0].Position;
			const FVector Edge2 = Data.Vertices[I2].Position - Data.Vertices[I0].Position;
			const FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2);
			if (FVector::DotProduct(FaceNormal, FaceNormal) < 1e-20f)
			{
				continue;
			}

			const FVector N = FaceNormal.GetUnsafeNormal();
			Data.Vertices[I0].Normal += N;
			Data.Vertices[I1].Normal += N;
			Data.Vertices[I2].Normal += N;
		}

		for (FVertex& Vertex : Data.Vertices)
		{
			if (FVector::DotProduct(Vertex.Normal, Vertex.Normal) > 0.0f)
			{
				Vertex.Normal = Vertex.Normal.GetUnsafeNormal();
			}
			else
			{
				Vertex.Normal = FVector(0.0f, 1.0f, 0.0f);
			}
		}
	}

	bool IndexInRange(int32 Index, size_t Count, int32 Components)
	{
		if (Index < 0)
		{
			return false;
		}
		const size_t Needed = static_cast<size_t>(Index + 1) * static_cast<size_t>(Components);
		return Needed <= Count;
	}

	bool FaceIndicesValid(
		const tinyobj::attrib_t& Attrib, const tinyobj::index_t& Index, bool bHasFileNormals, bool bHasTexcoords)
	{
		if (!IndexInRange(Index.vertex_index, Attrib.vertices.size(), 3))
		{
			return false;
		}
		if (bHasFileNormals && Index.normal_index >= 0 && !IndexInRange(Index.normal_index, Attrib.normals.size(), 3))
		{
			return false;
		}
		if (bHasTexcoords && Index.texcoord_index >= 0 &&
			!IndexInRange(Index.texcoord_index, Attrib.texcoords.size(), 2))
		{
			return false;
		}
		return true;
	}

	uint32 GetOrCreateVertex(FMeshData& Data, TMap<FVertexKey, uint32>& Unique, const tinyobj::attrib_t& Attrib,
		const tinyobj::index_t& Index, bool bHasFileNormals, bool bHasTexcoords)
	{
		const FVertexKey Key{Index.vertex_index, Index.normal_index, Index.texcoord_index};
		if (const uint32* Found = Unique.Find(Key))
		{
			return *Found;
		}

		FVertex Vertex{};
		const size_t Vi = static_cast<size_t>(Index.vertex_index) * 3u;
		Vertex.Position = FVector(Attrib.vertices[Vi + 0], Attrib.vertices[Vi + 1], Attrib.vertices[Vi + 2]);

		if (bHasFileNormals && Index.normal_index >= 0)
		{
			const size_t Ni = static_cast<size_t>(Index.normal_index) * 3u;
			const FVector N(Attrib.normals[Ni + 0], Attrib.normals[Ni + 1], Attrib.normals[Ni + 2]);
			Vertex.Normal = (FVector::DotProduct(N, N) > 0.0f) ? N.GetUnsafeNormal() : FVector(0.0f, 1.0f, 0.0f);
		}
		else
		{
			Vertex.Normal = FVector(0.0f, 1.0f, 0.0f);
		}

		if (bHasTexcoords && Index.texcoord_index >= 0)
		{
			const size_t Ti = static_cast<size_t>(Index.texcoord_index) * 2u;
			Vertex.TexCoord = FVector2D(Attrib.texcoords[Ti + 0], Attrib.texcoords[Ti + 1]);
		}

		const uint32 NewIndex = static_cast<uint32>(Data.Vertices.Num());
		Unique.Add(Key, NewIndex);
		Data.Vertices.Add(Vertex);
		return NewIndex;
	}

	FMaterial MaterialFromTiny(const tinyobj::material_t& Src)
	{
		FMaterial Material;
		Material.Shading = EMaterialShadingModel::BlinnPhong;
		Material.Albedo = FVector(Src.diffuse[0], Src.diffuse[1], Src.diffuse[2]);
		Material.Specular = FVector(Src.specular[0], Src.specular[1], Src.specular[2]);
		Material.Alpha = Src.dissolve;
		// Max/OBJ often exports low Ns; remap so highlights read clearly in Blinn-Phong.
		const float Ns = FMath::Max(Src.shininess, 1.0f);
		Material.Shininess = FMath::Clamp((Ns * Ns * 0.25f) + (Ns * 2.0f), 8.0f, 256.0f);

		// Heuristic metalness from MTL (no explicit metal map): strong Ks relative to Kd.
		const float Kd = (Material.Albedo.X + Material.Albedo.Y + Material.Albedo.Z) / 3.0f;
		const float Ks = (Material.Specular.X + Material.Specular.Y + Material.Specular.Z) / 3.0f;
		if (Ks > 0.2f)
		{
			Material.Metallic = FMath::Clamp((Ks - 0.15f) / 0.6f, 0.0f, 1.0f);
			// Painted metals in this asset use gray Ks; keep some metal even when Kd is dark.
			if (Kd < 0.35f && Ks >= 0.35f)
			{
				Material.Metallic = FMath::Max(Material.Metallic, 0.65f);
			}
		}
		// Ensure specular floor so dielectrics still catch highlights.
		if (Ks < 0.04f)
		{
			Material.Specular = FVector(0.04f, 0.04f, 0.04f);
		}

		Material.SyncRoughnessFromShininess();
		Material.bCastsShadows = Material.Alpha >= 0.999f;
		return Material;
	}

} // namespace

FMeshData LoadObj(const FString& Path)
{
	tinyobj::ObjReaderConfig Config;
	Config.triangulate = true;
	Config.mtl_search_path = TCHAR_TO_UTF8(*FPaths::GetPath(Path));

	tinyobj::ObjReader Reader;
	if (!Reader.ParseFromFile(TCHAR_TO_UTF8(*Path), Config))
	{
		if (!Reader.Error().empty())
		{
			UE_LOG(LogMeshUtilities, Error, "tinyobjloader: %s", Reader.Error().c_str());
		}
		return FMeshData();
	}

	if (!Reader.Warning().empty())
	{
		UE_LOG(LogMeshUtilities, Warning, "tinyobjloader: %s", Reader.Warning().c_str());
	}

	const auto& Attrib = Reader.GetAttrib();
	const auto& Shapes = Reader.GetShapes();
	const auto& TinyMaterials = Reader.GetMaterials();

	int32 IndexEstimate = 0;
	for (const auto& Shape : Shapes)
	{
		IndexEstimate += static_cast<int32>(Shape.mesh.indices.size());
	}

	FMeshData Data;
	Data.Vertices.Reserve(IndexEstimate);

	TMap<FVertexKey, uint32> Unique;
	Unique.Reserve(IndexEstimate);

	// Material id -> triangle indices, so each FMeshSection is contiguous (sorted by id below).
	TMap<int32, TArray<uint32>> IndicesByMaterial;

	const bool bHasFileNormals = !Attrib.normals.empty();
	const bool bHasTexcoords = !Attrib.texcoords.empty();

	for (const auto& Shape : Shapes)
	{
		size_t IndexOffset = 0;
		for (size_t Face = 0; Face < Shape.mesh.num_face_vertices.size(); ++Face)
		{
			const uint32 FaceVerts = Shape.mesh.num_face_vertices[Face];
			const int32 MaterialId = Face < Shape.mesh.material_ids.size() ? Shape.mesh.material_ids[Face] : -1;

			// Triangulated OBJ: expect 3 verts per face.
			if (FaceVerts != 3)
			{
				IndexOffset += FaceVerts;
				continue;
			}

			const tinyobj::index_t& I0 = Shape.mesh.indices[IndexOffset + 0];
			const tinyobj::index_t& I1 = Shape.mesh.indices[IndexOffset + 1];
			const tinyobj::index_t& I2 = Shape.mesh.indices[IndexOffset + 2];
			IndexOffset += 3;

			if (!FaceIndicesValid(Attrib, I0, bHasFileNormals, bHasTexcoords) ||
				!FaceIndicesValid(Attrib, I1, bHasFileNormals, bHasTexcoords) ||
				!FaceIndicesValid(Attrib, I2, bHasFileNormals, bHasTexcoords))
			{
				UE_LOG(LogMeshUtilities, Warning, "MeshData: skipping face with out-of-range indices in %s", *Path);
				continue;
			}

			TArray<uint32>& Bucket = IndicesByMaterial.FindOrAdd(MaterialId);
			Bucket.Add(GetOrCreateVertex(Data, Unique, Attrib, I0, bHasFileNormals, bHasTexcoords));
			Bucket.Add(GetOrCreateVertex(Data, Unique, Attrib, I1, bHasFileNormals, bHasTexcoords));
			Bucket.Add(GetOrCreateVertex(Data, Unique, Attrib, I2, bHasFileNormals, bHasTexcoords));
		}
	}

	if (Data.Vertices.Num() == 0 || IndicesByMaterial.Num() == 0)
	{
		UE_LOG(LogMeshUtilities, Error, "Mesh has no geometry: %s", *Path);
		return FMeshData();
	}

	if (!TinyMaterials.empty())
	{
		Data.Materials.Reserve(static_cast<int32>(TinyMaterials.size()));
		Data.AlbedoMapPaths.Reserve(static_cast<int32>(TinyMaterials.size()));
		const FString ObjDir = FPaths::GetPath(Path);
		for (const tinyobj::material_t& Src : TinyMaterials)
		{
			Data.Materials.Add(MaterialFromTiny(Src));
			if (!Src.diffuse_texname.empty())
			{
				Data.AlbedoMapPaths.Add(FPaths::Combine(ObjDir, Src.diffuse_texname.c_str()));
			}
			else
			{
				Data.AlbedoMapPaths.AddDefaulted();
			}
		}
	}

	IndicesByMaterial.KeySort(TLess<int32>());
	Data.Indices.Reserve(IndexEstimate);
	for (const auto& Pair : IndicesByMaterial)
	{
		const TArray<uint32>& Bucket = Pair.Value;
		if (Bucket.Num() == 0)
		{
			continue;
		}

		int32 Slot = 0;
		if (Pair.Key >= 0 && Pair.Key < Data.Materials.Num())
		{
			Slot = Pair.Key;
		}

		FMeshSection Sub;
		Sub.IndexOffset = Data.Indices.Num();
		Sub.IndexCount = Bucket.Num();
		Sub.MaterialIndex = Slot;
		Data.Indices.Append(Bucket);
		Data.Submeshes.Add(Sub);
	}

	if (Data.Materials.Num() == 0)
	{
		Data.Materials.Add(FMaterial{});
		Data.AlbedoMapPaths.AddDefaulted();
		for (FMeshSection& Sub : Data.Submeshes)
		{
			Sub.MaterialIndex = 0;
		}
	}

	if (!bHasFileNormals)
	{
		ComputeSmoothNormals(Data);
	}

	UE_LOG(LogMeshUtilities, Log, "OBJ '%s': %d verts, %d tris, %d submeshes, %d materials", *Path, Data.Vertices.Num(),
		Data.Indices.Num() / 3, Data.Submeshes.Num(), Data.Materials.Num());
	return Data;
}
