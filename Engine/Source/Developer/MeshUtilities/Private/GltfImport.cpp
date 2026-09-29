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
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GltfImport.h"
#include "GltfScene.h"
#include "ImportCoordinateConversion.h"
#include "MeshData.h"
#include "MeshUtilitiesLog.h"
#include "Misc/CString.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{

	/**
	 * What an animation's extras carry for its clip (Docs/ASSET_FORMATS.md, Docs/ART_PIPELINE.md): `loop`, 1 or 0 (or
	 * true / false; a clip without it loops) into OutbLoop, and the notifies (`{"notifies": [{"name": "Footstep_L",
	 * "time": 0.25}, ...]}`, time in seconds of the glTF timeline) into OutNotifies, relative to ClipStart, clamped to
	 * the clip and sorted by time (stable). False with OutError when the extras are not that shape.
	 */
	bool ReadAnimationExtras(const cgltf_extras& Extras, const FString& AnimationName, float ClipStart,
		float ClipLength, bool& OutbLoop, TArray<FRawAnimNotify>& OutNotifies, FString& OutError)
	{
		OutbLoop = true;
		OutNotifies.Reset();
		if (Extras.data == nullptr)
		{
			return true;
		}
		TSharedPtr<FJsonObject> Object;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(UTF8_TO_TCHAR(Extras.data))), Object) ||
			!Object.IsValid())
		{
			OutError = FString::Printf("the extras of the animation '%s' are not a JSON object", *AnimationName);
			return false;
		}
		const TSharedPtr<FJsonValue> Loop = Object->TryGetField(TEXT("loop"));
		if (Loop.IsValid())
		{
			bool bLoop = false;
			double Number = -1.0;
			if (Loop->Type == EJson::Boolean && Loop->TryGetBool(bLoop))
			{
				OutbLoop = bLoop;
			}
			else if (Loop->Type == EJson::Number && Loop->TryGetNumber(Number) && (Number == 0.0 || Number == 1.0))
			{
				OutbLoop = Number == 1.0;
			}
			else
			{
				OutError = FString::Printf("the \"loop\" of the animation '%s' must be 1 or 0", *AnimationName);
				return false;
			}
		}
		if (!Object->HasField(TEXT("notifies")))
		{
			return true;
		}
		// N26's first exporter wrote a "Name@frame,..." string, which read as no notifies: a list or an error now.
		const TArray<TSharedPtr<FJsonValue>>* Notifies = nullptr;
		if (!Object->TryGetArrayField(TEXT("notifies"), Notifies))
		{
			OutError = FString::Printf(
				"the \"notifies\" of the animation '%s' must be a list of {\"name\", \"time\"}", *AnimationName);
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Notifies)
		{
			const TSharedPtr<FJsonObject>& Entry = Value.IsValid() ? Value->AsObject() : TSharedPtr<FJsonObject>();
			FString Name;
			float Time = 0.0f;
			if (!Entry.IsValid() || !Entry->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty() ||
				!Entry->TryGetNumberField(TEXT("time"), Time))
			{
				OutError = FString::Printf(
					"a notify of the animation '%s' needs a \"name\" and a \"time\" (seconds)", *AnimationName);
				return false;
			}
			FRawAnimNotify& Notify = OutNotifies.AddDefaulted_GetRef();
			Notify.NotifyName = FName(*Name);
			Notify.Time = FMath::Clamp(Time - ClipStart, 0.0f, ClipLength);
		}
		StableSort(OutNotifies.GetData(), OutNotifies.Num(),
			[](const FRawAnimNotify& A, const FRawAnimNotify& B) { return A.Time < B.Time; });
		return true;
	}

	/**
	 * The physical material a glTF material's extras name (`{"physMaterial": "/Game/PhysicalMaterials/PM_Wood"}`,
	 * Docs/ART_PIPELINE.md), or empty; extras that are not a JSON object, or a physMaterial that is not a string, are
	 * reported and name none.
	 */
	FString ReadMaterialPhysMaterial(const cgltf_material& Material)
	{
		if (Material.extras.data == nullptr)
		{
			return FString();
		}
		const FString MaterialName(Material.name != nullptr ? UTF8_TO_TCHAR(Material.name) : TEXT("Material"));
		TSharedPtr<FJsonObject> Object;
		if (!FJsonSerializer::Deserialize(
				TJsonReaderFactory<>::Create(FString(UTF8_TO_TCHAR(Material.extras.data))), Object) ||
			!Object.IsValid())
		{
			UE_LOG(LogMeshUtilities, Warning, "glTF: the extras of the material '%s' are not a JSON object",
				*MaterialName);
			return FString();
		}
		if (!Object->HasField(TEXT("physMaterial")))
		{
			return FString();
		}
		FString Path;
		if (!Object->TryGetStringField(TEXT("physMaterial"), Path))
		{
			UE_LOG(LogMeshUtilities, Warning,
				"glTF: the \"physMaterial\" of the material '%s' must be a physical material's path", *MaterialName);
			return FString();
		}
		return Path;
	}

	/** glTF 2.0 is right-handed, Y up, in metres. */
	FImportCoordinateConversion GltfConversion()
	{
		return FImportCoordinateConversion(EImportAxes::RightHandedYUp, FImportCoordinateConversion::CmPerMetre);
	}

	/** The frame rate imported clips are sampled at (the engine's fixed step). */
	constexpr float AnimationFrameRate = 30.0f;

	[[nodiscard]] uint32 ReadIndex(const cgltf_accessor* Acc, cgltf_size I)
	{
		cgltf_uint Out = 0;
		cgltf_accessor_read_uint(Acc, I, &Out, 1);
		return static_cast<uint32>(Out);
	}

	/** The value of a base64 digit, or -1. */
	[[nodiscard]] int32 Base64Digit(ANSICHAR Char)
	{
		if (Char >= 'A' && Char <= 'Z')
		{
			return Char - 'A';
		}
		if (Char >= 'a' && Char <= 'z')
		{
			return Char - 'a' + 26;
		}
		if (Char >= '0' && Char <= '9')
		{
			return Char - '0' + 52;
		}
		return Char == '+' ? 62 : Char == '/' ? 63 : -1;
	}

	/** The bytes of a `data:[<type>];base64,<data>` URI; false when it is not one. */
	[[nodiscard]] bool DecodeDataUri(const ANSICHAR* Uri, TArray<uint8>& OutBytes)
	{
		OutBytes.Reset();
		if (FCStringAnsi::Strncmp(Uri, "data:", 5) != 0)
		{
			return false;
		}
		const ANSICHAR* Comma = Uri;
		while (*Comma != '\0' && *Comma != ',')
		{
			++Comma;
		}
		if (*Comma != ',')
		{
			return false;
		}
		uint32 Bits = 0;
		int32 NumBits = 0;
		for (const ANSICHAR* Char = Comma + 1; *Char != '\0' && *Char != '='; ++Char)
		{
			const int32 Digit = Base64Digit(*Char);
			if (Digit < 0)
			{
				return false;
			}
			Bits = (Bits << 6) | uint32(Digit);
			NumBits += 6;
			if (NumBits >= 8)
			{
				NumBits -= 8;
				OutBytes.Add(uint8((Bits >> NumBits) & 0xFF));
			}
		}
		return true;
	}

	/**
	 * A slot's base colour image: the absolute path of an external file next to the glTF (OutPath), or the bytes of an
	 * embedded one (a buffer view of a .glb, a data URI: OutEmbedded). Nothing without an image.
	 */
	void ReadImage(const cgltf_data& Data, const cgltf_texture_view& View, const FString& GltfDir, FString& OutPath,
		FMeshEmbeddedImage& OutEmbedded)
	{
		OutPath.Empty();
		OutEmbedded = FMeshEmbeddedImage();
		const cgltf_image* Image = View.texture != nullptr ? View.texture->image : nullptr;
		if (Image == nullptr)
		{
			return;
		}
		const int32 ImageIndex = int32(Image - Data.images);
		const FString ImageName = Image->name != nullptr && FCStringAnsi::Strlen(Image->name) > 0
			? FPaths::GetBaseFilename(FString(UTF8_TO_TCHAR(Image->name)))
			: FString::Printf("Image_%d", ImageIndex);
		if (Image->buffer_view != nullptr)
		{
			const cgltf_buffer_view& BufferView = *Image->buffer_view;
			if (BufferView.buffer != nullptr && BufferView.buffer->data != nullptr)
			{
				OutEmbedded.Name = ImageName;
				OutEmbedded.EncodedData.Append(
					static_cast<const uint8*>(BufferView.buffer->data) + BufferView.offset, int32(BufferView.size));
			}
			return;
		}
		if (Image->uri == nullptr || FCStringAnsi::Strlen(Image->uri) == 0)
		{
			return;
		}
		if (FCStringAnsi::Strncmp(Image->uri, "data:", 5) == 0)
		{
			if (DecodeDataUri(Image->uri, OutEmbedded.EncodedData))
			{
				OutEmbedded.Name = ImageName;
			}
			else
			{
				UE_LOG(LogMeshUtilities, Warning, "glTF: the data URI of image %d is not base64; it is skipped",
					ImageIndex);
				OutEmbedded.EncodedData.Empty();
			}
			return;
		}
		OutPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(GltfDir, FString(Image->uri)));
		FPaths::NormalizeFilename(OutPath);
	}

	/**
	 * A slot's material from a glTF material (null: glTF's default material): the base colour factor, its alpha and the
	 * base colour texture. The rest of the metallic-roughness model has nothing to drive on the GS.
	 */
	void AddMaterialSlot(const cgltf_data& Data, const cgltf_material* Mat, const FString& GltfDir, FMeshData& Mesh)
	{
		FMaterial M{};
		M.Albedo = FVector(0.8f, 0.8f, 0.8f);
		FString BaseMap;
		FMeshEmbeddedImage Embedded;
		if (Mat != nullptr && Mat->has_pbr_metallic_roughness)
		{
			const cgltf_pbr_metallic_roughness& Pbr = Mat->pbr_metallic_roughness;
			M.Albedo = FVector(Pbr.base_color_factor[0], Pbr.base_color_factor[1], Pbr.base_color_factor[2]);
			M.Alpha = Pbr.base_color_factor[3];
			ReadImage(Data, Pbr.base_color_texture, GltfDir, BaseMap, Embedded);
		}
		Mesh.Materials.Add(M);
		Mesh.MaterialSlotNames.Add(Mat != nullptr ? FString(Mat->name != nullptr ? Mat->name : "Material") : FString());
		Mesh.AlbedoMapPaths.Add(BaseMap);
		Mesh.AlbedoMapImages.Add(MoveTemp(Embedded));
		Mesh.PhysicalMaterialNames.Add(Mat != nullptr ? ReadMaterialPhysMaterial(*Mat) : FString());
	}

	/** A skinned vertex's influences before the reduction: each set's (joint, weight). */
	struct FJointWeight
	{
		int32 Joint = 0;
		float Weight = 0.0f;
	};

	/**
	 * The two largest influences of a vertex, renormalized and quantized to 1/255 steps adding up to 255, as bones
	 * (JointToBone maps a skin joint to its bone). Equal weights keep the lower joint first; a joint named twice adds
	 * up.
	 */
	[[nodiscard]] FSkinWeightInfo ReduceInfluences(TArray<FJointWeight>& Influences, const TArray<int32>& JointToBone)
	{
		// Merge a joint named by several sets, drop the empty ones.
		TArray<FJointWeight> Merged;
		float Total = 0.0f;
		for (const FJointWeight& Influence : Influences)
		{
			if (!(Influence.Weight > 0.0f) || !JointToBone.IsValidIndex(Influence.Joint))
			{
				continue;
			}
			Total += Influence.Weight;
			FJointWeight* Existing = Merged.FindByPredicate(
				[&Influence](const FJointWeight& Other) { return Other.Joint == Influence.Joint; });
			if (Existing != nullptr)
			{
				Existing->Weight += Influence.Weight;
			}
			else
			{
				Merged.Add(Influence);
			}
		}
		FSkinWeightInfo Info;
		if (Merged.Num() == 0 || !(Total > 0.0f))
		{
			// No weight: the vertex follows the first joint.
			Info.InfluenceBones[0] = Info.InfluenceBones[1] = uint8(JointToBone.Num() > 0 ? JointToBone[0] : 0);
			return Info;
		}
		Merged.StableSort([](const FJointWeight& A, const FJointWeight& B)
			{ return A.Weight > B.Weight || (A.Weight == B.Weight && A.Joint < B.Joint); });
		const float First = Merged[0].Weight / Total;
		const float Second = Merged.Num() > 1 ? Merged[1].Weight / Total : 0.0f;
		const int32 FirstByte = FMath::Clamp(FMath::RoundToInt((First / (First + Second)) * 255.0f), 0, 255);
		Info.InfluenceBones[0] = uint8(JointToBone[Merged[0].Joint]);
		Info.InfluenceWeights[0] = uint8(FirstByte);
		Info.InfluenceWeights[1] = uint8(255 - FirstByte);
		Info.InfluenceBones[1] = Info.InfluenceWeights[1] != 0 && Merged.Num() > 1 ? uint8(JointToBone[Merged[1].Joint])
																				   : Info.InfluenceBones[0];
		if (Info.InfluenceWeights[1] != 0 && Merged.Num() <= 1)
		{
			Info.InfluenceWeights[0] = 255;
			Info.InfluenceWeights[1] = 0;
		}
		return Info;
	}

	/**
	 * Appends every triangle primitive of a glTF mesh to Mesh (in glTF's space): one section and material slot per
	 * primitive. BaseVertex is the index of the first vertex appended and moves past the last. With OutSkinWeights,
	 * each vertex's JOINTS_n / WEIGHTS_n reduced to two bones (JointToBone: a skin joint's bone) go there too.
	 */
	void AppendMeshPrimitives(const cgltf_data& Data, const cgltf_mesh& Gmesh, const FString& GltfDir, FMeshData& Mesh,
		uint32& BaseVertex, const TArray<int32>* JointToBone = nullptr,
		TArray<FSkinWeightInfo>* OutSkinWeights = nullptr)
	{
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
			TArray<const cgltf_accessor*> Joints;
			TArray<const cgltf_accessor*> Weights;
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
				else if (Attr.type == cgltf_attribute_type_joints && Attr.index >= 0)
				{
					Joints.SetNumZeroed(FMath::Max(Joints.Num(), Attr.index + 1));
					Joints[Attr.index] = Attr.data;
				}
				else if (Attr.type == cgltf_attribute_type_weights && Attr.index >= 0)
				{
					Weights.SetNumZeroed(FMath::Max(Weights.Num(), Attr.index + 1));
					Weights[Attr.index] = Attr.data;
				}
			}
			if (Pos == nullptr)
			{
				continue;
			}

			const cgltf_size Vcount = Pos->count;
			const int32 StartIndex = Mesh.Indices.Num();
			TArray<FJointWeight> Influences;
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
					// glTF's UV origin is the image's top left; the engine's textures keep their bottom row first
					// (UTextureFactory, OpenGL's order), so v = 0 is the bottom (FBX's convention, which the import
					// replaced in N21 without turning v over: the first painted textures, N27, showed it).
					V.TexCoord = FVector2D(Tmp[0], 1.0f - Tmp[1]);
				}
				Mesh.Vertices.Add(V);

				if (OutSkinWeights != nullptr && JointToBone != nullptr)
				{
					Influences.Reset();
					for (int32 Set = 0; Set < FMath::Min(Joints.Num(), Weights.Num()); ++Set)
					{
						cgltf_uint JointValues[4] = {0, 0, 0, 0};
						float WeightValues[4] = {0.0f, 0.0f, 0.0f, 0.0f};
						if (Joints[Set] == nullptr || Weights[Set] == nullptr ||
							!cgltf_accessor_read_uint(Joints[Set], Vi, JointValues, 4) ||
							!cgltf_accessor_read_float(Weights[Set], Vi, WeightValues, 4))
						{
							continue;
						}
						for (int32 Component = 0; Component < 4; ++Component)
						{
							Influences.Add(FJointWeight{int32(JointValues[Component]), WeightValues[Component]});
						}
					}
					OutSkinWeights->Add(ReduceInfluences(Influences, *JointToBone));
				}
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

			AddMaterialSlot(Data, Prim.material, GltfDir, Mesh);

			BaseVertex += static_cast<uint32>(Vcount);
		}
	}

	/** Parses Path and loads its buffers; null (with OutError) on a failure. The caller frees it (cgltf_free). */
	[[nodiscard]] cgltf_data* ParseGltf(const FString& Path, FString& OutError)
	{
		cgltf_options Options{};
		cgltf_data* Data = nullptr;
		cgltf_result Result = cgltf_parse_file(&Options, TCHAR_TO_UTF8(*Path), &Data);
		if (Result != cgltf_result_success)
		{
			OutError = "cgltf_parse_file failed for " + Path;
			return nullptr;
		}
		Result = cgltf_load_buffers(&Options, Data, TCHAR_TO_UTF8(*Path));
		if (Result != cgltf_result_success)
		{
			OutError = "cgltf_load_buffers failed for " + Path;
			cgltf_free(Data);
			return nullptr;
		}
		return Data;
	}

	/** A cgltf column-major matrix as an FMatrix (row vectors: the rows are the axes' images, row 3 the offset). */
	[[nodiscard]] FMatrix ToMatrix(const cgltf_float (&Values)[16])
	{
		FMatrix Matrix;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				Matrix.M[Row][Column] = Values[(Row * 4) + Column];
			}
		}
		return Matrix;
	}

	[[nodiscard]] FMatrix GetWorldMatrix(const cgltf_node& Node)
	{
		cgltf_float World[16];
		cgltf_node_transform_world(&Node, World);
		return ToMatrix(World);
	}

	/** A node's local transform in glTF's space: its TRS, or its matrix taken apart. */
	[[nodiscard]] FTransform GetLocalTransform(const cgltf_node& Node)
	{
		if (Node.has_matrix)
		{
			cgltf_float Local[16];
			cgltf_node_transform_local(&Node, Local);
			return FTransform(ToMatrix(Local));
		}
		FTransform Transform;
		if (Node.has_translation)
		{
			Transform.SetTranslation(FVector(Node.translation[0], Node.translation[1], Node.translation[2]));
		}
		if (Node.has_rotation)
		{
			Transform.SetRotation(
				FQuat(Node.rotation[0], Node.rotation[1], Node.rotation[2], Node.rotation[3]).GetNormalized());
		}
		if (Node.has_scale)
		{
			Transform.SetScale3D(FVector(Node.scale[0], Node.scale[1], Node.scale[2]));
		}
		return Transform;
	}

	/** The prefix of a socket node (UE's FBX static mesh sockets). */
	constexpr const ANSICHAR* SocketPrefix = "SOCKET_";

	[[nodiscard]] bool IsSocketNode(const cgltf_node& Node)
	{
		return Node.name != nullptr &&
			FCStringAnsi::Strncmp(Node.name, SocketPrefix, FCStringAnsi::Strlen(SocketPrefix)) == 0;
	}

	[[nodiscard]] FString GetNodeName(const cgltf_data& Data, const cgltf_node& Node)
	{
		return Node.name != nullptr ? FString(UTF8_TO_TCHAR(Node.name))
									: FString::Printf("Node_%d", static_cast<int32>(&Node - Data.nodes));
	}

	/**
	 * The sockets of the file: each `SOCKET_<Name>` node, relative to its nearest ancestor with a mesh (whose vertices
	 * are the mesh's space: the static import ignores the mesh nodes' transforms), in the engine's axes and units.
	 */
	void ReadSockets(
		const cgltf_data& Data, const FImportCoordinateConversion& Conversion, TArray<FMeshSocketData>& Out)
	{
		const int32 PrefixLength = FCStringAnsi::Strlen(SocketPrefix);
		for (cgltf_size Ni = 0; Ni < Data.nodes_count; ++Ni)
		{
			const cgltf_node& Node = Data.nodes[Ni];
			if (!IsSocketNode(Node))
			{
				continue;
			}
			FMatrix SocketMatrix = GetWorldMatrix(Node);
			const cgltf_node* MeshNode = Node.parent;
			while (MeshNode != nullptr && MeshNode->mesh == nullptr)
			{
				MeshNode = MeshNode->parent;
			}
			if (MeshNode != nullptr)
			{
				SocketMatrix = SocketMatrix * GetWorldMatrix(*MeshNode).Inverse();
			}
			FMeshSocketData& Socket = Out.AddDefaulted_GetRef();
			Socket.Name = FString(Node.name + PrefixLength);
			Socket.Transform = FTransform(Conversion.ConvertMatrix(SocketMatrix));
		}
	}

	/**
	 * The bones of a set of joint nodes (a skin's joints, in its order): each joint's parent joint (its nearest
	 * ancestor among them) and an order in which parents come first. OutOrder[Bone] is a joint; OutJointToBone its
	 * inverse.
	 */
	void OrderJoints(const TArray<const cgltf_node*>& JointNodes, TArray<int32>& OutJointParents,
		TArray<int32>& OutOrder, TArray<int32>& OutJointToBone)
	{
		const int32 NumJoints = JointNodes.Num();
		OutJointParents.Init(INDEX_NONE, NumJoints);
		for (int32 Joint = 0; Joint < NumJoints; ++Joint)
		{
			for (const cgltf_node* Ancestor = JointNodes[Joint]->parent; Ancestor != nullptr;
				Ancestor = Ancestor->parent)
			{
				const int32 ParentJoint = JointNodes.IndexOfByKey(Ancestor);
				if (ParentJoint != INDEX_NONE)
				{
					OutJointParents[Joint] = ParentJoint;
					break;
				}
			}
		}
		// Each joint after its ancestors, the skin's order otherwise (so an ordered skin keeps its order).
		OutOrder.Reset();
		OutJointToBone.Init(INDEX_NONE, NumJoints);
		TArray<int32> Chain;
		for (int32 Joint = 0; Joint < NumJoints; ++Joint)
		{
			Chain.Reset();
			for (int32 Current = Joint; Current != INDEX_NONE && OutJointToBone[Current] == INDEX_NONE;
				Current = OutJointParents[Current])
			{
				if (Chain.Contains(Current))
				{
					break;
				}
				Chain.Add(Current);
			}
			for (int32 Index = Chain.Num() - 1; Index >= 0; --Index)
			{
				OutJointToBone[Chain[Index]] = OutOrder.Num();
				OutOrder.Add(Chain[Index]);
			}
		}
	}

	/**
	 * The transform from a joint node's parent up to its parent joint (or the scene root for a root joint), through the
	 * nodes in between, at rest, in glTF's space: what the bone's local transform picks up after the node's own.
	 */
	[[nodiscard]] FTransform GetIntermediateTransform(const cgltf_node& JointNode, const cgltf_node* ParentJointNode)
	{
		FTransform Intermediate;
		for (const cgltf_node* Ancestor = JointNode.parent; Ancestor != nullptr && Ancestor != ParentJointNode;
			Ancestor = Ancestor->parent)
		{
			Intermediate = Intermediate * GetLocalTransform(*Ancestor);
		}
		return Intermediate;
	}

	/** The skeleton of a set of joint nodes (names, parents, reference pose; the caller fills the bind pose). */
	void BuildReferenceSkeleton(const cgltf_data& Data, const TArray<const cgltf_node*>& JointNodes,
		const TArray<int32>& JointParents, const TArray<int32>& Order, const TArray<int32>& JointToBone,
		const FImportCoordinateConversion& Conversion, FReferenceSkeleton& Out)
	{
		Out = FReferenceSkeleton();
		for (const int32 Joint : Order)
		{
			const cgltf_node& Node = *JointNodes[Joint];
			const int32 ParentJoint = JointParents[Joint];
			Out.BoneNames.Add(FName(*GetNodeName(Data, Node)));
			Out.ParentIndices.Add(ParentJoint != INDEX_NONE ? JointToBone[ParentJoint] : INDEX_NONE);
			const FTransform Local = GetLocalTransform(Node) *
				GetIntermediateTransform(Node, ParentJoint != INDEX_NONE ? JointNodes[ParentJoint] : nullptr);
			Out.RefBonePose.Add(Conversion.ConvertTransform(Local));
		}
	}

	/** The joint nodes of a skin, in its order. */
	[[nodiscard]] TArray<const cgltf_node*> GetSkinJoints(const cgltf_skin& Skin)
	{
		TArray<const cgltf_node*> Joints;
		for (cgltf_size Index = 0; Index < Skin.joints_count; ++Index)
		{
			Joints.Add(Skin.joints[Index]);
		}
		return Joints;
	}

	/** The first skin a mesh node uses (in node order), or null. */
	[[nodiscard]] const cgltf_skin* FindMeshSkin(const cgltf_data& Data)
	{
		for (cgltf_size Ni = 0; Ni < Data.nodes_count; ++Ni)
		{
			if (Data.nodes[Ni].mesh != nullptr && Data.nodes[Ni].skin != nullptr)
			{
				return Data.nodes[Ni].skin;
			}
		}
		return nullptr;
	}

	/** Frees a cgltf_data at the end of a scope. */
	struct FScopedGltfData
	{
		explicit FScopedGltfData(cgltf_data* InData)
			: Data(InData)
		{
		}
		~FScopedGltfData()
		{
			if (Data != nullptr)
			{
				cgltf_free(Data);
			}
		}
		FScopedGltfData(const FScopedGltfData&) = delete;
		FScopedGltfData& operator=(const FScopedGltfData&) = delete;

		cgltf_data* Data = nullptr;
	};

	/** An animation channel's sampler for one of a node's paths, or null. */
	struct FNodeChannels
	{
		const cgltf_animation_sampler* Translation = nullptr;
		const cgltf_animation_sampler* Rotation = nullptr;
		const cgltf_animation_sampler* Scale = nullptr;
	};

	/**
	 * The keys of a sampler around Time: the last key at or before it (OutA), the next one (OutB) and the weight of B
	 * (0 for STEP, and before the first or after the last key).
	 */
	void FindSamplerKeys(
		const cgltf_animation_sampler& Sampler, float Time, cgltf_size& OutA, cgltf_size& OutB, float& OutAlpha)
	{
		const cgltf_accessor& Input = *Sampler.input;
		OutA = 0;
		OutB = 0;
		OutAlpha = 0.0f;
		const cgltf_size Count = Input.count;
		if (Count == 0)
		{
			return;
		}
		float First = 0.0f;
		float Last = 0.0f;
		cgltf_accessor_read_float(&Input, 0, &First, 1);
		cgltf_accessor_read_float(&Input, Count - 1, &Last, 1);
		if (Count == 1 || Time <= First)
		{
			return;
		}
		if (Time >= Last)
		{
			OutA = OutB = Count - 1;
			return;
		}
		cgltf_size Low = 0;
		cgltf_size High = Count - 1;
		while (High - Low > 1)
		{
			const cgltf_size Middle = (Low + High) / 2;
			float MiddleTime = 0.0f;
			cgltf_accessor_read_float(&Input, Middle, &MiddleTime, 1);
			if (MiddleTime <= Time)
			{
				Low = Middle;
			}
			else
			{
				High = Middle;
			}
		}
		OutA = Low;
		OutB = High;
		if (Sampler.interpolation == cgltf_interpolation_type_step)
		{
			return;
		}
		float TimeA = 0.0f;
		float TimeB = 0.0f;
		cgltf_accessor_read_float(&Input, Low, &TimeA, 1);
		cgltf_accessor_read_float(&Input, High, &TimeB, 1);
		OutAlpha = TimeB > TimeA ? (Time - TimeA) / (TimeB - TimeA) : 0.0f;
	}

	[[nodiscard]] FVector SampleVector(const cgltf_animation_sampler& Sampler, float Time)
	{
		cgltf_size KeyA = 0;
		cgltf_size KeyB = 0;
		float Alpha = 0.0f;
		FindSamplerKeys(Sampler, Time, KeyA, KeyB, Alpha);
		float A[3] = {0.0f, 0.0f, 0.0f};
		float B[3] = {0.0f, 0.0f, 0.0f};
		cgltf_accessor_read_float(Sampler.output, KeyA, A, 3);
		cgltf_accessor_read_float(Sampler.output, KeyB, B, 3);
		const FVector VectorA(A[0], A[1], A[2]);
		return VectorA + ((FVector(B[0], B[1], B[2]) - VectorA) * Alpha);
	}

	[[nodiscard]] FQuat SampleRotation(const cgltf_animation_sampler& Sampler, float Time)
	{
		cgltf_size KeyA = 0;
		cgltf_size KeyB = 0;
		float Alpha = 0.0f;
		FindSamplerKeys(Sampler, Time, KeyA, KeyB, Alpha);
		float A[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		float B[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		cgltf_accessor_read_float(Sampler.output, KeyA, A, 4);
		cgltf_accessor_read_float(Sampler.output, KeyB, B, 4);
		const FQuat RotationA = FQuat(A[0], A[1], A[2], A[3]).GetNormalized();
		return Alpha > 0.0f ? FQuat::Slerp(RotationA, FQuat(B[0], B[1], B[2], B[3]).GetNormalized(), Alpha) : RotationA;
	}

	/**
	 * The nodes of InSkeleton's bones in the file (by name), checked: each bone's nearest ancestor bone must be its
	 * parent, and a skin in the file must have the same bones (names, order, parents). False with OutError otherwise.
	 */
	[[nodiscard]] bool MatchSkeleton(const cgltf_data& Data, const FReferenceSkeleton& InSkeleton, const FString& Path,
		TArray<const cgltf_node*>& OutBoneNodes, FString& OutError)
	{
		OutBoneNodes.Reset();
		for (int32 Bone = 0; Bone < InSkeleton.GetNum(); ++Bone)
		{
			const FName BoneName = InSkeleton.GetBoneName(Bone);
			const cgltf_node* Found = nullptr;
			for (cgltf_size Ni = 0; Ni < Data.nodes_count && Found == nullptr; ++Ni)
			{
				if (Data.nodes[Ni].name != nullptr && FName(UTF8_TO_TCHAR(Data.nodes[Ni].name)) == BoneName)
				{
					Found = &Data.nodes[Ni];
				}
			}
			if (Found == nullptr)
			{
				OutError = FString::Printf(
					"the bones of '%s' do not match the skeleton: no node '%s'", *Path, *BoneName.ToString());
				return false;
			}
			OutBoneNodes.Add(Found);
		}
		for (int32 Bone = 0; Bone < OutBoneNodes.Num(); ++Bone)
		{
			int32 FileParent = INDEX_NONE;
			for (const cgltf_node* Ancestor = OutBoneNodes[Bone]->parent;
				Ancestor != nullptr && FileParent == INDEX_NONE; Ancestor = Ancestor->parent)
			{
				FileParent = OutBoneNodes.IndexOfByKey(Ancestor);
			}
			if (FileParent != InSkeleton.GetParentIndex(Bone))
			{
				OutError = FString::Printf("the bones of '%s' do not match the skeleton: '%s' has another parent",
					*Path, *InSkeleton.GetBoneName(Bone).ToString());
				return false;
			}
		}
		const cgltf_skin* Skin = FindMeshSkin(Data);
		if (Skin == nullptr && Data.skins_count > 0)
		{
			Skin = &Data.skins[0];
		}
		if (Skin != nullptr)
		{
			const TArray<const cgltf_node*> Joints = GetSkinJoints(*Skin);
			TArray<int32> JointParents;
			TArray<int32> Order;
			TArray<int32> JointToBone;
			OrderJoints(Joints, JointParents, Order, JointToBone);
			FReferenceSkeleton FileSkeleton;
			BuildReferenceSkeleton(Data, Joints, JointParents, Order, JointToBone, GltfConversion(), FileSkeleton);
			if (!FileSkeleton.HasSameBones(InSkeleton))
			{
				OutError = FString::Printf(
					"the bones of '%s' do not match the skeleton (names, order and parents of its skin)", *Path);
				return false;
			}
		}
		return true;
	}

} // namespace

bool LoadStaticMeshFromGltf(const FString& Path, FMeshData& Out, FString& OutError)
{
	Out = FMeshData();
	OutError.Empty();

	cgltf_data* Data = ParseGltf(Path, OutError);
	if (Data == nullptr)
	{
		return false;
	}

	const FString GltfDir = FPaths::GetPath(Path);

	FMeshData Mesh;
	uint32 BaseVertex = 0;
	for (cgltf_size Mi = 0; Mi < Data->meshes_count; ++Mi)
	{
		AppendMeshPrimitives(*Data, Data->meshes[Mi], GltfDir, Mesh, BaseVertex);
	}
	ReadSockets(*Data, GltfConversion(), Mesh.Sockets);

	cgltf_free(Data);

	if (Mesh.IsEmpty())
	{
		OutError = "glTF contained no triangle mesh data";
		return false;
	}
	GltfConversion().ConvertMeshData(Mesh);
	Out = MoveTemp(Mesh);
	return true;
}

bool LoadSkeletalMeshFromGltf(const FString& Path, FSkeletalMeshImportData& Out, FString& OutError)
{
	Out = FSkeletalMeshImportData();
	OutError.Empty();
	const FScopedGltfData Scoped(ParseGltf(Path, OutError));
	if (Scoped.Data == nullptr)
	{
		return false;
	}
	const cgltf_data& Data = *Scoped.Data;
	const cgltf_skin* Skin = FindMeshSkin(Data);
	if (Skin == nullptr || Skin->joints_count == 0)
	{
		OutError = FString::Printf("no skinned mesh in '%s'", *Path);
		return false;
	}
	if (int32(Skin->joints_count) > MaxSkinBones)
	{
		OutError = FString::Printf(
			"'%s' has %d joints; a skeleton has at most %d", *Path, int32(Skin->joints_count), MaxSkinBones);
		return false;
	}

	const FImportCoordinateConversion Conversion = GltfConversion();
	const TArray<const cgltf_node*> Joints = GetSkinJoints(*Skin);
	TArray<int32> JointParents;
	TArray<int32> Order;
	TArray<int32> JointToBone;
	OrderJoints(Joints, JointParents, Order, JointToBone);
	BuildReferenceSkeleton(Data, Joints, JointParents, Order, JointToBone, Conversion, Out.RefSkeleton);
	for (const int32 Joint : Order)
	{
		cgltf_float InverseBind[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
		if (Skin->inverse_bind_matrices != nullptr)
		{
			cgltf_accessor_read_float(Skin->inverse_bind_matrices, cgltf_size(Joint), InverseBind, 16);
		}
		Out.RefSkeleton.InverseBindPose.Add(Conversion.ConvertMatrix(ToMatrix(InverseBind)));
	}
	if (!Out.RefSkeleton.IsValid())
	{
		OutError = FString::Printf("the skin of '%s' has joints with the same name", *Path);
		return false;
	}

	// Every mesh node skinned with it; glTF skins in the skin's space and ignores the mesh nodes' transforms.
	const FString GltfDir = FPaths::GetPath(Path);
	uint32 BaseVertex = 0;
	for (cgltf_size Ni = 0; Ni < Data.nodes_count; ++Ni)
	{
		const cgltf_node& Node = Data.nodes[Ni];
		if (Node.mesh == nullptr)
		{
			continue;
		}
		if (Node.skin != Skin)
		{
			UE_LOG(LogMeshUtilities, Warning,
				"glTF: the mesh node '%s' of '%s' is not skinned with the first skin; skipped",
				*GetNodeName(Data, Node), *Path);
			continue;
		}
		AppendMeshPrimitives(Data, *Node.mesh, GltfDir, Out.Mesh, BaseVertex, &JointToBone, &Out.SkinWeights);
	}
	if (Out.Mesh.IsEmpty())
	{
		OutError = FString::Printf("the skinned meshes of '%s' have no triangles", *Path);
		return false;
	}
	Conversion.ConvertMeshData(Out.Mesh);

	// The sockets under the joints, at rest.
	const int32 PrefixLength = FCStringAnsi::Strlen(SocketPrefix);
	for (cgltf_size Ni = 0; Ni < Data.nodes_count; ++Ni)
	{
		const cgltf_node& Node = Data.nodes[Ni];
		if (!IsSocketNode(Node))
		{
			continue;
		}
		int32 Joint = INDEX_NONE;
		for (const cgltf_node* Ancestor = Node.parent; Ancestor != nullptr && Joint == INDEX_NONE;
			Ancestor = Ancestor->parent)
		{
			Joint = Joints.IndexOfByKey(Ancestor);
		}
		if (Joint == INDEX_NONE)
		{
			UE_LOG(LogMeshUtilities, Warning, "glTF: the socket '%s' of '%s' is under no joint; skipped",
				*GetNodeName(Data, Node), *Path);
			continue;
		}
		FBoneSocketData& Socket = Out.Sockets.AddDefaulted_GetRef();
		Socket.Name = FString(UTF8_TO_TCHAR(Node.name + PrefixLength));
		Socket.BoneName = Out.RefSkeleton.GetBoneName(JointToBone[Joint]);
		Socket.RelativeTransform =
			FTransform(Conversion.ConvertMatrix(GetWorldMatrix(Node) * GetWorldMatrix(*Joints[Joint]).Inverse()));
	}
	return true;
}

bool LoadAnimSequencesFromGltf(const FString& Path, const FReferenceSkeleton& InSkeleton, const FString& AnimationName,
	TArray<FRawAnimSequence>& Out, FString& OutError)
{
	Out.Reset();
	OutError.Empty();
	const FScopedGltfData Scoped(ParseGltf(Path, OutError));
	if (Scoped.Data == nullptr)
	{
		return false;
	}
	const cgltf_data& Data = *Scoped.Data;
	TArray<const cgltf_node*> BoneNodes;
	if (!MatchSkeleton(Data, InSkeleton, Path, BoneNodes, OutError))
	{
		return false;
	}
	if (Data.animations_count == 0)
	{
		OutError = FString::Printf("'%s' has no animations", *Path);
		return false;
	}

	const FImportCoordinateConversion Conversion = GltfConversion();
	TArray<FTransform> Intermediate;
	for (int32 Bone = 0; Bone < BoneNodes.Num(); ++Bone)
	{
		const int32 Parent = InSkeleton.GetParentIndex(Bone);
		Intermediate.Add(
			GetIntermediateTransform(*BoneNodes[Bone], Parent != INDEX_NONE ? BoneNodes[Parent] : nullptr));
	}

	for (cgltf_size Ai = 0; Ai < Data.animations_count; ++Ai)
	{
		const cgltf_animation& Animation = Data.animations[Ai];
		const FString Name = Animation.name != nullptr && FCStringAnsi::Strlen(Animation.name) > 0
			? FString(UTF8_TO_TCHAR(Animation.name))
			: FString::Printf("Animation_%d", int32(Ai));
		if (!AnimationName.IsEmpty() && Name != AnimationName)
		{
			continue;
		}

		// The bones' channels and the clip's time range.
		TArray<FNodeChannels> Channels;
		Channels.SetNum(BoneNodes.Num());
		float Start = TNumericLimits<float>::Max();
		float End = TNumericLimits<float>::Lowest();
		for (cgltf_size Ci = 0; Ci < Animation.channels_count; ++Ci)
		{
			const cgltf_animation_channel& Channel = Animation.channels[Ci];
			const cgltf_animation_sampler* Sampler = Channel.sampler;
			if (Sampler == nullptr || Sampler->input == nullptr || Sampler->output == nullptr ||
				Sampler->input->count == 0)
			{
				continue;
			}
			if (Sampler->interpolation == cgltf_interpolation_type_cubic_spline)
			{
				OutError = FString::Printf("the animation '%s' of '%s' uses CUBICSPLINE interpolation, which is not "
										   "supported: export it with LINEAR or STEP keys (Blender: Always Sample "
										   "Animations)",
					*Name, *Path);
				Out.Reset();
				return false;
			}
			float First = 0.0f;
			float Last = 0.0f;
			cgltf_accessor_read_float(Sampler->input, 0, &First, 1);
			cgltf_accessor_read_float(Sampler->input, Sampler->input->count - 1, &Last, 1);
			Start = FMath::Min(Start, First);
			End = FMath::Max(End, Last);
			const int32 Bone = BoneNodes.IndexOfByKey(Channel.target_node);
			if (Bone == INDEX_NONE)
			{
				continue;
			}
			switch (Channel.target_path)
			{
				case cgltf_animation_path_type_translation:
					Channels[Bone].Translation = Sampler;
					break;
				case cgltf_animation_path_type_rotation:
					Channels[Bone].Rotation = Sampler;
					break;
				case cgltf_animation_path_type_scale:
					Channels[Bone].Scale = Sampler;
					break;
				default:
					break;
			}
		}
		if (Start > End)
		{
			Start = End = 0.0f;
		}

		FRawAnimSequence& Clip = Out.AddDefaulted_GetRef();
		Clip.Name = FName(*Name);
		Clip.FrameRate = AnimationFrameRate;
		const int32 NumFrames = FMath::Max(FMath::RoundToInt((End - Start) * AnimationFrameRate), 0) + 1;
		Clip.SequenceLength = float(NumFrames - 1) / AnimationFrameRate;
		if (!ReadAnimationExtras(
				Animation.extras, Name, Start, Clip.SequenceLength, Clip.bLoop, Clip.Notifies, OutError))
		{
			OutError = FString::Printf("'%s': %s", *Path, *OutError);
			Out.Reset();
			return false;
		}
		Clip.Tracks.SetNum(BoneNodes.Num());
		for (int32 Bone = 0; Bone < BoneNodes.Num(); ++Bone)
		{
			const FTransform Rest = GetLocalTransform(*BoneNodes[Bone]);
			const FNodeChannels& Bound = Channels[Bone];
			FRawAnimSequenceTrack& Track = Clip.Tracks[Bone];
			for (int32 Frame = 0; Frame < NumFrames; ++Frame)
			{
				const float Time = Start + (float(Frame) / AnimationFrameRate);
				FTransform Local = Rest;
				if (Bound.Translation != nullptr)
				{
					Local.SetTranslation(SampleVector(*Bound.Translation, Time));
				}
				if (Bound.Rotation != nullptr)
				{
					Local.SetRotation(SampleRotation(*Bound.Rotation, Time));
				}
				if (Bound.Scale != nullptr)
				{
					Local.SetScale3D(SampleVector(*Bound.Scale, Time));
				}
				const FTransform Engine = Conversion.ConvertTransform(Local * Intermediate[Bone]);
				Track.PosKeys.Add(Engine.GetTranslation());
				Track.RotKeys.Add(Engine.GetRotation());
				Track.ScaleKeys.Add(Engine.GetScale3D());
			}
		}
	}
	if (Out.Num() == 0)
	{
		OutError = FString::Printf("'%s' has no animation named '%s'", *Path, *AnimationName);
		return false;
	}
	return true;
}

bool LoadGltfScene(const FString& Path, FGltfScene& Out, FString& OutError)
{
	Out = FGltfScene();
	OutError.Empty();

	cgltf_data* Data = ParseGltf(Path, OutError);
	if (Data == nullptr)
	{
		return false;
	}
	const FString GltfDir = FPaths::GetPath(Path);
	const FImportCoordinateConversion Conversion = GltfConversion();

	for (cgltf_size Mi = 0; Mi < Data->meshes_count; ++Mi)
	{
		const cgltf_mesh& Gmesh = Data->meshes[Mi];
		FGltfSceneMesh& Mesh = Out.Meshes.AddDefaulted_GetRef();
		Mesh.Name = Gmesh.name != nullptr ? FString(Gmesh.name) : FString::Printf("Mesh_%d", static_cast<int32>(Mi));
		uint32 BaseVertex = 0;
		AppendMeshPrimitives(*Data, Gmesh, GltfDir, Mesh.Data, BaseVertex);
		if (!Mesh.Data.IsEmpty())
		{
			Conversion.ConvertMeshData(Mesh.Data);
		}
	}

	for (cgltf_size Li = 0; Li < Data->lights_count; ++Li)
	{
		const cgltf_light& Glight = Data->lights[Li];
		FGltfSceneLight& Light = Out.Lights.AddDefaulted_GetRef();
		Light.Name = Glight.name != nullptr ? FString(Glight.name) : FString();
		Light.Type = Glight.type == cgltf_light_type_directional ? EGltfLightType::Directional
			: Glight.type == cgltf_light_type_spot               ? EGltfLightType::Spot
																 : EGltfLightType::Point;
		Light.Color = FLinearColor(Glight.color[0], Glight.color[1], Glight.color[2]);
		Light.Intensity = Glight.intensity;
		Light.Range = Glight.range > 0.0f ? Glight.range * Conversion.GetUnitsToCm() : 0.0f;
	}

	for (cgltf_size Ni = 0; Ni < Data->nodes_count; ++Ni)
	{
		const cgltf_node& Gnode = Data->nodes[Ni];
		FGltfSceneNode& Node = Out.Nodes.AddDefaulted_GetRef();
		Node.Name = Gnode.name != nullptr ? FString(Gnode.name) : FString::Printf("Node_%d", static_cast<int32>(Ni));
		Node.Parent = Gnode.parent != nullptr ? static_cast<int32>(Gnode.parent - Data->nodes) : INDEX_NONE;
		Node.Mesh = Gnode.mesh != nullptr ? static_cast<int32>(Gnode.mesh - Data->meshes) : INDEX_NONE;
		Node.Light = Gnode.light != nullptr ? static_cast<int32>(Gnode.light - Data->lights) : INDEX_NONE;
		const FMatrix WorldMatrix = Conversion.ConvertMatrix(GetWorldMatrix(Gnode));
		Node.WorldTransform = FTransform(WorldMatrix);
		if (Node.Light != INDEX_NONE)
		{
			// A glTF light shines along its local -Z; the converted matrix takes the converted axis to the world.
			Node.LightDirection =
				FVector(WorldMatrix.TransformVector(Conversion.ConvertDirection(FVector(0.0f, 0.0f, -1.0f))))
					.GetSafeNormal();
		}
		if (Gnode.extras.data != nullptr)
		{
			Node.Extras = FString(UTF8_TO_TCHAR(Gnode.extras.data));
		}
	}

	cgltf_free(Data);
	return true;
}
