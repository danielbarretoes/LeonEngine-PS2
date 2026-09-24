#include "Level/LightmapIO.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include "Misc/Paths.h"
#include "Texture2D.h"
#include <random>
#include <vector>

namespace {

[[nodiscard]] bool IsLm01Magic(const char magic[4]) {
    return magic[0] == 'L' && magic[1] == 'M' && magic[2] == '0' && magic[3] == '1';
}

} // namespace

std::shared_ptr<UTexture2D> LoadLightmapFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return nullptr;
    }
    char magic[4]{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    in.read(magic, 4);
    in.read(reinterpret_cast<char*>(&width), 4);
    in.read(reinterpret_cast<char*>(&height), 4);
    if (!IsLm01Magic(magic) || width == 0 || height == 0 || width > 4096 || height > 4096) {
        return nullptr;
    }
    const std::size_t pixelBytes =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    std::vector<unsigned char> rgba(pixelBytes);
    in.read(reinterpret_cast<char*>(rgba.data()), static_cast<std::streamsize>(rgba.size()));
    if (!in) {
        return nullptr;
    }
    UTexture2D tex = UTexture2D::Create(static_cast<int>(width), static_cast<int>(height), rgba.data());
    if (!tex.Valid()) {
        return nullptr;
    }
    return std::make_shared<UTexture2D>(std::move(tex));
}

std::filesystem::path ResolveLightmapAbsolutePath(const std::string& levelPath,
                                                  const std::string& lightmapRel) {
    if (lightmapRel.empty()) {
        return {};
    }
    std::filesystem::path rel(lightmapRel);
    if (rel.is_absolute()) {
        return rel;
    }
    if (!levelPath.empty()) {
        const std::filesystem::path besideLevel =
            (std::filesystem::path(levelPath).parent_path() / rel).lexically_normal();
        std::error_code ec;
        if (std::filesystem::is_regular_file(besideLevel, ec) && !ec) {
            return besideLevel;
        }
        // Legacy paths used lowercase "lightmaps/"; directory is "Lightmaps/".
        std::string alt = lightmapRel;
        if (alt.rfind("lightmaps/", 0) == 0) {
            alt.replace(0, 10, "Lightmaps/");
            const std::filesystem::path fixed =
                (std::filesystem::path(levelPath).parent_path() / alt).lexically_normal();
            if (std::filesystem::is_regular_file(fixed, ec) && !ec) {
                return fixed;
            }
        }
        return besideLevel;
    }
    const std::string resolved = FPaths::ResolveAssetPath(lightmapRel);
    return resolved.empty() ? rel : std::filesystem::path(resolved);
}

int LoadLevelLightmaps(Level& level, const std::string& levelPath, std::string* outMessage) {
    int loaded = 0;
    int failed = 0;
    for (StaticMeshComponent& mesh : level.StaticMeshes()) {
        if (mesh.lightmapPath.empty()) {
            continue;
        }
        const auto path = ResolveLightmapAbsolutePath(levelPath, mesh.lightmapPath);
        auto tex = LoadLightmapFile(path);
        if (tex) {
            mesh.lightmap = std::move(tex);
            ++loaded;
        } else {
            ++failed;
            std::cerr << "LightmapIO: failed to load '" << path.generic_string() << "'\n";
        }
    }
    if (outMessage != nullptr) {
        *outMessage = "Lightmaps: loaded " + std::to_string(loaded);
        if (failed > 0) {
            *outMessage += ", failed " + std::to_string(failed);
        }
    }
    return loaded;
}

std::string EnsureLightmapId(StaticMeshComponent& mesh) {
    if (!mesh.lightmapId.empty()) {
        return mesh.lightmapId;
    }
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint32_t> dist;
    char buf[17]{};
    std::snprintf(buf, sizeof(buf), "%08x%08x", dist(rng), dist(rng));
    mesh.lightmapId = buf;
    return mesh.lightmapId;
}

