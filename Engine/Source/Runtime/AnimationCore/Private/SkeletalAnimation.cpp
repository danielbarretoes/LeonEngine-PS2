#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include "SkeletalAnimation.h"
#include <limits>
#include <ufbx.h>
#include <unordered_map> // IWYU pragma: keep — used below; include-cleaner false positive

namespace {

glm::mat4 ToGlm(const ufbx_transform& T) {
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

glm::mat4 ToGlm(const ufbx_matrix& M) {
    glm::mat4 Out(1.0f);
    Out[0] = glm::vec4(static_cast<float>(M.m00), static_cast<float>(M.m10),
                       static_cast<float>(M.m20), 0.0f);
    Out[1] = glm::vec4(static_cast<float>(M.m01), static_cast<float>(M.m11),
                       static_cast<float>(M.m21), 0.0f);
    Out[2] = glm::vec4(static_cast<float>(M.m02), static_cast<float>(M.m12),
                       static_cast<float>(M.m22), 0.0f);
    Out[3] = glm::vec4(static_cast<float>(M.m03), static_cast<float>(M.m13),
                       static_cast<float>(M.m23), 1.0f);
    return Out;
}

ufbx_load_opts MakeLoadOpts() {
    ufbx_load_opts Opts{};
    Opts.target_axes = ufbx_axes_right_handed_y_up;
    Opts.target_unit_meters = 1.0f;
    Opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    Opts.generate_missing_normals = true;
    return Opts;
}

glm::mat4 EvaluateNodeToWorld(ufbx_anim* Anim, ufbx_node* Node, double InTime) {
    std::vector<ufbx_node*> Chain;
    for (ufbx_node* N = Node; N != nullptr; N = N->parent) {
        Chain.push_back(N);
    }
    glm::mat4 World(1.0f);
    for (auto It = Chain.rbegin(); It != Chain.rend(); ++It) {
        // child.node_to_world = parent.node_to_world * child.node_to_parent
        World *= ToGlm(ufbx_evaluate_transform(Anim, *It, InTime));
    }
    return World;
}

bool BakeAnimFromScene(ufbx_scene* Scene, const USkeleton& InSkeleton,
                       const std::unordered_map<std::string, ufbx_node*>& NodesByName,
                       UAnimSequence& Out) {
    if (Scene == nullptr || InSkeleton.BoneCount() <= 0) {
        return false;
    }

    // Mixamo often ships a short static "Take 001" as scene->anim plus the real clip
    // on another stack (e.g. "mixamo.com"). Prefer the longest stack.
    ufbx_anim* Anim = Scene->anim;
    double Begin = (Anim != nullptr) ? Anim->time_begin : 0.0;
    double End = (Anim != nullptr) ? Anim->time_end : 0.0;
    for (size_t I = 0; I < Scene->anim_stacks.count; ++I) {
        ufbx_anim_stack* Stack = Scene->anim_stacks.data[I];
        if (Stack == nullptr || Stack->anim == nullptr) {
            continue;
        }
        const double StackDur = Stack->time_end - Stack->time_begin;
        if (StackDur > (End - Begin) + 1.0e-4) {
            Anim = Stack->anim;
            Begin = Stack->time_begin;
            End = Stack->time_end;
        }
    }
    if (Anim == nullptr) {
        return false;
    }
    if (End <= Begin + 1.0e-4) {
        End = Begin + 1.0;
    }

    constexpr float Fps = 30.0f;
    const float Duration = static_cast<float>(End - Begin);
    const int LocalFrameCount = std::max(2, static_cast<int>(std::ceil(Duration * Fps)) + 1);

    Out.Name = "clip";
    Out.DurationSeconds = Duration;
    Out.FramesPerSecond = Fps;
    Out.LocalPoseFrames.resize(static_cast<std::size_t>(LocalFrameCount));

    for (int F = 0; F < LocalFrameCount; ++F) {
        const double T =
            Begin + (static_cast<double>(F) / static_cast<double>(LocalFrameCount - 1)) * (End - Begin);
        auto& Frame = Out.LocalPoseFrames[static_cast<std::size_t>(F)];
        Frame.resize(static_cast<std::size_t>(InSkeleton.BoneCount()), glm::mat4(1.0f));
        for (int B = 0; B < InSkeleton.BoneCount(); ++B) {
            const auto It = NodesByName.find(InSkeleton.BoneNames[static_cast<std::size_t>(B)]);
            if (It == NodesByName.end() || It->second == nullptr) {
                continue;
            }
            // Bake full node_to_world so skinning does not depend on cluster-only parents.
            Frame[static_cast<std::size_t>(B)] = EvaluateNodeToWorld(Anim, It->second, T);
        }
    }
    return LocalFrameCount > 0;
}

void CollectNodesByName(ufbx_scene* Scene, std::unordered_map<std::string, ufbx_node*>& Out) {
    Out.clear();
    for (size_t I = 0; I < Scene->nodes.count; ++I) {
        ufbx_node* Node = Scene->nodes.data[I];
        if (Node == nullptr || Node->name.data == nullptr || Node->name.length == 0) {
            continue;
        }
        Out[std::string(Node->name.data, Node->name.length)] = Node;
    }
}

} // namespace

int USkeleton::FindBoneIndex(const std::string& InName) const {
    for (int I = 0; I < BoneCount(); ++I) {
        if (BoneNames[static_cast<std::size_t>(I)] == InName) {
            return I;
        }
    }
    return -1;
}

bool UAnimSequence::IsFinished(float TimeSeconds) const {
    if (bLooping || DurationSeconds <= 1.0e-4f) {
        return false;
    }
    return TimeSeconds >= (DurationSeconds - 1.0e-4f);
}

void UAnimSequence::SampleLocalPose(float TimeSeconds, std::vector<glm::mat4>& OutBoneWorld) const {
    const int LocalBoneCount = FrameCount() > 0 ? static_cast<int>(LocalPoseFrames[0].size()) : 0;
    OutBoneWorld.assign(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
    if (LocalBoneCount <= 0 || FrameCount() <= 0) {
        return;
    }

    float T = TimeSeconds;
    if (DurationSeconds > 1.0e-4f) {
        if (bLooping) {
            T = std::fmod(T, DurationSeconds);
            if (T < 0.0f) {
                T += DurationSeconds;
            }
        } else {
            T = std::clamp(T, 0.0f, DurationSeconds);
        }
    }
    const float FrameF = T * FramesPerSecond;
    int F0 = 0;
    int F1 = 0;
    float Alpha = 0.0f;
    if (bLooping) {
        F0 = static_cast<int>(FrameF) % FrameCount();
        F1 = (F0 + 1) % FrameCount();
        Alpha = FrameF - std::floor(FrameF);
    } else {
        const float MaxFrame = static_cast<float>(FrameCount() - 1);
        const float Clamped = std::min(FrameF, MaxFrame);
        F0 = static_cast<int>(Clamped);
        F1 = std::min(F0 + 1, FrameCount() - 1);
        Alpha = Clamped - std::floor(Clamped);
    }

    const auto& A = LocalPoseFrames[static_cast<std::size_t>(F0)];
    const auto& B = LocalPoseFrames[static_cast<std::size_t>(F1)];
    for (int I = 0; I < LocalBoneCount; ++I) {
        // Matrix lerp is approximate but fine for a micro blend-space / crossfade.
        OutBoneWorld[static_cast<std::size_t>(I)] =
            A[static_cast<std::size_t>(I)] * (1.0f - Alpha) +
            B[static_cast<std::size_t>(I)] * Alpha;
    }
}

void UBlendSpace1D::Evaluate(float AxisValue, const UAnimSequence*& OutA, const UAnimSequence*& OutB,
                            float& OutAlpha) const {
    OutA = nullptr;
    OutB = nullptr;
    OutAlpha = 0.0f;
    if (Samples.empty()) {
        return;
    }

    // Sort indices by sample position (stable for typical Idle@0 / Run@1 authoring).
    std::vector<std::size_t> Order(Samples.size());
    for (std::size_t I = 0; I < Samples.size(); ++I) {
        Order[I] = I;
    }
    std::sort(Order.begin(), Order.end(), [this](std::size_t A, std::size_t B) {
        return Samples[A].Position < Samples[B].Position;
    });

    const float X = std::clamp(AxisValue, AxisMin, AxisMax);
    const FBlendSample& First = Samples[Order.front()];
    const FBlendSample& Last = Samples[Order.back()];
    if (X <= First.Position || Order.size() == 1) {
        OutA = First.Sequence;
        OutB = First.Sequence;
        OutAlpha = 0.0f;
        return;
    }
    if (X >= Last.Position) {
        OutA = Last.Sequence;
        OutB = Last.Sequence;
        OutAlpha = 0.0f;
        return;
    }

    for (std::size_t I = 0; I + 1 < Order.size(); ++I) {
        const FBlendSample& A = Samples[Order[I]];
        const FBlendSample& B = Samples[Order[I + 1]];
        if (X >= A.Position && X <= B.Position) {
            OutA = A.Sequence;
            OutB = B.Sequence;
            const float Span = B.Position - A.Position;
            OutAlpha = (Span > 1.0e-6f) ? ((X - A.Position) / Span) : 0.0f;
            return;
        }
    }
}

void UAnimInstance::SetBlendSpaceInput(float AxisValue) {
    BlendInputTarget = AxisValue;
}

void UAnimInstance::UpdateLocomotion(float DeltaTime) {
    if (LocomotionBlendInterpSpeed <= 1.0e-6f) {
        BlendInput = BlendInputTarget;
    } else {
        const float T = 1.0f - std::exp(-LocomotionBlendInterpSpeed * DeltaTime);
        BlendInput += (BlendInputTarget - BlendInput) * T;
    }

    SampleA = nullptr;
    SampleB = nullptr;
    BlendAlpha = 0.0f;
    if (BlendSpace != nullptr) {
        BlendSpace->Evaluate(BlendInput, SampleA, SampleB, BlendAlpha);
    }
    if (SampleA != nullptr) {
        TimeA += DeltaTime;
    }
    if (SampleB != nullptr && SampleB != SampleA) {
        TimeB += DeltaTime;
    } else if (SampleB == SampleA) {
        TimeB = TimeA;
    }
}

void UAnimInstance::SampleLocomotionBoneWorld(std::vector<glm::mat4>& OutBoneWorld) const {
    OutBoneWorld.clear();
    if (Skeleton == nullptr) {
        return;
    }
    const int LocalBoneCount = Skeleton->BoneCount();
    const bool bHasA = SampleA != nullptr && SampleA->FrameCount() > 0;
    const bool bHasB = SampleB != nullptr && SampleB->FrameCount() > 0;
    if (!bHasA && !bHasB) {
        OutBoneWorld.assign(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
        return;
    }

    std::vector<glm::mat4> WorldA;
    std::vector<glm::mat4> WorldB;
    if (bHasA) {
        SampleA->SampleLocalPose(TimeA, WorldA);
    }
    if (bHasB) {
        SampleB->SampleLocalPose(TimeB, WorldB);
    }

    OutBoneWorld.resize(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
    for (int I = 0; I < LocalBoneCount; ++I) {
        const glm::mat4 A = (WorldA.size() == static_cast<std::size_t>(LocalBoneCount))
                                ? WorldA[static_cast<std::size_t>(I)]
                                : (WorldB.size() == static_cast<std::size_t>(LocalBoneCount)
                                       ? WorldB[static_cast<std::size_t>(I)]
                                       : glm::mat4(1.0f));
        const glm::mat4 B = (WorldB.size() == static_cast<std::size_t>(LocalBoneCount))
                                ? WorldB[static_cast<std::size_t>(I)]
                                : A;
        OutBoneWorld[static_cast<std::size_t>(I)] = A * (1.0f - BlendAlpha) + B * BlendAlpha;
    }
}

void UAnimInstance::SkinFromBoneWorld(const std::vector<glm::mat4>& BoneWorld,
                                     std::vector<glm::mat4>& OutSkin) const {
    OutSkin.clear();
    if (Skeleton == nullptr || Skeleton->BoneCount() <= 0) {
        return;
    }
    const int LocalBoneCount = Skeleton->BoneCount();
    if (BoneWorld.size() != static_cast<std::size_t>(LocalBoneCount)) {
        OutSkin.assign(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
        return;
    }
    OutSkin.resize(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
    for (int I = 0; I < LocalBoneCount; ++I) {
        OutSkin[static_cast<std::size_t>(I)] =
            BoneWorld[static_cast<std::size_t>(I)] *
            Skeleton->InverseBindPose[static_cast<std::size_t>(I)];
    }
}

void UAnimInstance::NativeUpdateAnimation(float DeltaTime) {
    UpdateLocomotion(DeltaTime);
}

void UAnimInstance::GetBoneWorldMatrices(std::vector<glm::mat4>& OutBoneWorld) const {
    SampleLocomotionBoneWorld(OutBoneWorld);
}

void UAnimInstance::GetSkinMatrices(std::vector<glm::mat4>& OutSkin) const {
    std::vector<glm::mat4> World;
    GetBoneWorldMatrices(World);
    SkinFromBoneWorld(World, OutSkin);
}

bool LoadSkeletalMeshFromFbx(const std::string& Path, FSkeletalMeshData& Out) {
    Out = {};
    ufbx_error Error{};
    const ufbx_load_opts Opts = MakeLoadOpts();
    ufbx_scene* Scene = ufbx_load_file(Path.c_str(), &Opts, &Error);
    if (Scene == nullptr) {
        std::cerr << "ufbx: failed to load mesh FBX '" << Path << "': " << Error.description.data
                  << '\n';
        return false;
    }

    ufbx_mesh* Mesh = nullptr;
    ufbx_skin_deformer* Skin = nullptr;
    for (size_t I = 0; I < Scene->meshes.count; ++I) {
        ufbx_mesh* Candidate = Scene->meshes.data[I];
        if (Candidate != nullptr && Candidate->skin_deformers.count > 0) {
            Mesh = Candidate;
            Skin = Candidate->skin_deformers.data[0];
            break;
        }
    }
    if (Mesh == nullptr || Skin == nullptr) {
        std::cerr << "ufbx: no skinned mesh in '" << Path << "'\n";
        ufbx_free_scene(Scene);
        return false;
    }

    const int ClusterCount = static_cast<int>(Skin->clusters.count);
    if (ClusterCount <= 0 || ClusterCount > MaxSkinBones) {
        std::cerr << "ufbx: invalid bone count " << ClusterCount << " in '" << Path << "'\n";
        ufbx_free_scene(Scene);
        return false;
    }

    Out.Skeleton.BoneNames.resize(static_cast<std::size_t>(ClusterCount));
    Out.Skeleton.ParentIndices.assign(static_cast<std::size_t>(ClusterCount), -1);
    Out.Skeleton.InverseBindPose.resize(static_cast<std::size_t>(ClusterCount), glm::mat4(1.0f));

    std::unordered_map<ufbx_node*, int> NodeToBone;
    for (int C = 0; C < ClusterCount; ++C) {
        ufbx_skin_cluster* Cluster = Skin->clusters.data[C];
        if (Cluster == nullptr || Cluster->bone_node == nullptr) {
            continue;
        }
        ufbx_node* Bone = Cluster->bone_node;
        const std::string LocalName(Bone->name.data, Bone->name.length);
        Out.Skeleton.BoneNames[static_cast<std::size_t>(C)] = LocalName;
        Out.Skeleton.InverseBindPose[static_cast<std::size_t>(C)] =
            ToGlm(Cluster->geometry_to_bone);
        NodeToBone[Bone] = C;
    }
    for (int C = 0; C < ClusterCount; ++C) {
        ufbx_skin_cluster* Cluster = Skin->clusters.data[C];
        if (Cluster == nullptr || Cluster->bone_node == nullptr) {
            continue;
        }
        ufbx_node* Parent = Cluster->bone_node->parent;
        while (Parent != nullptr) {
            const auto It = NodeToBone.find(Parent);
            if (It != NodeToBone.end()) {
                Out.Skeleton.ParentIndices[static_cast<std::size_t>(C)] = It->second;
                break;
            }
            Parent = Parent->parent;
        }
    }

    // Triangulate into unique vertices (per corner attributes).
    // `ufbx_triangulate_face` returns the number of *triangles* (not indices).
    Out.LocalMin = glm::vec3(std::numeric_limits<float>::max());
    Out.LocalMax = glm::vec3(std::numeric_limits<float>::lowest());

    const size_t TriIndexCapacity = std::max<size_t>(Mesh->max_face_triangles * 3u, 16u * 3u);
    std::vector<uint32_t> Tri(TriIndexCapacity);

    for (size_t Fi = 0; Fi < Mesh->faces.count; ++Fi) {
        const ufbx_face Face = Mesh->faces.data[Fi];
        if (Face.num_indices < 3) {
            continue;
        }
        const uint32_t NumTris = ufbx_triangulate_face(Tri.data(), Tri.size(), Mesh, Face);
        for (uint32_t T = 0; T < NumTris; ++T) {
            for (int K = 0; K < 3; ++K) {
                const uint32_t Index = Tri[static_cast<size_t>(T) * 3u + static_cast<size_t>(K)];
                const uint32_t Vi = Mesh->vertex_indices.data[Index];

                FSkeletalVertex V{};
                const ufbx_vec3 Pos = ufbx_get_vertex_vec3(&Mesh->vertex_position, Index);
                V.Position = {static_cast<float>(Pos.x), static_cast<float>(Pos.y),
                              static_cast<float>(Pos.z)};
                if (Mesh->vertex_normal.exists) {
                    const ufbx_vec3 N = ufbx_get_vertex_vec3(&Mesh->vertex_normal, Index);
                    V.Normal = glm::normalize(glm::vec3{
                        static_cast<float>(N.x), static_cast<float>(N.y), static_cast<float>(N.z)});
                }
                if (Mesh->vertex_uv.exists) {
                    const ufbx_vec2 Uv = ufbx_get_vertex_vec2(&Mesh->vertex_uv, Index);
                    V.TexCoord = {static_cast<float>(Uv.x), static_cast<float>(Uv.y)};
                }

                // Skin weights (up to 4).
                if (Vi < Skin->vertices.count) {
                    const ufbx_skin_vertex Sv = Skin->vertices.data[Vi];
                    float Wsum = 0.0f;
                    const uint32_t Nw = std::min<uint32_t>(Sv.num_weights, MaxBoneInfluences);
                    for (uint32_t Wi = 0; Wi < Nw; ++Wi) {
                        const ufbx_skin_weight Sw = Skin->weights.data[Sv.weight_begin + Wi];
                        V.BoneIndices[Wi] = static_cast<int>(Sw.cluster_index);
                        V.BoneWeights[Wi] = static_cast<float>(Sw.weight);
                        Wsum += static_cast<float>(Sw.weight);
                    }
                    if (Wsum > 1.0e-6f) {
                        V.BoneWeights /= Wsum;
                    } else {
                        V.BoneWeights[0] = 1.0f;
                    }
                } else {
                    V.BoneWeights[0] = 1.0f;
                }

                Out.LocalMin = glm::min(Out.LocalMin, V.Position);
                Out.LocalMax = glm::max(Out.LocalMax, V.Position);
                Out.Indices.push_back(static_cast<std::uint32_t>(Out.Vertices.size()));
                Out.Vertices.push_back(V);
            }
        }
    }

    if (Out.empty()) {
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

bool LoadAnimSequenceFromFbx(const std::string& Path, const USkeleton& InSkeleton, UAnimSequence& Out) {
    Out = {};
    ufbx_error Error{};
    const ufbx_load_opts Opts = MakeLoadOpts();
    ufbx_scene* Scene = ufbx_load_file(Path.c_str(), &Opts, &Error);
    if (Scene == nullptr) {
        std::cerr << "ufbx: failed to load anim FBX '" << Path << "': " << Error.description.data
                  << '\n';
        return false;
    }

    std::unordered_map<std::string, ufbx_node*> NodesByName;
    CollectNodesByName(Scene, NodesByName);
    const bool bOk = BakeAnimFromScene(Scene, InSkeleton, NodesByName, Out);
    if (bOk) {
        // Prefer filename stem as clip name.
        const auto Slash = Path.find_last_of("/\\");
        const auto Dot = Path.find_last_of('.');
        const std::size_t Start = Slash == std::string::npos ? 0 : Slash + 1;
        const std::size_t End = (Dot == std::string::npos || Dot < Start) ? Path.size() : Dot;
        Out.Name = Path.substr(Start, End - Start);
    }
    ufbx_free_scene(Scene);
    return bOk;
}

