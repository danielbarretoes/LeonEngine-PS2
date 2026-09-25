#include "FbxSkeletalImport.h"

#include "Migration/GlmInterop.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <ufbx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

	glm::mat4 ToGlm(const ufbx_transform& T)
	{
		const glm::quat Q(static_cast<float>(T.rotation.w), static_cast<float>(T.rotation.x),
			static_cast<float>(T.rotation.y), static_cast<float>(T.rotation.z));
		glm::mat4 M = glm::mat4_cast(Q);
		M[0] *= static_cast<float>(T.scale.x);
		M[1] *= static_cast<float>(T.scale.y);
		M[2] *= static_cast<float>(T.scale.z);
		M[3] = glm::vec4(static_cast<float>(T.translation.x), static_cast<float>(T.translation.y),
			static_cast<float>(T.translation.z), 1.0f);
		return M;
	}

	glm::mat4 ToGlm(const ufbx_matrix& M)
	{
		glm::mat4 Out(1.0f);
		Out[0] = glm::vec4(static_cast<float>(M.m00), static_cast<float>(M.m10), static_cast<float>(M.m20), 0.0f);
		Out[1] = glm::vec4(static_cast<float>(M.m01), static_cast<float>(M.m11), static_cast<float>(M.m21), 0.0f);
		Out[2] = glm::vec4(static_cast<float>(M.m02), static_cast<float>(M.m12), static_cast<float>(M.m22), 0.0f);
		Out[3] = glm::vec4(static_cast<float>(M.m03), static_cast<float>(M.m13), static_cast<float>(M.m23), 1.0f);
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

	glm::mat4 EvaluateNodeToWorld(ufbx_anim* Anim, ufbx_node* Node, double InTime)
	{
		std::vector<ufbx_node*> Chain;
		for (ufbx_node* N = Node; N != nullptr; N = N->parent)
		{
			Chain.push_back(N);
		}
		glm::mat4 World(1.0f);
		for (auto It = Chain.rbegin(); It != Chain.rend(); ++It)
		{
			// child.node_to_world = parent.node_to_world * child.node_to_parent
			World *= ToGlm(ufbx_evaluate_transform(Anim, *It, InTime));
		}
		return World;
	}

	bool BakeAnimFromScene(ufbx_scene* Scene, const USkeleton& InSkeleton,
		const std::unordered_map<std::string, ufbx_node*>& NodesByName, UAnimSequence& Out)
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
		const int LocalFrameCount = std::max(2, static_cast<int>(std::ceil(Duration * Fps)) + 1);

		Out.Name = FName("clip");
		Out.DurationSeconds = Duration;
		Out.FramesPerSecond = Fps;
		Out.LocalPoseFrames.SetNum(LocalFrameCount);

		for (int F = 0; F < LocalFrameCount; ++F)
		{
			const double T =
				Begin + (static_cast<double>(F) / static_cast<double>(LocalFrameCount - 1)) * (End - Begin);
			TArray<FMatrix>& Frame = Out.LocalPoseFrames[F];
			Frame.Init(FMatrix::Identity, InSkeleton.BoneCount());
			for (int32 B = 0; B < InSkeleton.BoneCount(); ++B)
			{
				const auto It = NodesByName.find(std::string(*InSkeleton.BoneNames[B].ToString()));
				if (It == NodesByName.end() || It->second == nullptr)
				{
					continue;
				}
				// Bake full node_to_world so skinning does not depend on cluster-only parents.
				Frame[B] = FromGlm(EvaluateNodeToWorld(Anim, It->second, T));
			}
		}
		return LocalFrameCount > 0;
	}

	void CollectNodesByName(ufbx_scene* Scene, std::unordered_map<std::string, ufbx_node*>& Out)
	{
		Out.clear();
		for (size_t I = 0; I < Scene->nodes.count; ++I)
		{
			ufbx_node* Node = Scene->nodes.data[I];
			if (Node == nullptr || Node->name.data == nullptr || Node->name.length == 0)
			{
				continue;
			}
			Out[std::string(Node->name.data, Node->name.length)] = Node;
		}
	}

} // namespace

bool LoadSkeletalMeshFromFbx(const std::string& Path, FSkeletalMeshData& Out)
{
	Out = {};
	ufbx_error Error{};
	const ufbx_load_opts Opts = MakeLoadOpts();
	ufbx_scene* Scene = ufbx_load_file(Path.c_str(), &Opts, &Error);
	if (Scene == nullptr)
	{
		std::cerr << "ufbx: failed to load mesh FBX '" << Path << "': " << Error.description.data << '\n';
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
		std::cerr << "ufbx: no skinned mesh in '" << Path << "'\n";
		ufbx_free_scene(Scene);
		return false;
	}

	const int ClusterCount = static_cast<int>(Skin->clusters.count);
	if (ClusterCount <= 0 || ClusterCount > MaxSkinBones)
	{
		std::cerr << "ufbx: invalid bone count " << ClusterCount << " in '" << Path << "'\n";
		ufbx_free_scene(Scene);
		return false;
	}

	Out.Skeleton.BoneNames.SetNum(ClusterCount);
	Out.Skeleton.ParentIndices.Init(INDEX_NONE, ClusterCount);
	Out.Skeleton.InverseBindPose.Init(FMatrix::Identity, ClusterCount);

	std::unordered_map<ufbx_node*, int> NodeToBone;
	for (int C = 0; C < ClusterCount; ++C)
	{
		ufbx_skin_cluster* Cluster = Skin->clusters.data[C];
		if (Cluster == nullptr || Cluster->bone_node == nullptr)
		{
			continue;
		}
		ufbx_node* Bone = Cluster->bone_node;
		const std::string LocalName(Bone->name.data, Bone->name.length);
		Out.Skeleton.BoneNames[C] = FName(LocalName.c_str());
		Out.Skeleton.InverseBindPose[C] = FromGlm(ToGlm(Cluster->geometry_to_bone));
		NodeToBone[Bone] = C;
	}
	for (int C = 0; C < ClusterCount; ++C)
	{
		ufbx_skin_cluster* Cluster = Skin->clusters.data[C];
		if (Cluster == nullptr || Cluster->bone_node == nullptr)
		{
			continue;
		}
		ufbx_node* Parent = Cluster->bone_node->parent;
		while (Parent != nullptr)
		{
			const auto It = NodeToBone.find(Parent);
			if (It != NodeToBone.end())
			{
				Out.Skeleton.ParentIndices[C] = It->second;
				break;
			}
			Parent = Parent->parent;
		}
	}

	// Triangulate into unique vertices (per corner attributes).
	// `ufbx_triangulate_face` returns the number of *triangles* (not indices).
	Out.LocalMin = FVector(TNumericLimits<float>::Max());
	Out.LocalMax = FVector(TNumericLimits<float>::Lowest());

	const size_t TriIndexCapacity = std::max<size_t>(Mesh->max_face_triangles * 3u, 16u * 3u);
	std::vector<uint32_t> Tri(TriIndexCapacity);

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
				const uint32_t Index = Tri[static_cast<size_t>(T) * 3u + static_cast<size_t>(K)];
				const uint32_t Vi = Mesh->vertex_indices.data[Index];

				FSkeletalVertex V{};
				const ufbx_vec3 Pos = ufbx_get_vertex_vec3(&Mesh->vertex_position, Index);
				V.Position = {static_cast<float>(Pos.x), static_cast<float>(Pos.y), static_cast<float>(Pos.z)};
				if (Mesh->vertex_normal.exists)
				{
					const ufbx_vec3 N = ufbx_get_vertex_vec3(&Mesh->vertex_normal, Index);
					V.Normal = FromGlm(glm::normalize(
						glm::vec3{static_cast<float>(N.x), static_cast<float>(N.y), static_cast<float>(N.z)}));
				}
				if (Mesh->vertex_uv.exists)
				{
					const ufbx_vec2 Uv = ufbx_get_vertex_vec2(&Mesh->vertex_uv, Index);
					V.TexCoord = {static_cast<float>(Uv.x), static_cast<float>(Uv.y)};
				}

				// Skin weights (up to 4).
				if (Vi < Skin->vertices.count)
				{
					const ufbx_skin_vertex Sv = Skin->vertices.data[Vi];
					float Wsum = 0.0f;
					const uint32_t Nw = std::min<uint32_t>(Sv.num_weights, MaxBoneInfluences);
					for (uint32_t Wi = 0; Wi < Nw; ++Wi)
					{
						const ufbx_skin_weight Sw = Skin->weights.data[Sv.weight_begin + Wi];
						V.BoneIndices[Wi] = static_cast<int>(Sw.cluster_index);
						V.BoneWeights[Wi] = static_cast<float>(Sw.weight);
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
		std::cerr << "ufbx: skinned mesh produced no triangles in '" << Path << "'\n";
		ufbx_free_scene(Scene);
		return false;
	}

	std::unordered_map<std::string, ufbx_node*> NodesByName;
	CollectNodesByName(Scene, NodesByName);
	BakeAnimFromScene(Scene, Out.Skeleton, NodesByName, Out.EmbeddedAnim);

	ufbx_free_scene(Scene);
	return Out.Skeleton.BoneCount() > 0;
}

bool LoadAnimSequenceFromFbx(const std::string& Path, const USkeleton& InSkeleton, UAnimSequence& Out)
{
	Out = {};
	ufbx_error Error{};
	const ufbx_load_opts Opts = MakeLoadOpts();
	ufbx_scene* Scene = ufbx_load_file(Path.c_str(), &Opts, &Error);
	if (Scene == nullptr)
	{
		std::cerr << "ufbx: failed to load anim FBX '" << Path << "': " << Error.description.data << '\n';
		return false;
	}

	std::unordered_map<std::string, ufbx_node*> NodesByName;
	CollectNodesByName(Scene, NodesByName);
	const bool bOk = BakeAnimFromScene(Scene, InSkeleton, NodesByName, Out);
	if (bOk)
	{
		// Prefer filename stem as clip name.
		const auto Slash = Path.find_last_of("/\\");
		const auto Dot = Path.find_last_of('.');
		const std::size_t Start = Slash == std::string::npos ? 0 : Slash + 1;
		const std::size_t End = (Dot == std::string::npos || Dot < Start) ? Path.size() : Dot;
		Out.Name = FName(Path.substr(Start, End - Start).c_str());
	}
	ufbx_free_scene(Scene);
	return bOk;
}
