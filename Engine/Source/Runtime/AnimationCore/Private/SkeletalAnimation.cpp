#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <leon/animation/SkeletalAnimation.h>
#include <limits>
#include <ufbx.h>
#include <unordered_map> // IWYU pragma: keep — used below; include-cleaner false positive

namespace leon {
namespace {

glm::mat4 ToGlm(const ufbx_transform& t) {
    const glm::quat q(static_cast<float>(t.rotation.w), static_cast<float>(t.rotation.x),
                      static_cast<float>(t.rotation.y), static_cast<float>(t.rotation.z));
    glm::mat4 m = glm::mat4_cast(q);
    m[0] *= static_cast<float>(t.scale.x);
    m[1] *= static_cast<float>(t.scale.y);
    m[2] *= static_cast<float>(t.scale.z);
    m[3] = glm::vec4(static_cast<float>(t.translation.x), static_cast<float>(t.translation.y),
                     static_cast<float>(t.translation.z), 1.0f);
    return m;
}

glm::mat4 ToGlm(const ufbx_matrix& m) {
    glm::mat4 out(1.0f);
    out[0] = glm::vec4(static_cast<float>(m.m00), static_cast<float>(m.m10),
                       static_cast<float>(m.m20), 0.0f);
    out[1] = glm::vec4(static_cast<float>(m.m01), static_cast<float>(m.m11),
                       static_cast<float>(m.m21), 0.0f);
    out[2] = glm::vec4(static_cast<float>(m.m02), static_cast<float>(m.m12),
                       static_cast<float>(m.m22), 0.0f);
    out[3] = glm::vec4(static_cast<float>(m.m03), static_cast<float>(m.m13),
                       static_cast<float>(m.m23), 1.0f);
    return out;
}

ufbx_load_opts MakeLoadOpts() {
    ufbx_load_opts opts{};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;
    opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    opts.generate_missing_normals = true;
    return opts;
}

glm::mat4 EvaluateNodeToWorld(ufbx_anim* anim, ufbx_node* node, double time) {
    std::vector<ufbx_node*> chain;
    for (ufbx_node* n = node; n != nullptr; n = n->parent) {
        chain.push_back(n);
    }
    glm::mat4 world(1.0f);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        // child.node_to_world = parent.node_to_world * child.node_to_parent
        world *= ToGlm(ufbx_evaluate_transform(anim, *it, time));
    }
    return world;
}

bool BakeAnimFromScene(ufbx_scene* scene, const Skeleton& skeleton,
                       const std::unordered_map<std::string, ufbx_node*>& nodesByName,
                       AnimSequence& out) {
    if (scene == nullptr || skeleton.BoneCount() <= 0) {
        return false;
    }

    // Mixamo often ships a short static "Take 001" as scene->anim plus the real clip
    // on another stack (e.g. "mixamo.com"). Prefer the longest stack.
    ufbx_anim* anim = scene->anim;
    double begin = (anim != nullptr) ? anim->time_begin : 0.0;
    double end = (anim != nullptr) ? anim->time_end : 0.0;
    for (size_t i = 0; i < scene->anim_stacks.count; ++i) {
        ufbx_anim_stack* stack = scene->anim_stacks.data[i];
        if (stack == nullptr || stack->anim == nullptr) {
            continue;
        }
        const double stackDur = stack->time_end - stack->time_begin;
        if (stackDur > (end - begin) + 1.0e-4) {
            anim = stack->anim;
            begin = stack->time_begin;
            end = stack->time_end;
        }
    }
    if (anim == nullptr) {
        return false;
    }
    if (end <= begin + 1.0e-4) {
        end = begin + 1.0;
    }

    constexpr float kFps = 30.0f;
    const float duration = static_cast<float>(end - begin);
    const int frameCount = std::max(2, static_cast<int>(std::ceil(duration * kFps)) + 1);

    out.name = "clip";
    out.durationSeconds = duration;
    out.framesPerSecond = kFps;
    out.localPoseFrames.resize(static_cast<std::size_t>(frameCount));

    for (int f = 0; f < frameCount; ++f) {
        const double t =
            begin + (static_cast<double>(f) / static_cast<double>(frameCount - 1)) * (end - begin);
        auto& frame = out.localPoseFrames[static_cast<std::size_t>(f)];
        frame.resize(static_cast<std::size_t>(skeleton.BoneCount()), glm::mat4(1.0f));
        for (int b = 0; b < skeleton.BoneCount(); ++b) {
            const auto it = nodesByName.find(skeleton.boneNames[static_cast<std::size_t>(b)]);
            if (it == nodesByName.end() || it->second == nullptr) {
                continue;
            }
            // Bake full node_to_world so skinning does not depend on cluster-only parents.
            frame[static_cast<std::size_t>(b)] = EvaluateNodeToWorld(anim, it->second, t);
        }
    }
    return frameCount > 0;
}

void CollectNodesByName(ufbx_scene* scene, std::unordered_map<std::string, ufbx_node*>& out) {
    out.clear();
    for (size_t i = 0; i < scene->nodes.count; ++i) {
        ufbx_node* node = scene->nodes.data[i];
        if (node == nullptr || node->name.data == nullptr || node->name.length == 0) {
            continue;
        }
        out[std::string(node->name.data, node->name.length)] = node;
    }
}

} // namespace

int Skeleton::FindBoneIndex(const std::string& name) const {
    for (int i = 0; i < BoneCount(); ++i) {
        if (boneNames[static_cast<std::size_t>(i)] == name) {
            return i;
        }
    }
    return -1;
}

bool AnimSequence::IsFinished(float timeSeconds) const {
    if (bLooping || durationSeconds <= 1.0e-4f) {
        return false;
    }
    return timeSeconds >= (durationSeconds - 1.0e-4f);
}

void AnimSequence::SampleLocalPose(float timeSeconds, std::vector<glm::mat4>& outBoneWorld) const {
    const int boneCount = FrameCount() > 0 ? static_cast<int>(localPoseFrames[0].size()) : 0;
    outBoneWorld.assign(static_cast<std::size_t>(boneCount), glm::mat4(1.0f));
    if (boneCount <= 0 || FrameCount() <= 0) {
        return;
    }

    float t = timeSeconds;
    if (durationSeconds > 1.0e-4f) {
        if (bLooping) {
            t = std::fmod(t, durationSeconds);
            if (t < 0.0f) {
                t += durationSeconds;
            }
        } else {
            t = std::clamp(t, 0.0f, durationSeconds);
        }
    }
    const float frameF = t * framesPerSecond;
    int f0 = 0;
    int f1 = 0;
    float alpha = 0.0f;
    if (bLooping) {
        f0 = static_cast<int>(frameF) % FrameCount();
        f1 = (f0 + 1) % FrameCount();
        alpha = frameF - std::floor(frameF);
    } else {
        const float maxFrame = static_cast<float>(FrameCount() - 1);
        const float clamped = std::min(frameF, maxFrame);
        f0 = static_cast<int>(clamped);
        f1 = std::min(f0 + 1, FrameCount() - 1);
        alpha = clamped - std::floor(clamped);
    }

    const auto& a = localPoseFrames[static_cast<std::size_t>(f0)];
    const auto& b = localPoseFrames[static_cast<std::size_t>(f1)];
    for (int i = 0; i < boneCount; ++i) {
        // Matrix lerp is approximate but fine for a micro blend-space / crossfade.
        outBoneWorld[static_cast<std::size_t>(i)] =
            a[static_cast<std::size_t>(i)] * (1.0f - alpha) +
            b[static_cast<std::size_t>(i)] * alpha;
    }
}

void BlendSpace1D::Evaluate(float axisValue, const AnimSequence*& outA, const AnimSequence*& outB,
                            float& outAlpha) const {
    outA = nullptr;
    outB = nullptr;
    outAlpha = 0.0f;
    if (samples.empty()) {
        return;
    }

    // Sort indices by sample position (stable for typical Idle@0 / Run@1 authoring).
    std::vector<std::size_t> order(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
        return samples[a].position < samples[b].position;
    });

    const float x = std::clamp(axisValue, axisMin, axisMax);
    const BlendSpace1DSample& first = samples[order.front()];
    const BlendSpace1DSample& last = samples[order.back()];
    if (x <= first.position || order.size() == 1) {
        outA = first.sequence;
        outB = first.sequence;
        outAlpha = 0.0f;
        return;
    }
    if (x >= last.position) {
        outA = last.sequence;
        outB = last.sequence;
        outAlpha = 0.0f;
        return;
    }

    for (std::size_t i = 0; i + 1 < order.size(); ++i) {
        const BlendSpace1DSample& a = samples[order[i]];
        const BlendSpace1DSample& b = samples[order[i + 1]];
        if (x >= a.position && x <= b.position) {
            outA = a.sequence;
            outB = b.sequence;
            const float span = b.position - a.position;
            outAlpha = (span > 1.0e-6f) ? ((x - a.position) / span) : 0.0f;
            return;
        }
    }
}

void AnimInstance::SetBlendSpaceInput(float axisValue) {
    blendInputTarget_ = axisValue;
}

void AnimInstance::UpdateLocomotion(float deltaTime) {
    if (locomotionBlendInterpSpeed_ <= 1.0e-6f) {
        blendInput_ = blendInputTarget_;
    } else {
        const float t = 1.0f - std::exp(-locomotionBlendInterpSpeed_ * deltaTime);
        blendInput_ += (blendInputTarget_ - blendInput_) * t;
    }

    sampleA_ = nullptr;
    sampleB_ = nullptr;
    blendAlpha_ = 0.0f;
    if (blendSpace_ != nullptr) {
        blendSpace_->Evaluate(blendInput_, sampleA_, sampleB_, blendAlpha_);
    }
    if (sampleA_ != nullptr) {
        timeA_ += deltaTime;
    }
    if (sampleB_ != nullptr && sampleB_ != sampleA_) {
        timeB_ += deltaTime;
    } else if (sampleB_ == sampleA_) {
        timeB_ = timeA_;
    }
}

void AnimInstance::SampleLocomotionBoneWorld(std::vector<glm::mat4>& outBoneWorld) const {
    outBoneWorld.clear();
    if (skeleton_ == nullptr) {
        return;
    }
    const int boneCount = skeleton_->BoneCount();
    const bool hasA = sampleA_ != nullptr && sampleA_->FrameCount() > 0;
    const bool hasB = sampleB_ != nullptr && sampleB_->FrameCount() > 0;
    if (!hasA && !hasB) {
        outBoneWorld.assign(static_cast<std::size_t>(boneCount), glm::mat4(1.0f));
        return;
    }

    std::vector<glm::mat4> worldA;
    std::vector<glm::mat4> worldB;
    if (hasA) {
        sampleA_->SampleLocalPose(timeA_, worldA);
    }
    if (hasB) {
        sampleB_->SampleLocalPose(timeB_, worldB);
    }

    outBoneWorld.resize(static_cast<std::size_t>(boneCount), glm::mat4(1.0f));
    for (int i = 0; i < boneCount; ++i) {
        const glm::mat4 a = (worldA.size() == static_cast<std::size_t>(boneCount))
                                ? worldA[static_cast<std::size_t>(i)]
                                : (worldB.size() == static_cast<std::size_t>(boneCount)
                                       ? worldB[static_cast<std::size_t>(i)]
                                       : glm::mat4(1.0f));
        const glm::mat4 b = (worldB.size() == static_cast<std::size_t>(boneCount))
                                ? worldB[static_cast<std::size_t>(i)]
                                : a;
        outBoneWorld[static_cast<std::size_t>(i)] = a * (1.0f - blendAlpha_) + b * blendAlpha_;
    }
}

void AnimInstance::SkinFromBoneWorld(const std::vector<glm::mat4>& boneWorld,
                                     std::vector<glm::mat4>& outSkin) const {
    outSkin.clear();
    if (skeleton_ == nullptr || skeleton_->BoneCount() <= 0) {
        return;
    }
    const int boneCount = skeleton_->BoneCount();
    if (boneWorld.size() != static_cast<std::size_t>(boneCount)) {
        outSkin.assign(static_cast<std::size_t>(boneCount), glm::mat4(1.0f));
        return;
    }
    outSkin.resize(static_cast<std::size_t>(boneCount), glm::mat4(1.0f));
    for (int i = 0; i < boneCount; ++i) {
        outSkin[static_cast<std::size_t>(i)] =
            boneWorld[static_cast<std::size_t>(i)] *
            skeleton_->inverseBindPose[static_cast<std::size_t>(i)];
    }
}

void AnimInstance::NativeUpdateAnimation(float deltaTime) {
    UpdateLocomotion(deltaTime);
}

void AnimInstance::GetBoneWorldMatrices(std::vector<glm::mat4>& outBoneWorld) const {
    SampleLocomotionBoneWorld(outBoneWorld);
}

void AnimInstance::GetSkinMatrices(std::vector<glm::mat4>& outSkin) const {
    std::vector<glm::mat4> world;
    GetBoneWorldMatrices(world);
    SkinFromBoneWorld(world, outSkin);
}

bool LoadSkeletalMeshFromFbx(const std::string& path, SkeletalMeshData& out) {
    out = {};
    ufbx_error error{};
    const ufbx_load_opts opts = MakeLoadOpts();
    ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);
    if (scene == nullptr) {
        std::cerr << "ufbx: failed to load mesh FBX '" << path << "': " << error.description.data
                  << '\n';
        return false;
    }

    ufbx_mesh* mesh = nullptr;
    ufbx_skin_deformer* skin = nullptr;
    for (size_t i = 0; i < scene->meshes.count; ++i) {
        ufbx_mesh* candidate = scene->meshes.data[i];
        if (candidate != nullptr && candidate->skin_deformers.count > 0) {
            mesh = candidate;
            skin = candidate->skin_deformers.data[0];
            break;
        }
    }
    if (mesh == nullptr || skin == nullptr) {
        std::cerr << "ufbx: no skinned mesh in '" << path << "'\n";
        ufbx_free_scene(scene);
        return false;
    }

    const int clusterCount = static_cast<int>(skin->clusters.count);
    if (clusterCount <= 0 || clusterCount > kMaxSkinBones) {
        std::cerr << "ufbx: invalid bone count " << clusterCount << " in '" << path << "'\n";
        ufbx_free_scene(scene);
        return false;
    }

    out.skeleton.boneNames.resize(static_cast<std::size_t>(clusterCount));
    out.skeleton.parentIndices.assign(static_cast<std::size_t>(clusterCount), -1);
    out.skeleton.inverseBindPose.resize(static_cast<std::size_t>(clusterCount), glm::mat4(1.0f));

    std::unordered_map<ufbx_node*, int> nodeToBone;
    for (int c = 0; c < clusterCount; ++c) {
        ufbx_skin_cluster* cluster = skin->clusters.data[c];
        if (cluster == nullptr || cluster->bone_node == nullptr) {
            continue;
        }
        ufbx_node* bone = cluster->bone_node;
        const std::string name(bone->name.data, bone->name.length);
        out.skeleton.boneNames[static_cast<std::size_t>(c)] = name;
        out.skeleton.inverseBindPose[static_cast<std::size_t>(c)] =
            ToGlm(cluster->geometry_to_bone);
        nodeToBone[bone] = c;
    }
    for (int c = 0; c < clusterCount; ++c) {
        ufbx_skin_cluster* cluster = skin->clusters.data[c];
        if (cluster == nullptr || cluster->bone_node == nullptr) {
            continue;
        }
        ufbx_node* parent = cluster->bone_node->parent;
        while (parent != nullptr) {
            const auto it = nodeToBone.find(parent);
            if (it != nodeToBone.end()) {
                out.skeleton.parentIndices[static_cast<std::size_t>(c)] = it->second;
                break;
            }
            parent = parent->parent;
        }
    }

    // Triangulate into unique vertices (per corner attributes).
    // `ufbx_triangulate_face` returns the number of *triangles* (not indices).
    out.localMin = glm::vec3(std::numeric_limits<float>::max());
    out.localMax = glm::vec3(std::numeric_limits<float>::lowest());

    const size_t triIndexCapacity = std::max<size_t>(mesh->max_face_triangles * 3u, 16u * 3u);
    std::vector<uint32_t> tri(triIndexCapacity);

    for (size_t fi = 0; fi < mesh->faces.count; ++fi) {
        const ufbx_face face = mesh->faces.data[fi];
        if (face.num_indices < 3) {
            continue;
        }
        const uint32_t numTris = ufbx_triangulate_face(tri.data(), tri.size(), mesh, face);
        for (uint32_t t = 0; t < numTris; ++t) {
            for (int k = 0; k < 3; ++k) {
                const uint32_t index = tri[static_cast<size_t>(t) * 3u + static_cast<size_t>(k)];
                const uint32_t vi = mesh->vertex_indices.data[index];

                SkeletalVertex v{};
                const ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
                v.position = {static_cast<float>(pos.x), static_cast<float>(pos.y),
                              static_cast<float>(pos.z)};
                if (mesh->vertex_normal.exists) {
                    const ufbx_vec3 n = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
                    v.normal = glm::normalize(glm::vec3{
                        static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z)});
                }
                if (mesh->vertex_uv.exists) {
                    const ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
                    v.texCoord = {static_cast<float>(uv.x), static_cast<float>(uv.y)};
                }

                // Skin weights (up to 4).
                if (vi < skin->vertices.count) {
                    const ufbx_skin_vertex sv = skin->vertices.data[vi];
                    float wsum = 0.0f;
                    const uint32_t nw = std::min<uint32_t>(sv.num_weights, kMaxBoneInfluences);
                    for (uint32_t wi = 0; wi < nw; ++wi) {
                        const ufbx_skin_weight sw = skin->weights.data[sv.weight_begin + wi];
                        v.boneIndices[wi] = static_cast<int>(sw.cluster_index);
                        v.boneWeights[wi] = static_cast<float>(sw.weight);
                        wsum += static_cast<float>(sw.weight);
                    }
                    if (wsum > 1.0e-6f) {
                        v.boneWeights /= wsum;
                    } else {
                        v.boneWeights[0] = 1.0f;
                    }
                } else {
                    v.boneWeights[0] = 1.0f;
                }

                out.localMin = glm::min(out.localMin, v.position);
                out.localMax = glm::max(out.localMax, v.position);
                out.indices.push_back(static_cast<std::uint32_t>(out.vertices.size()));
                out.vertices.push_back(v);
            }
        }
    }

    if (out.empty()) {
        std::cerr << "ufbx: skinned mesh produced no triangles in '" << path << "'\n";
        ufbx_free_scene(scene);
        return false;
    }

    std::unordered_map<std::string, ufbx_node*> nodesByName;
    CollectNodesByName(scene, nodesByName);
    BakeAnimFromScene(scene, out.skeleton, nodesByName, out.embeddedAnim);

    ufbx_free_scene(scene);
    return out.skeleton.BoneCount() > 0;
}

bool LoadAnimSequenceFromFbx(const std::string& path, const Skeleton& skeleton, AnimSequence& out) {
    out = {};
    ufbx_error error{};
    const ufbx_load_opts opts = MakeLoadOpts();
    ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);
    if (scene == nullptr) {
        std::cerr << "ufbx: failed to load anim FBX '" << path << "': " << error.description.data
                  << '\n';
        return false;
    }

    std::unordered_map<std::string, ufbx_node*> nodesByName;
    CollectNodesByName(scene, nodesByName);
    const bool ok = BakeAnimFromScene(scene, skeleton, nodesByName, out);
    if (ok) {
        // Prefer filename stem as clip name.
        const auto slash = path.find_last_of("/\\");
        const auto dot = path.find_last_of('.');
        const std::size_t start = slash == std::string::npos ? 0 : slash + 1;
        const std::size_t end = (dot == std::string::npos || dot < start) ? path.size() : dot;
        out.name = path.substr(start, end - start);
    }
    ufbx_free_scene(scene);
    return ok;
}

} // namespace leon
