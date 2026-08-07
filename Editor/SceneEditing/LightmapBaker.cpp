#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <leon/core/FileIO.h>
#include <leon/editor/LightmapBaker.h>
#include <leon/level/LightmapIO.h>
#include <leon/render/Frustum.h>
#include <leon/render/Texture.h>
#include <limits>
#include <memory>
#include <vector>

namespace leon::editor {
namespace {

[[nodiscard]] int ClampLightmapResolution(int res) {
    static constexpr int kAllowed[] = {32, 64, 128, 256, 512};
    int best = 128;
    int bestDist = std::numeric_limits<int>::max();
    for (int v : kAllowed) {
        const int d = std::abs(v - res);
        if (d < bestDist) {
            bestDist = d;
            best = v;
        }
    }
    return best;
}

[[nodiscard]] float EdgeFunction(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

[[nodiscard]] glm::vec3 EvaluateLighting(const glm::vec3& worldPos, const glm::vec3& worldN,
                                         const Level& level, const std::vector<Aabb>& occluders,
                                         std::size_t selfOccluderIndex) {
    glm::vec3 lit = glm::vec3(0.12f); // flat ambient
    const glm::vec3 N = glm::normalize(worldN);

    for (const DirectionalLight& light : level.DirectionalLights()) {
        const glm::vec3 L = glm::normalize(-light.GetDirection());
        const float ndl = std::max(glm::dot(N, L), 0.0f);
        float shadow = 0.0f;
        if (light.castShadows && ndl > 0.0f) {
            // Cheap occlusion: ray toward light against other static AABBs (skip self).
            const glm::vec3 origin = worldPos + N * 0.02f;
            float tHit = 0.0f;
            for (std::size_t oi = 0; oi < occluders.size(); ++oi) {
                if (oi == selfOccluderIndex) {
                    continue;
                }
                if (occluders[oi].intersectRay(origin, L, tHit) && tHit > 0.05f && tHit < 80.0f) {
                    shadow = 1.0f;
                    break;
                }
            }
        }
        lit += light.lightColor * light.intensity * ndl * (1.0f - shadow);
    }

    for (const PointLight& light : level.PointLights()) {
        glm::vec3 toLight = light.transform.position - worldPos;
        const float dist = glm::length(toLight);
        if (dist < 1.0e-4f) {
            continue;
        }
        toLight /= dist;
        float atten = 1.0f - dist / std::max(light.range, 0.001f);
        atten = std::clamp(atten, 0.0f, 1.0f);
        atten *= atten;
        const float ndl = std::max(glm::dot(N, toLight), 0.0f);
        lit += light.lightColor * light.intensity * ndl * atten;
    }

    return glm::clamp(lit, glm::vec3(0.0f), glm::vec3(8.0f));
}

void DilateLightmap(std::vector<unsigned char>& rgba, std::vector<float>& coverage, int size) {
    std::vector<unsigned char> out = rgba;
    std::vector<float> nextCoverage = coverage;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int i = y * size + x;
            if (coverage[static_cast<std::size_t>(i)] > 0.0f) {
                continue;
            }
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            int count = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= size || ny >= size) {
                        continue;
                    }
                    const int ni = ny * size + nx;
                    if (coverage[static_cast<std::size_t>(ni)] <= 0.0f) {
                        continue;
                    }
                    const int o = ni * 4;
                    r += out[static_cast<std::size_t>(o + 0)];
                    g += out[static_cast<std::size_t>(o + 1)];
                    b += out[static_cast<std::size_t>(o + 2)];
                    ++count;
                }
            }
            if (count > 0) {
                const int o = i * 4;
                rgba[static_cast<std::size_t>(o + 0)] =
                    static_cast<unsigned char>(r / static_cast<float>(count));
                rgba[static_cast<std::size_t>(o + 1)] =
                    static_cast<unsigned char>(g / static_cast<float>(count));
                rgba[static_cast<std::size_t>(o + 2)] =
                    static_cast<unsigned char>(b / static_cast<float>(count));
                rgba[static_cast<std::size_t>(o + 3)] = 255;
                // Mark filled so a subsequent dilate pass can expand further.
                nextCoverage[static_cast<std::size_t>(i)] = 1.0f;
            }
        }
    }
    coverage = std::move(nextCoverage);
}

[[nodiscard]] std::shared_ptr<Texture> BakeMeshLightmap(const StaticMeshComponent& object,
                                                        const Level& level,
                                                        const std::vector<Aabb>& occluders,
                                                        std::size_t selfOccluderIndex,
                                                        std::vector<unsigned char>* outRgba,
                                                        int* outSize) {
    if (object.mesh == nullptr || !object.mesh->HasCpuData()) {
        return nullptr;
    }
    const MeshData& data = object.mesh->CpuData();
    if (data.indices.size() < 3) {
        return nullptr;
    }

    const int size = ClampLightmapResolution(object.lightmapResolution);
    std::vector<unsigned char> rgba(static_cast<std::size_t>(size * size * 4), 0);
    std::vector<float> coverage(static_cast<std::size_t>(size * size), 0.0f);
    std::vector<glm::vec3> accum(static_cast<std::size_t>(size * size), glm::vec3(0.0f));

    const glm::mat4 model = object.EffectiveModelMatrix();
    const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));

    for (std::size_t t = 0; t + 2 < data.indices.size(); t += 3) {
        const Vertex& v0 = data.vertices[data.indices[t]];
        const Vertex& v1 = data.vertices[data.indices[t + 1]];
        const Vertex& v2 = data.vertices[data.indices[t + 2]];

        const glm::vec2 uv0 = v0.texCoord;
        const glm::vec2 uv1 = v1.texCoord;
        const glm::vec2 uv2 = v2.texCoord;

        // Degenerate / missing UVs → skip (would stamp entire atlas).
        const float area = EdgeFunction(uv0, uv1, uv2);
        if (std::abs(area) < 1.0e-8f) {
            continue;
        }

        const glm::vec2 p0 = uv0 * static_cast<float>(size);
        const glm::vec2 p1 = uv1 * static_cast<float>(size);
        const glm::vec2 p2 = uv2 * static_cast<float>(size);

        const int minX = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
        const int maxX =
            std::min(size - 1, static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
        const int minY = std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
        const int maxY =
            std::min(size - 1, static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));

        const glm::vec3 w0 = glm::vec3(model * glm::vec4(v0.position, 1.0f));
        const glm::vec3 w1 = glm::vec3(model * glm::vec4(v1.position, 1.0f));
        const glm::vec3 w2 = glm::vec3(model * glm::vec4(v2.position, 1.0f));
        const glm::vec3 n0 = glm::normalize(normalMat * v0.normal);
        const glm::vec3 n1 = glm::normalize(normalMat * v1.normal);
        const glm::vec3 n2 = glm::normalize(normalMat * v2.normal);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const glm::vec2 p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
                float wA = EdgeFunction(p1, p2, p);
                float wB = EdgeFunction(p2, p0, p);
                float wC = EdgeFunction(p0, p1, p);
                if (area < 0.0f) {
                    wA = -wA;
                    wB = -wB;
                    wC = -wC;
                }
                if (wA < 0.0f || wB < 0.0f || wC < 0.0f) {
                    continue;
                }
                const float inv = 1.0f / std::abs(area);
                const float b0 = wA * inv;
                const float b1 = wB * inv;
                const float b2 = wC * inv;
                const glm::vec3 worldPos = w0 * b0 + w1 * b1 + w2 * b2;
                const glm::vec3 worldN = glm::normalize(n0 * b0 + n1 * b1 + n2 * b2);
                const glm::vec3 sample =
                    EvaluateLighting(worldPos, worldN, level, occluders, selfOccluderIndex);
                const int idx = y * size + x;
                accum[static_cast<std::size_t>(idx)] += sample;
                coverage[static_cast<std::size_t>(idx)] += 1.0f;
            }
        }
    }

    for (int i = 0; i < size * size; ++i) {
        if (coverage[static_cast<std::size_t>(i)] <= 0.0f) {
            continue;
        }
        glm::vec3 c = accum[static_cast<std::size_t>(i)] / coverage[static_cast<std::size_t>(i)];
        // Simple Reinhard so high intensity stays visible.
        c = c / (c + glm::vec3(1.0f));
        const int o = i * 4;
        rgba[static_cast<std::size_t>(o + 0)] =
            static_cast<unsigned char>(std::clamp(c.x, 0.0f, 1.0f) * 255.0f);
        rgba[static_cast<std::size_t>(o + 1)] =
            static_cast<unsigned char>(std::clamp(c.y, 0.0f, 1.0f) * 255.0f);
        rgba[static_cast<std::size_t>(o + 2)] =
            static_cast<unsigned char>(std::clamp(c.z, 0.0f, 1.0f) * 255.0f);
        rgba[static_cast<std::size_t>(o + 3)] = 255;
    }

    DilateLightmap(rgba, coverage, size);
    DilateLightmap(rgba, coverage, size);

    if (outRgba != nullptr) {
        *outRgba = rgba;
        *outSize = size;
    }

    Texture tex = Texture::Create(size, size, rgba.data());
    if (!tex.Valid()) {
        return nullptr;
    }
    return std::make_shared<Texture>(std::move(tex));
}

bool WriteLmFile(const std::filesystem::path& path, int w, int h,
                 const std::vector<unsigned char>& rgba) {
    const char magic[4] = {'L', 'M', '0', '1'};
    const std::uint32_t width = static_cast<std::uint32_t>(w);
    const std::uint32_t height = static_cast<std::uint32_t>(h);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(12 + rgba.size());
    bytes.insert(bytes.end(), magic, magic + 4);
    const auto appendU32 = [&](std::uint32_t v) {
        bytes.push_back(static_cast<std::uint8_t>(v & 0xFFu));
        bytes.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
        bytes.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
        bytes.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
    };
    appendU32(width);
    appendU32(height);
    bytes.insert(bytes.end(), rgba.begin(), rgba.end());
    return WriteFileAtomic(path, bytes);
}

} // namespace

int BakeLevelLightmaps(Level& level, std::string* outMessage, const std::string& levelPath) {
    // Parallel arrays: occluder AABB + owning mesh index (for skip-self shadow rays).
    std::vector<Aabb> occluders;
    std::vector<std::size_t> occluderMeshIndex;
    const auto& meshes = level.StaticMeshes();
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        const StaticMeshComponent& mesh = meshes[i];
        if (mesh.hidden || mesh.mobility != EComponentMobility::Static || mesh.mesh == nullptr ||
            !mesh.mesh->Valid()) {
            continue;
        }
        occluders.push_back(Aabb::fromLocalTransformed(mesh.mesh->LocalMin(), mesh.mesh->LocalMax(),
                                                       mesh.EffectiveModelMatrix()));
        occluderMeshIndex.push_back(i);
    }

    std::filesystem::path lmDir;
    bool lmDirOk = false;
    if (!levelPath.empty()) {
        lmDir = std::filesystem::path(levelPath).parent_path() / "Lightmaps";
        std::error_code ec;
        std::filesystem::create_directories(lmDir, ec);
        if (ec) {
            std::cerr << "LightmapBaker: cannot create " << lmDir.string() << ": " << ec.message()
                      << '\n';
            lmDir.clear();
        } else {
            lmDirOk = true;
        }
    }

    int baked = 0;
    int skipped = 0;
    int persistFailed = 0;
    for (std::size_t meshIndex = 0; meshIndex < level.StaticMeshes().size(); ++meshIndex) {
        StaticMeshComponent& mesh = level.StaticMeshes()[meshIndex];
        if (mesh.hidden || mesh.mobility != EComponentMobility::Static) {
            ++skipped;
            mesh.lightmap.reset();
            mesh.lightmapPath.clear();
            continue;
        }
        if (mesh.mesh == nullptr || !mesh.mesh->HasCpuData()) {
            ++skipped;
            continue;
        }

        std::size_t selfOccluder = occluders.size(); // npos-like: no self in list
        for (std::size_t oi = 0; oi < occluderMeshIndex.size(); ++oi) {
            if (occluderMeshIndex[oi] == meshIndex) {
                selfOccluder = oi;
                break;
            }
        }

        std::vector<unsigned char> rgba;
        int size = 0;
        auto lm = BakeMeshLightmap(mesh, level, occluders, selfOccluder, &rgba, &size);
        if (lm) {
            mesh.lightmap = std::move(lm);
            mesh.lightmapResolution = ClampLightmapResolution(mesh.lightmapResolution);
            if (lmDirOk && size > 0 && !rgba.empty()) {
                const std::string id = EnsureLightmapId(mesh);
                const std::string fileName = "LM_" + id + ".lm";
                const std::filesystem::path absPath = lmDir / fileName;
                if (WriteLmFile(absPath, size, size, rgba)) {
                    mesh.lightmapPath =
                        (std::filesystem::path("Lightmaps") / fileName).generic_string();
                } else {
                    ++persistFailed;
                }
            }
            ++baked;
        } else {
            ++skipped;
        }
    }

    if (outMessage != nullptr) {
        *outMessage = "Build Lights: baked " + std::to_string(baked) + " static mesh(es), skipped " +
                      std::to_string(skipped);
        if (lmDirOk) {
            if (persistFailed > 0) {
                *outMessage += " (" + std::to_string(persistFailed) + " .lm write(s) failed)";
            } else {
                *outMessage += " (saved under Lightmaps/)";
            }
        } else if (!levelPath.empty()) {
            *outMessage += " (in memory only — Lightmaps/ unavailable)";
        }
    }
    return baked;
}

} // namespace leon::editor
