#include "LeonMeshFormat.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include "MeshData.h"
#include <string>
#include <vector>

namespace {

constexpr char kMagic[4] = {'L', 'M', 'S', 'H'};
constexpr std::uint32_t kVersion = 1;

#pragma pack(push, 1)
struct FLeonMeshHeader {
    char magic[4];
    std::uint32_t version;
    std::uint32_t flags;
    std::uint32_t vertexCount;
    std::uint32_t indexCount;
    std::uint32_t submeshCount;
    std::uint32_t materialSlotCount;
    float aabbMin[3];
    float aabbMax[3];
};
#pragma pack(pop)

struct FLeonMeshSection {
    std::uint32_t indexOffset = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t materialIndex = 0;
};

[[nodiscard]] std::string ExtLower(const std::string& path) {
    const auto pos = path.find_last_of('.');
    if (pos == std::string::npos) {
        return {};
    }
    std::string e = path.substr(pos);
    for (char& c : e) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return e;
}

void ComputeAabb(const FMeshData& data, float outMin[3], float outMax[3]) {
    outMin[0] = outMin[1] = outMin[2] = 0.0f;
    outMax[0] = outMax[1] = outMax[2] = 0.0f;
    if (data.vertices.empty()) {
        return;
    }
    outMin[0] = outMax[0] = data.vertices[0].position.x;
    outMin[1] = outMax[1] = data.vertices[0].position.y;
    outMin[2] = outMax[2] = data.vertices[0].position.z;
    for (const FVertex& v : data.vertices) {
        outMin[0] = std::min(outMin[0], v.position.x);
        outMin[1] = std::min(outMin[1], v.position.y);
        outMin[2] = std::min(outMin[2], v.position.z);
        outMax[0] = std::max(outMax[0], v.position.x);
        outMax[1] = std::max(outMax[1], v.position.y);
        outMax[2] = std::max(outMax[2], v.position.z);
    }
}

} // namespace

bool IsLeonMeshPath(const std::string& path) {
    return ExtLower(path) == ".lmesh";
}

bool LoadLeonMeshFile(const std::string& path, FMeshData& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "LeonMesh: cannot open " << path << '\n';
        return false;
    }
    FLeonMeshHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in || std::memcmp(header.magic, kMagic, 4) != 0 || header.version != kVersion) {
        std::cerr << "LeonMesh: bad header in " << path << '\n';
        return false;
    }
    if (header.vertexCount == 0 || header.indexCount == 0) {
        std::cerr << "LeonMesh: empty mesh in " << path << '\n';
        return false;
    }

    FMeshData data;
    data.vertices.resize(header.vertexCount);
    data.indices.resize(header.indexCount);
    in.read(reinterpret_cast<char*>(data.vertices.data()),
            static_cast<std::streamsize>(sizeof(FVertex) * header.vertexCount));
    in.read(reinterpret_cast<char*>(data.indices.data()),
            static_cast<std::streamsize>(sizeof(std::uint32_t) * header.indexCount));
    if (!in) {
        std::cerr << "LeonMesh: truncated vertex/index data in " << path << '\n';
        return false;
    }

    if (header.submeshCount == 0) {
        data.submeshes.push_back(FMeshSection{0, static_cast<int>(header.indexCount), 0});
    } else {
        std::vector<FLeonMeshSection> subs(header.submeshCount);
        in.read(reinterpret_cast<char*>(subs.data()),
                static_cast<std::streamsize>(sizeof(FLeonMeshSection) * header.submeshCount));
        if (!in) {
            std::cerr << "LeonMesh: truncated submeshes in " << path << '\n';
            return false;
        }
        data.submeshes.reserve(subs.size());
        for (const FLeonMeshSection& s : subs) {
            data.submeshes.push_back(FMeshSection{static_cast<int>(s.indexOffset),
                                             static_cast<int>(s.indexCount),
                                             static_cast<int>(s.materialIndex)});
        }
    }

    // Optional string table: material slot paths (null-terminated), one per slot.
    data.materials.resize(std::max<std::uint32_t>(1, header.materialSlotCount));
    data.albedoMapPaths.resize(data.materials.size());
    for (std::uint32_t i = 0; i < header.materialSlotCount; ++i) {
        std::string slot;
        char ch = 0;
        while (in.get(ch)) {
            if (ch == '\0') {
                break;
            }
            slot.push_back(ch);
        }
        // Slot string may be a future .lmat path; keep in albedoMapPaths unused for now
        // or encode as material name. Store as tag in albedoMapPaths if looks like texture.
        if (!slot.empty() && (slot.find(".png") != std::string::npos ||
                              slot.find(".jpg") != std::string::npos)) {
            data.albedoMapPaths[i] = slot;
        }
        (void)slot;
    }

    out = std::move(data);
    return !out.empty();
}

bool SaveLeonMeshFile(const std::string& path, const FMeshData& data) {
    if (data.empty()) {
        std::cerr << "LeonMesh: refusing to save empty mesh\n";
        return false;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "LeonMesh: cannot write " << path << '\n';
        return false;
    }

    FLeonMeshHeader header{};
    std::memcpy(header.magic, kMagic, 4);
    header.version = kVersion;
    header.flags = 0;
    header.vertexCount = static_cast<std::uint32_t>(data.vertices.size());
    header.indexCount = static_cast<std::uint32_t>(data.indices.size());
    header.submeshCount = static_cast<std::uint32_t>(
        data.submeshes.empty() ? 1 : data.submeshes.size());
    header.materialSlotCount = static_cast<std::uint32_t>(
        std::max<std::size_t>(1, data.materials.size()));
    ComputeAabb(data, header.aabbMin, header.aabbMax);

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(data.vertices.data()),
              static_cast<std::streamsize>(sizeof(FVertex) * data.vertices.size()));
    out.write(reinterpret_cast<const char*>(data.indices.data()),
              static_cast<std::streamsize>(sizeof(std::uint32_t) * data.indices.size()));

    if (data.submeshes.empty()) {
        FLeonMeshSection s{0, header.indexCount, 0};
        out.write(reinterpret_cast<const char*>(&s), sizeof(s));
    } else {
        for (const FMeshSection& sm : data.submeshes) {
            FLeonMeshSection s{static_cast<std::uint32_t>(sm.indexOffset),
                           static_cast<std::uint32_t>(sm.indexCount),
                           static_cast<std::uint32_t>(sm.materialIndex)};
            out.write(reinterpret_cast<const char*>(&s), sizeof(s));
        }
    }

    for (std::uint32_t i = 0; i < header.materialSlotCount; ++i) {
        std::string slot;
        if (i < data.albedoMapPaths.size()) {
            slot = data.albedoMapPaths[i];
        }
        out.write(slot.c_str(), static_cast<std::streamsize>(slot.size() + 1));
    }
    return static_cast<bool>(out);
}

