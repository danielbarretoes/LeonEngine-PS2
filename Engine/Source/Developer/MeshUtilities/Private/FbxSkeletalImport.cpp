#include "FbxSkeletalImport.h"

#include "Containers/StringConv.h"
#include "LegacyGLMath.h"
#include "MeshUtilitiesLog.h"
#include "Misc/Paths.h"

#include <ufbx.h>

namespace
{

	/** node_to_parent as a GL-convention matrix: rotation * scale columns, translation in column 3 (like glm). */
	FMatrix ToMatrix(const ufbx_transform& T)
	{
		FMatrix M = LegacyGL::QuatToMatrix(static_cast<float>(T.rotation.w), static_cast<float>(T.rotation.x),
			static_cast<float>(T.rotation.y), static_cast<float>(T.rotation.z));
		const float Scale[3] = {
			static_cast<float>(T.scale.x), static_cast<float>(T.scale.y), static_cast<float>(T.scale.z)};
		for (int32 Column = 0; Column < 3; ++Column)
		{
			for (int32 Row = 0; Row < 4; ++Row)
			{
				M.M[Column][Row] *= Scale[Column];
			}
		}
		M.M[3][0] = static_cast<float>(T.translation.x);
		M.M[3][1] = static_cast<float>(T.translation.y);
		M.M[3][2] = static_cast<float>(T.translation.z);
		M.M[3][3] = 1.0f;
		return M;
	}

	/** ufbx's row-major 3x4 affine matrix in the GL memory layout (column c = the image of axis c). */
	FMatrix ToMatrix(const ufbx_matrix& In)
	{
		FMatrix Out = FMatrix::Identity;
		const float Columns[4][3] = {
			{static_cast<float>(In.m00), static_cast<float>(In.m10), static_cast<float>(In.m20)},
			{static_cast<float>(In.m01), static_cast<float>(In.m11), static_cast<float>(In.m21)},
			{static_cast<float>(In.m02), static_cast<float>(In.m12), static_cast<float>(In.m22)},
			{static_cast<float>(In.m03), static_cast<float>(In.m13), static_cast<float>(In.m23)},
		};
		for (int32 Column = 0; Column < 4; ++Column)
		{
			Out.M[Column][0] = Columns[Column][0];
			Out.M[Column][1] = Columns[Column][1];
			Out.M[Column][2] = Columns[Column][2];
			Out.M[Column][3] = Column == 3 ? 1.0f : 0.0f;
		}
		return Out;
	}

	ufbx_load_opts MakeLoadOpts()
	{
		ufbx_load_opts Opts{};
		Opts.target_axes = ufbx_axes_right_handed_y_up;
		Opts.target_unit_meters = 1.0f;
		Opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
		Opts.generate_missing_normals = true;
		return Opts;
	}

	FMatrix EvaluateNodeToWorld(ufbx_anim* Anim, ufbx_node* Node, double InTime)
	{
		TArray<ufbx_node*> Chain;
		for (ufbx_node* N = Node; N != nullptr; N = N->parent)
		{
			Chain.Add(N);
		}
		FMatrix World = FMatrix::Identity;
		for (int32 Index = Chain.Num() - 1; Index >= 0; --Index)
		{
			// child.node_to_world = parent.node_to_world * child.node_to_parent
			World = ToMatrix(ufbx_evaluate_transform(Anim, Chain[Index], InTime)) * World;
		}
		return World;
	}

	using FNodesByName = TMap<FString, ufbx_node*>;

	bool BakeAnimFromScene(
		ufbx_scene* Scene, const USkeleton& InSkeleton, const FNodesByName& NodesByName, UAnimSequence& Out)
	{
		if (Scene == nullptr || InSkeleton.BoneCount() <= 0)
		{
			return false;
		}

		// Mixamo often ships a short static "Take 001" as scene->anim plus the real clip
		// on another stack (e.g. "mixamo.com"). Prefer the longest stack.
		ufbx_anim* Anim = Scene->anim;
		double Begin = (Anim != nullptr) ? Anim->time_begin : 0.0;
		double End = (Anim != nullptr) ? Anim->time_end : 0.0;
		for (size_t I = 0; I < Scene->anim_stacks.count; ++I)
		{
			ufbx_anim_stack* Stack = Scene->anim_stacks.data[I];
			if (Stack == nullptr || Stack->anim == nullptr)
			{
				continue;
			}
			const double StackDur = Stack->time_end - Stack->time_begin;
			if (StackDur > (End - Begin) + 1.0e-4)
			{
				Anim = Stack->anim;
				Begin = Stack->time_begin;
				End = Stack->time_end;
			}
		}
		if (Anim == nullptr)
		{
			return false;
		}
		if (End <= Begin + 1.0e-4)
		{
			End = Begin + 1.0;
		}

		constexpr float Fps = 30.0f;
		const float Duration = static_cast<float>(End - Begin);
		const int32 LocalFrameCount = FMath::Max(2, FMath::CeilToInt(Duration * Fps) + 1);

		Out.Name = FName("clip");
		Out.DurationSeconds = Duration;
		Out.FramesPerSecond = Fps;
		Out.LocalPoseFrames.SetNum(LocalFrameCount);

		for (int32 F = 0; F < LocalFrameCount; ++F)
		{
			const double T =
				Begin + (static_cast<double>(F) / static_cast<double>(LocalFrameCount - 1)) * (End - Begin);
			TArray<FMatrix>& Frame = Out.LocalPoseFrames[F];
			Frame.Init(FMatrix::Identity, InSkeleton.BoneCount());
			for (int32 B = 0; B < InSkeleton.BoneCount(); ++B)
			{
				ufbx_node* const* Node = NodesByName.Find(InSkeleton.BoneNames[B].ToString());
				if (Node == nullptr || *Node == nullptr)
				{
					continue;
				}
				// Bake full node_to_world so skinning does not depend on cluster-only parents.
				Frame[B] = EvaluateNodeToWorld(Anim, *Node, T);
			}
		}
		return LocalFrameCount > 0;
	}

	void CollectNodesByName(ufbx_scene* Scene, FNodesByName& Out)
	{
		Out.Empty();
		for (size_t I = 0; I < Scene->nodes.count; ++I)
		{
			ufbx_node* Node = Scene->nodes.data[I];
			if (Node == nullptr || Node->name.data == nullptr || Node->name.length == 0)
			{
				continue;
			}
			Out.Add(FString(static_cast<int32>(Node->name.length), Node->name.data), Node);
		}
	}

} // namespace

bool LoadSkeletalMeshFromFbx(const FString& Path, FSkeletalMeshData& Out)
{
	Out = FSkeletalMeshData();
	ufbx_error Error{};
	const ufbx_load_opts Opts = MakeLoadOpts();
	ufbx_scene* Scene = ufbx_load_file(TCHAR_TO_UTF8(*Path), &Opts, &Error);
	if (Scene == nullptr)
	{
		UE_LOG(LogMeshUtilities, Error, "ufbx: failed to load mesh FBX '%s': %s", *Path, Error.description.data);
		return false;
	}

	ufbx_mesh* Mesh = nullptr;
	ufbx_skin_deformer* Skin = nullptr;
	for (size_t I = 0; I < Scene->meshes.count; ++I)
	{
		ufbx_mesh* Candidate = Scene->meshes.data[I];
		if (Candidate != nullptr && Candidate->skin_deformers.count > 0)
		{
			Mesh = Candidate;
			Skin = Candidate->skin_deformers.data[0];
			break;
		}
	}
	if (Mesh == nullptr || Skin == nullptr)
	{
		UE_LOG(LogMeshUtilities, Error, "ufbx: no skinned mesh in '%s'", *Path);
		ufbx_free_scene(Scene);
		return false;
	}

	const int32 ClusterCount = static_cast<int32>(Skin->clusters.count);
	if (ClusterCount <= 0 || ClusterCount > MaxSkinBones)
	{
		UE_LOG(LogMeshUtilities, Error, "ufbx: invalid bone count %d in '%s'", ClusterCount, *Path);
		ufbx_free_scene(Scene);
		return false;
	}

	Out.Skeleton.BoneNames.SetNum(ClusterCount);
	Out.Skeleton.ParentIndices.Init(INDEX_NONE, ClusterCount);
	Out.Skeleton.InverseBindPose.Init(FMatrix::Identity, ClusterCount);

	TMap<ufbx_node*, int32> NodeToBone;
	for (int32 C = 0; C < ClusterCount; ++C)
	{
		ufbx_skin_cluster* Cluster = Skin->clusters.data[C];
		if (Cluster == nullptr || Cluster->bone_node == nullptr)
		{
			continue;
		}
		ufbx_node* Bone = Cluster->bone_node;
		const FString LocalName(static_cast<int32>(Bone->name.length), Bone->name.data);
		Out.Skeleton.BoneNames[C] = FName(*LocalName);
		Out.Skeleton.InverseBindPose[C] = ToMatrix(Cluster->geometry_to_bone);
		NodeToBone.Add(Bone, C);
	}
	for (int32 C = 0; C < ClusterCount; ++C)
	{
		ufbx_skin_cluster* Cluster = Skin->clusters.data[C];
		if (Cluster == nullptr || Cluster->bone_node == nullptr)
		{
			continue;
		}
		ufbx_node* Parent = Cluster->bone_node->parent;
		while (Parent != nullptr)
		{
			if (const int32* ParentBone = NodeToBone.Find(Parent))
			{
				Out.Skeleton.ParentIndices[C] = *ParentBone;
				break;
			}
			Parent = Parent->parent;
		}
	}

	// Triangulate into unique vertices (per corner attributes).
	// ufbx_triangulate_face returns the number of *triangles* (not indices).
	Out.LocalMin = FVector(TNumericLimits<float>::Max());
	Out.LocalMax = FVector(TNumericLimits<float>::Lowest());

	TArray<uint32> Tri;
	Tri.SetNumZeroed(static_cast<int32>(FMath::Max<size_t>(Mesh->max_face_triangles * 3u, 16u * 3u)));

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
				const uint32 Index = Tri[static_cast<int32>(T * 3u) + K];
				const uint32 Vi = Mesh->vertex_indices.data[Index];

				FSkeletalVertex V{};
				const ufbx_vec3 Pos = ufbx_get_vertex_vec3(&Mesh->vertex_position, Index);
				V.Position = FVector(static_cast<float>(Pos.x), static_cast<float>(Pos.y), static_cast<float>(Pos.z));
				if (Mesh->vertex_normal.exists)
				{
					const ufbx_vec3 N = ufbx_get_vertex_vec3(&Mesh->vertex_normal, Index);
					V.Normal = FVector(static_cast<float>(N.x), static_cast<float>(N.y), static_cast<float>(N.z))
								   .GetUnsafeNormal();
				}
				if (Mesh->vertex_uv.exists)
				{
					const ufbx_vec2 Uv = ufbx_get_vertex_vec2(&Mesh->vertex_uv, Index);
					V.TexCoord = FVector2D(static_cast<float>(Uv.x), static_cast<float>(Uv.y));
				}

				// Skin weights (up to 4).
				if (Vi < Skin->vertices.count)
				{
					const ufbx_skin_vertex Sv = Skin->vertices.data[Vi];
					float Wsum = 0.0f;
					const uint32 Nw = FMath::Min<uint32>(Sv.num_weights, MaxBoneInfluences);
					for (uint32 Wi = 0; Wi < Nw; ++Wi)
					{
						const ufbx_skin_weight Sw = Skin->weights.data[Sv.weight_begin + Wi];
						V.BoneIndices[static_cast<int32>(Wi)] = static_cast<int32>(Sw.cluster_index);
						V.BoneWeights[static_cast<int32>(Wi)] = static_cast<float>(Sw.weight);
						Wsum += static_cast<float>(Sw.weight);
					}
					if (Wsum > 1.0e-6f)
					{
						for (int32 C = 0; C < MaxBoneInfluences; ++C)
						{
							V.BoneWeights[C] /= Wsum;
						}
					}
					else
					{
						V.BoneWeights[0] = 1.0f;
					}
				}
				else
				{
					V.BoneWeights[0] = 1.0f;
				}

				Out.LocalMin = Out.LocalMin.ComponentMin(V.Position);
				Out.LocalMax = Out.LocalMax.ComponentMax(V.Position);
				Out.Indices.Add(static_cast<uint32>(Out.Vertices.Num()));
				Out.Vertices.Add(V);
			}
		}
	}

	if (Out.IsEmpty())
	{
		UE_LOG(LogMeshUtilities, Error, "ufbx: skinned mesh produced no triangles in '%s'", *Path);
		ufbx_free_scene(Scene);
		return false;
	}

	FNodesByName NodesByName;
	CollectNodesByName(Scene, NodesByName);
	BakeAnimFromScene(Scene, Out.Skeleton, NodesByName, Out.EmbeddedAnim);

	ufbx_free_scene(Scene);
	return Out.Skeleton.BoneCount() > 0;
}

bool LoadAnimSequenceFromFbx(const FString& Path, const USkeleton& InSkeleton, UAnimSequence& Out)
{
	Out = UAnimSequence();
	ufbx_error Error{};
	const ufbx_load_opts Opts = MakeLoadOpts();
	ufbx_scene* Scene = ufbx_load_file(TCHAR_TO_UTF8(*Path), &Opts, &Error);
	if (Scene == nullptr)
	{
		UE_LOG(LogMeshUtilities, Error, "ufbx: failed to load anim FBX '%s': %s", *Path, Error.description.data);
		return false;
	}

	FNodesByName NodesByName;
	CollectNodesByName(Scene, NodesByName);
	const bool bOk = BakeAnimFromScene(Scene, InSkeleton, NodesByName, Out);
	if (bOk)
	{
		// Prefer the file name stem as the clip name.
		Out.Name = FName(*FPaths::GetBaseFilename(Path));
	}
	ufbx_free_scene(Scene);
	return bOk;
}
