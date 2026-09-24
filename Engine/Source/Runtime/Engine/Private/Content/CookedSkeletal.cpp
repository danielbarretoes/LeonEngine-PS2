#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "Animation/CookedSkeletal.h"
#include <nlohmann/json.hpp>
#include <string>

namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

constexpr std::uint32_t kMaxCookedStringBytes = 1u << 20;   // 1 MiB
constexpr std::uint32_t kMaxCookedVertices = 2'000'000u;
constexpr std::uint32_t kMaxCookedIndices = 6'000'000u;
constexpr std::uint32_t kMaxCookedBones = 512u;
constexpr std::uint32_t kMaxCookedAnimFrames = 100'000u;

[[nodiscard]] bool readAllBytes(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        std::cerr << "CookedSkeletal: cannot read '" << path << "'\n";
        return false;
    }
    const auto end = in.tellg();
    if (end < 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(end));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(out.data()), end);
    return static_cast<bool>(in);
}

[[nodiscard]] std::string dirOf(const std::string& path) {
    return fs::path(path).parent_path().string();
}

[[nodiscard]] std::string joinRel(const std::string& baseDir, const std::string& rel) {
    return (fs::path(baseDir) / rel).lexically_normal().string();
}

[[nodiscard]] bool writeString(std::ostream& out, const std::string& s) {
    const auto len = static_cast<std::uint32_t>(s.size());
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (!s.empty()) {
        out.write(s.data(), static_cast<std::streamsize>(s.size()));
    }
    return static_cast<bool>(out);
}

[[nodiscard]] bool readString(const std::uint8_t*& ptr, const std::uint8_t* end, std::string& out) {
    if (ptr + sizeof(std::uint32_t) > end) {
        return false;
    }
    std::uint32_t len = 0;
    std::memcpy(&len, ptr, sizeof(len));
    ptr += sizeof(len);
    if (len > kMaxCookedStringBytes || ptr + len > end) {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    return true;
}

[[nodiscard]] bool readF32(const std::uint8_t*& ptr, const std::uint8_t* end, float& out) {
    if (ptr + sizeof(float) > end) {
        return false;
    }
    std::memcpy(&out, ptr, sizeof(float));
    ptr += sizeof(float);
    return true;
}

[[nodiscard]] bool readVec3(const std::uint8_t*& ptr, const std::uint8_t* end, glm::vec3& out) {
    return readF32(ptr, end, out.x) && readF32(ptr, end, out.y) && readF32(ptr, end, out.z);
}

[[nodiscard]] bool writeSimpleCharacterLmat(const std::string& path, const std::string& matName,
                                            const std::string& baseColorMapPath = {}) {
    std::ofstream out(path);
    if (!out) {
        return false;
    }
    out << "# Leon Material (.lmat)\n\n"
        << "[Info]\n"
        << "Name=" << matName << "\n"
        << "ShadingModel=DefaultLit\n\n"
        << "[Parameters]\n"
        << "BaseColor=0.72,0.74,0.78\n"
        << "Specular=0.04,0.04,0.04\n"
        << "Metallic=0.05\n"
        << "Roughness=0.45\n"
        << "Opacity=1.0\n"
        << "Shininess=24.0\n"
        << "UVScale=1.0,1.0\n"
        << "CastsShadows=true\n"
        << "PlanarMirror=false\n\n"
        << "[Textures]\n"
        << "BaseColorMap=" << baseColorMapPath << "\n"
        << "NormalMap=\n";
    return static_cast<bool>(out);
}

void writeVertex(std::ostream& out, const FSkeletalVertex& v) {
    out.write(reinterpret_cast<const char*>(&v.position), sizeof(float) * 3);
    out.write(reinterpret_cast<const char*>(&v.normal), sizeof(float) * 3);
    out.write(reinterpret_cast<const char*>(&v.texCoord), sizeof(float) * 2);
    out.write(reinterpret_cast<const char*>(&v.tangent), sizeof(float) * 4);
    const int bones[4] = {v.boneIndices.x, v.boneIndices.y, v.boneIndices.z, v.boneIndices.w};
    out.write(reinterpret_cast<const char*>(bones), sizeof(bones));
    out.write(reinterpret_cast<const char*>(&v.boneWeights), sizeof(float) * 4);
}

[[nodiscard]] bool readVertex(const std::uint8_t*& ptr, const std::uint8_t* end,
                              FSkeletalVertex& v) {
    auto take = [&](void* dst, std::size_t n) -> bool {
        if (ptr + n > end) {
            return false;
        }
        std::memcpy(dst, ptr, n);
        ptr += n;
        return true;
    };
    int bones[4]{};
    if (!take(&v.position, sizeof(float) * 3) || !take(&v.normal, sizeof(float) * 3) ||
        !take(&v.texCoord, sizeof(float) * 2) || !take(&v.tangent, sizeof(float) * 4) ||
        !take(bones, sizeof(bones)) || !take(&v.boneWeights, sizeof(float) * 4)) {
        return false;
    }
    v.boneIndices = {bones[0], bones[1], bones[2], bones[3]};
    return true;
}

} // namespace

bool SaveSkeletonLeon(const std::string& path, const USkeleton& skeleton, const std::string& name) {
    if (skeleton.BoneCount() <= 0) {
        return false;
    }
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "CookedSkeletal: cannot write skeleton '" << path << "'\n";
        return false;
    }
    const std::uint32_t magic = kLeonSkeletonMagic;
    const std::uint32_t version = static_cast<std::uint32_t>(kCookedFormatVersion);
    const std::uint32_t boneCount = static_cast<std::uint32_t>(skeleton.BoneCount());
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    if (!writeString(out, name)) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&boneCount), sizeof(boneCount));
    for (int i = 0; i < skeleton.BoneCount(); ++i) {
        if (!writeString(out, skeleton.boneNames[static_cast<std::size_t>(i)])) {
            return false;
        }
        const std::int32_t parent = skeleton.parentIndices[static_cast<std::size_t>(i)];
        out.write(reinterpret_cast<const char*>(&parent), sizeof(parent));
        const float* ib = glm::value_ptr(skeleton.inverseBindPose[static_cast<std::size_t>(i)]);
        out.write(reinterpret_cast<const char*>(ib), sizeof(float) * 16);
    }
    return static_cast<bool>(out);
}

bool LoadSkeleton(const std::string& path, USkeleton& out, std::string* outName) {
    std::vector<std::uint8_t> bytes;
    if (!readAllBytes(path, bytes) || bytes.size() < 12) {
        return false;
    }
    const std::uint8_t* ptr = bytes.data();
    const std::uint8_t* end = bytes.data() + bytes.size();
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::memcpy(&magic, ptr, 4);
    ptr += 4;
    std::memcpy(&version, ptr, 4);
    ptr += 4;
    if (magic != kLeonSkeletonMagic || version != static_cast<std::uint32_t>(kCookedFormatVersion)) {
        std::cerr << "CookedSkeletal: bad .lskel header in '" << path << "'\n";
        return false;
    }
    std::string name;
    if (!readString(ptr, end, name)) {
        return false;
    }
    if (outName != nullptr) {
        *outName = name;
    }
    std::uint32_t boneCount = 0;
    if (ptr + sizeof(boneCount) > end) {
        return false;
    }
    std::memcpy(&boneCount, ptr, sizeof(boneCount));
    ptr += sizeof(boneCount);
    out = {};
    for (std::uint32_t i = 0; i < boneCount; ++i) {
        std::string boneName;
        if (!readString(ptr, end, boneName)) {
            return false;
        }
        out.boneNames.push_back(std::move(boneName));
        if ((ptr + sizeof(std::int32_t) + (sizeof(float) * 16)) > end) {
            return false;
        }
        std::int32_t parent = -1;
        std::memcpy(&parent, ptr, sizeof(parent));
        ptr += sizeof(parent);
        out.parentIndices.push_back(parent);
        glm::mat4 ib(1.0f);
        std::memcpy(glm::value_ptr(ib), ptr, sizeof(float) * 16);
        ptr += sizeof(float) * 16;
        out.inverseBindPose.push_back(ib);
    }
    return out.BoneCount() > 0;
}

bool SaveSkeletalMeshLeon(const std::string& path, const FSkeletalMeshData& data,
                          const std::string& skeletonRelPath, const std::string& materialRelPath,
                          const std::string& assetName) {
    if (data.empty()) {
        return false;
    }
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "CookedSkeletal: cannot write skelmesh '" << path << "'\n";
        return false;
    }
    const std::uint32_t magic = kLeonSkelMeshMagic;
    const std::uint32_t version = static_cast<std::uint32_t>(kCookedFormatVersion);
    const std::uint32_t vcount = static_cast<std::uint32_t>(data.vertices.size());
    const std::uint32_t icount = static_cast<std::uint32_t>(data.indices.size());
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    if (!writeString(out, assetName) || !writeString(out, skeletonRelPath) ||
        !writeString(out, materialRelPath)) {
        return false;
    }
    const float localMin[3] = {data.localMin.x, data.localMin.y, data.localMin.z};
    const float localMax[3] = {data.localMax.x, data.localMax.y, data.localMax.z};
    out.write(reinterpret_cast<const char*>(localMin), sizeof(localMin));
    out.write(reinterpret_cast<const char*>(localMax), sizeof(localMax));
    out.write(reinterpret_cast<const char*>(&vcount), sizeof(vcount));
    out.write(reinterpret_cast<const char*>(&icount), sizeof(icount));
    for (const FSkeletalVertex& v : data.vertices) {
        writeVertex(out, v);
    }
    out.write(reinterpret_cast<const char*>(data.indices.data()),
              static_cast<std::streamsize>(data.indices.size() * sizeof(std::uint32_t)));
    return static_cast<bool>(out);
}

bool LoadSkeletalMesh(const std::string& path, FSkeletalMeshData& out,
                          USkeleton* skeletonOverride, std::string* outMaterialRelPath) {
    std::vector<std::uint8_t> bytes;
    if (!readAllBytes(path, bytes) || bytes.size() < 40) {
        std::cerr << "CookedSkeletal: cannot read skelmesh '" << path << "'\n";
        return false;
    }
    const std::uint8_t* ptr = bytes.data();
    const std::uint8_t* end = bytes.data() + bytes.size();
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::memcpy(&magic, ptr, 4);
    ptr += 4;
    std::memcpy(&version, ptr, 4);
    ptr += 4;
    if (magic != kLeonSkelMeshMagic || version != static_cast<std::uint32_t>(kCookedFormatVersion)) {
        std::cerr << "CookedSkeletal: bad .lskm header\n";
        return false;
    }

    out = {};
    std::string assetName;
    std::string skeletonRel;
    std::string materialRel;
    if (!readString(ptr, end, assetName) || !readString(ptr, end, skeletonRel) ||
        !readString(ptr, end, materialRel)) {
        return false;
    }
    (void)assetName;
    if (outMaterialRelPath != nullptr) {
        *outMaterialRelPath = materialRel;
    }
    if (!readVec3(ptr, end, out.localMin) || !readVec3(ptr, end, out.localMax)) {
        return false;
    }
    if (ptr + (sizeof(std::uint32_t) * 2) > end) {
        return false;
    }
    std::uint32_t vcount = 0;
    std::uint32_t icount = 0;
    std::memcpy(&vcount, ptr, 4);
    ptr += 4;
    std::memcpy(&icount, ptr, 4);
    ptr += 4;
    if (vcount == 0 || vcount > kMaxCookedVertices || icount > kMaxCookedIndices) {
        std::cerr << "CookedSkeletal: .lskm vertex/index count out of range\n";
        return false;
    }

    if (skeletonOverride != nullptr) {
        out.skeleton = *skeletonOverride;
    } else {
        const std::string skelPath = joinRel(dirOf(path), skeletonRel);
        if (!LoadSkeleton(skelPath, out.skeleton)) {
            return false;
        }
    }

    out.vertices.resize(vcount);
    for (std::uint32_t i = 0; i < vcount; ++i) {
        if (!readVertex(ptr, end, out.vertices[i])) {
            return false;
        }
    }
    const std::size_t indexBytes = static_cast<std::size_t>(icount) * sizeof(std::uint32_t);
    if (ptr + indexBytes > end) {
        return false;
    }
    out.indices.resize(icount);
    std::memcpy(out.indices.data(), ptr, indexBytes);
    return !out.empty();
}

bool SaveAnimSequenceLeon(const std::string& path, const UAnimSequence& anim, int boneCount,
                          const std::string& skeletonRelPath) {
    if (anim.FrameCount() <= 0 || boneCount <= 0) {
        return false;
    }
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    const std::uint32_t magic = kLeonAnimMagic;
    const std::uint32_t version = static_cast<std::uint32_t>(kCookedFormatVersion);
    const std::uint32_t frames = static_cast<std::uint32_t>(anim.FrameCount());
    const std::uint32_t bones = static_cast<std::uint32_t>(boneCount);
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    if (!writeString(out, anim.name) || !writeString(out, skeletonRelPath)) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&anim.durationSeconds), sizeof(anim.durationSeconds));
    out.write(reinterpret_cast<const char*>(&anim.framesPerSecond), sizeof(anim.framesPerSecond));
    out.write(reinterpret_cast<const char*>(&frames), sizeof(frames));
    out.write(reinterpret_cast<const char*>(&bones), sizeof(bones));
    for (int f = 0; f < anim.FrameCount(); ++f) {
        const auto& frame = anim.localPoseFrames[static_cast<std::size_t>(f)];
        if (static_cast<int>(frame.size()) != boneCount) {
            return false;
        }
        for (int b = 0; b < boneCount; ++b) {
            const float* p = glm::value_ptr(frame[static_cast<std::size_t>(b)]);
            out.write(reinterpret_cast<const char*>(p), sizeof(float) * 16);
        }
    }
    return static_cast<bool>(out);
}

bool LoadAnimSequence(const std::string& path, UAnimSequence& out) {
    std::vector<std::uint8_t> bytes;
    if (!readAllBytes(path, bytes) || bytes.size() < 28) {
        std::cerr << "CookedSkeletal: cannot read anim '" << path << "'\n";
        return false;
    }
    const std::uint8_t* ptr = bytes.data();
    const std::uint8_t* end = bytes.data() + bytes.size();
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::memcpy(&magic, ptr, 4);
    ptr += 4;
    std::memcpy(&version, ptr, 4);
    ptr += 4;
    if (magic != kLeonAnimMagic || version != static_cast<std::uint32_t>(kCookedFormatVersion)) {
        std::cerr << "CookedSkeletal: bad .lanim header\n";
        return false;
    }

    out = {};
    std::string name;
    std::string skeletonRel;
    if (!readString(ptr, end, name) || !readString(ptr, end, skeletonRel)) {
        return false;
    }
    (void)skeletonRel;
    out.name = std::move(name);
    if ((ptr + (sizeof(float) * 2) + (sizeof(std::uint32_t) * 2)) > end) {
        return false;
    }
    std::memcpy(&out.durationSeconds, ptr, sizeof(float));
    ptr += sizeof(float);
    std::memcpy(&out.framesPerSecond, ptr, sizeof(float));
    ptr += sizeof(float);
    std::uint32_t frameCount = 0;
    std::uint32_t boneCount = 0;
    std::memcpy(&frameCount, ptr, 4);
    ptr += 4;
    std::memcpy(&boneCount, ptr, 4);
    ptr += 4;
    if (frameCount == 0 || boneCount == 0 || frameCount > kMaxCookedAnimFrames ||
        boneCount > kMaxCookedBones) {
        std::cerr << "CookedSkeletal: .lanim frame/bone count out of range\n";
        return false;
    }
    const std::size_t need = static_cast<std::size_t>(frameCount) *
                             static_cast<std::size_t>(boneCount) * 16u * sizeof(float);
    if (ptr + need > end) {
        return false;
    }

    out.localPoseFrames.resize(frameCount);
    for (std::uint32_t f = 0; f < frameCount; ++f) {
        out.localPoseFrames[f].resize(boneCount);
        for (std::uint32_t b = 0; b < boneCount; ++b) {
            float* dst = glm::value_ptr(out.localPoseFrames[f][b]);
            std::memcpy(dst, ptr, sizeof(float) * 16);
            ptr += sizeof(float) * 16;
        }
    }
    return out.FrameCount() > 0;
}

bool SaveBlendSpace1DJson(const std::string& path, const BlendSpace1DAssetDesc& desc) {
    json root;
    root["version"] = kCookedFormatVersion;
    root["name"] = desc.name;
    root["axisMin"] = desc.axisMin;
    root["axisMax"] = desc.axisMax;
    root["samples"] = json::array();
    for (const auto& s : desc.samples) {
        root["samples"].push_back({{"anim", s.animRelPath}, {"position", s.position}});
    }
    std::ofstream out(path);
    if (!out) {
        return false;
    }
    out << root.dump(2) << '\n';
    return true;
}

bool LoadBlendSpace1DJson(const std::string& path, BlendSpace1DAssetDesc& out) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    json root;
    try {
        in >> root;
    } catch (const std::exception& ex) {
        std::cerr << "CookedSkeletal: bad blendspace JSON '" << path << "': " << ex.what() << '\n';
        return false;
    } catch (...) {
        std::cerr << "CookedSkeletal: bad blendspace JSON '" << path << "'\n";
        return false;
    }
    out = {};
    out.name = root.value("name", std::string{"BlendSpace1D"});
    out.axisMin = root.value("axisMin", 0.0f);
    out.axisMax = root.value("axisMax", 1.0f);
    if (!root.contains("samples") || !root["samples"].is_array()) {
        return false;
    }
    for (const json& s : root["samples"]) {
        BlendSpace1DAssetDesc::Sample sample;
        sample.animRelPath = s.at("anim").get<std::string>();
        sample.position = s.value("position", 0.0f);
        out.samples.push_back(std::move(sample));
    }
    return !out.samples.empty();
}

bool SaveCharacterVisualLchar(const std::string& path, const CharacterVisualDesc& desc) {
    std::ofstream out(path);
    if (!out) {
        return false;
    }
    out << "# Leon Character (.lchar) — version " << kCookedFormatVersion << "\n\n";
    out << "[Info]\n";
    out << "Name=" << (desc.name.empty() ? "Character" : desc.name) << "\n";
    out << "FitHeight=" << desc.fitHeight << "\n\n";
    out << "[Mesh]\n";
    out << "SkeletalMesh=" << desc.skeletalMeshRel << "\n\n";
    out << "[Animation]\n";
    out << "BlendSpace=" << desc.blendSpaceRel << "\n";
    if (!desc.jumpStartAnimRel.empty()) {
        out << "JumpStart=" << desc.jumpStartAnimRel << "\n";
    }
    if (!desc.fallLoopAnimRel.empty()) {
        out << "FallLoop=" << desc.fallLoopAnimRel << "\n";
    }
    if (!desc.landAnimRel.empty()) {
        out << "Land=" << desc.landAnimRel << "\n";
    }
    return static_cast<bool>(out);
}

bool LoadCharacterVisualLchar(const std::string& path, CharacterVisualDesc& outDesc) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    outDesc = {};
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        // strip comments
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        if (const auto semi = line.find(';'); semi != std::string::npos) {
            line = line.substr(0, semi);
        }
        // trim
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
            line.erase(line.begin());
        }
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            for (char& c : section) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) {
            key.pop_back();
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
            value.erase(value.begin());
        }
        for (char& c : key) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        if (section == "info") {
            if (key == "name") {
                outDesc.name = value;
            } else if (key == "fitheight") {
                try {
                    outDesc.fitHeight = std::stof(value);
                } catch (const std::exception& ex) {
                    std::cerr << "CookedSkeletal: bad FitHeight '" << value << "': " << ex.what()
                              << '\n';
                }
            }
        } else if (section == "mesh") {
            if (key == "skeletalmesh") {
                outDesc.skeletalMeshRel = value;
            }
        } else if (section == "animation") {
            if (key == "blendspace") {
                outDesc.blendSpaceRel = value;
            } else if (key == "jumpstart") {
                outDesc.jumpStartAnimRel = value;
            } else if (key == "fallloop") {
                outDesc.fallLoopAnimRel = value;
            } else if (key == "land") {
                outDesc.landAnimRel = value;
            }
        }
    }
    return !outDesc.skeletalMeshRel.empty() && !outDesc.blendSpaceRel.empty();
}

namespace {

bool LoadCharacterVisualJsonLegacy(const std::string& path, CharacterVisualDesc& out) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    json root;
    try {
        in >> root;
    } catch (const std::exception& ex) {
        std::cerr << "CookedSkeletal: bad character JSON '" << path << "': " << ex.what() << '\n';
        return false;
    } catch (...) {
        std::cerr << "CookedSkeletal: bad character JSON '" << path << "'\n";
        return false;
    }
    out = {};
    out.skeletalMeshRel = root.at("skeletalMesh").get<std::string>();
    out.blendSpaceRel = root.at("blendSpace").get<std::string>();
    out.fitHeight = root.value("fitHeight", 1.85f);
    out.jumpStartAnimRel = root.value("jumpStart", std::string{});
    out.fallLoopAnimRel = root.value("fallLoop", std::string{});
    out.landAnimRel = root.value("land", std::string{});
    return true;
}

} // namespace

bool LoadCharacterVisual(const std::string& path, CharacterVisualDesc& out) {
    const fs::path p(path);
    const std::string ext = p.extension().string();
    std::string extLower = ext;
    for (char& c : extLower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (extLower == ".lchar") {
        return LoadCharacterVisualLchar(path, out);
    }
    if (extLower == ".json") {
        return LoadCharacterVisualJsonLegacy(path, out);
    }
    // Path without / unknown ext: try .lchar semantics by content.
    if (LoadCharacterVisualLchar(path, out)) {
        return true;
    }
    return LoadCharacterVisualJsonLegacy(path, out);
}

bool CookAnimSequenceFromFbx(const std::string& fbxPath, const std::string& skeletonPath,
                             const std::string& outAnimPath, const std::string& animName,
                             bool looping) {
    USkeleton skeleton;
    if (!LoadSkeleton(skeletonPath, skeleton)) {
        std::cerr << "CookAnimSequenceFromFbx: failed skeleton '" << skeletonPath << "'\n";
        return false;
    }

    UAnimSequence anim;
    if (!LoadAnimSequenceFromFbx(fbxPath, skeleton, anim)) {
        std::cerr << "CookAnimSequenceFromFbx: failed FBX '" << fbxPath << "'\n";
        return false;
    }
    anim.name = animName.empty() ? fs::path(fbxPath).stem().string() : animName;
    anim.bLooping = looping;

    const fs::path animPath(outAnimPath);
    fs::create_directories(animPath.parent_path());

    const fs::path skelAbs = fs::absolute(skeletonPath).lexically_normal();
    const fs::path animDir = fs::absolute(animPath.parent_path()).lexically_normal();
    std::string skelRel = fs::relative(skelAbs, animDir).generic_string();
    if (skelRel.empty()) {
        skelRel = "../" + skelAbs.filename().string();
    }

    if (!SaveAnimSequenceLeon(animPath.string(), anim, skeleton.BoneCount(), skelRel)) {
        return false;
    }
    std::cout << "Cooked anim '" << anim.name << "' → " << outAnimPath << "\n";
    return true;
}

bool CookCharacterFromFbx(const std::string& characterName, const std::string& meshFbxPath,
                          const std::string& runFbxPath, const std::string& outDirectory,
                          const CookJumpAnimPaths& jumpAnims) {
    fs::create_directories(outDirectory);
    fs::create_directories(fs::path(outDirectory) / "Anims");
    fs::create_directories(fs::path(outDirectory) / "Materials");

    FSkeletalMeshData meshData;
    if (!LoadSkeletalMeshFromFbx(meshFbxPath, meshData)) {
        return false;
    }

    UAnimSequence idle = std::move(meshData.embeddedAnim);
    idle.name = "BreathingIdle";
    meshData.embeddedAnim = {};

    UAnimSequence run;
    if (!LoadAnimSequenceFromFbx(runFbxPath, meshData.skeleton, run)) {
        std::cerr << "CookCharacterFromFbx: run anim failed\n";
        return false;
    }
    if (run.name.empty()) {
        run.name = "Running";
    }

    UAnimSequence jumpStart;
    UAnimSequence fallLoop;
    UAnimSequence land;
    if (!jumpAnims.jumpStartFbx.empty()) {
        if (!LoadAnimSequenceFromFbx(jumpAnims.jumpStartFbx, meshData.skeleton, jumpStart)) {
            std::cerr << "CookCharacterFromFbx: jumpStart anim failed\n";
            return false;
        }
        jumpStart.name = "JumpingUp";
        jumpStart.bLooping = false;
    }
    if (!jumpAnims.fallLoopFbx.empty()) {
        if (!LoadAnimSequenceFromFbx(jumpAnims.fallLoopFbx, meshData.skeleton, fallLoop)) {
            std::cerr << "CookCharacterFromFbx: fallLoop anim failed\n";
            return false;
        }
        fallLoop.name = "FallingIdle";
        fallLoop.bLooping = true;
    }
    if (!jumpAnims.landFbx.empty()) {
        if (!LoadAnimSequenceFromFbx(jumpAnims.landFbx, meshData.skeleton, land)) {
            std::cerr << "CookCharacterFromFbx: land anim failed\n";
            return false;
        }
        land.name = "FallingToLanding";
        land.bLooping = false;
    }

    const std::string skeletonFile = characterName + ".lskel";
    const std::string skelMeshFile = characterName + ".lskm";
    const std::string materialRel = "Materials/M_" + characterName + ".lmat";
    const std::string idleAnimRel = "Anims/BreathingIdle.lanim";
    const std::string runAnimRel = "Anims/Running.lanim";
    const std::string jumpAnimRel = "Anims/JumpingUp.lanim";
    const std::string fallAnimRel = "Anims/FallingIdle.lanim";
    const std::string landAnimRel = "Anims/FallingToLanding.lanim";
    const std::string blendRel = characterName + "_Locomotion.blendspace1d.json";
    const std::string characterRel = characterName + ".lchar";

    const fs::path outDir(outDirectory);
    if (!SaveSkeletonLeon((outDir / skeletonFile).string(), meshData.skeleton, characterName)) {
        return false;
    }
    if (!SaveSkeletalMeshLeon((outDir / skelMeshFile).string(), meshData, skeletonFile,
                              materialRel, characterName)) {
        return false;
    }

    // Prefer pack albedo when present (ThirdPerson: Textures/T_Bot_D.png).
    std::string characterFolder = characterName;
    for (char& c : characterFolder) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const std::string albedoFile = "T_" + characterName + "_D.png";
    const std::string albedoRel =
        "assets/characters/" + characterFolder + "/Textures/" + albedoFile;
    const bool hasAlbedo = fs::exists(outDir / "Textures" / albedoFile);
    if (!writeSimpleCharacterLmat((outDir / materialRel).string(), "M_" + characterName,
                                  hasAlbedo ? albedoRel : std::string{})) {
        return false;
    }

    const std::string skelFromAnims = std::string("../") + skeletonFile;
    if (!SaveAnimSequenceLeon((outDir / idleAnimRel).string(), idle, meshData.skeleton.BoneCount(),
                              skelFromAnims)) {
        return false;
    }
    if (!SaveAnimSequenceLeon((outDir / runAnimRel).string(), run, meshData.skeleton.BoneCount(),
                              skelFromAnims)) {
        return false;
    }
    if (jumpStart.FrameCount() > 0) {
        if (!SaveAnimSequenceLeon((outDir / jumpAnimRel).string(), jumpStart,
                                  meshData.skeleton.BoneCount(), skelFromAnims)) {
            return false;
        }
    }
    if (fallLoop.FrameCount() > 0) {
        if (!SaveAnimSequenceLeon((outDir / fallAnimRel).string(), fallLoop,
                                  meshData.skeleton.BoneCount(), skelFromAnims)) {
            return false;
        }
    }
    if (land.FrameCount() > 0) {
        if (!SaveAnimSequenceLeon((outDir / landAnimRel).string(), land,
                                  meshData.skeleton.BoneCount(), skelFromAnims)) {
            return false;
        }
    }

    BlendSpace1DAssetDesc bs;
    bs.name = characterName + "_Locomotion";
    bs.samples.push_back({idleAnimRel, 0.0f});
    bs.samples.push_back({runAnimRel, 1.0f});
    if (!SaveBlendSpace1DJson((outDir / blendRel).string(), bs)) {
        return false;
    }

    CharacterVisualDesc character;
    character.name = characterName;
    character.skeletalMeshRel = skelMeshFile;
    character.blendSpaceRel = blendRel;
    character.fitHeight = 1.85f;
    if (jumpStart.FrameCount() > 0) {
        character.jumpStartAnimRel = jumpAnimRel;
    }
    if (fallLoop.FrameCount() > 0) {
        character.fallLoopAnimRel = fallAnimRel;
    }
    if (land.FrameCount() > 0) {
        character.landAnimRel = landAnimRel;
    }
    if (!SaveCharacterVisualLchar((outDir / characterRel).string(), character)) {
        return false;
    }

    std::cout << "Cooked character '" << characterName << "' → " << outDirectory << "\n"
              << "  " << skeletonFile << "\n"
              << "  " << skelMeshFile << "\n"
              << "  " << materialRel << "\n"
              << "  " << idleAnimRel << " / " << runAnimRel << "\n";
    if (jumpStart.FrameCount() > 0) {
        std::cout << "  " << jumpAnimRel << "\n";
    }
    if (fallLoop.FrameCount() > 0) {
        std::cout << "  " << fallAnimRel << "\n";
    }
    if (land.FrameCount() > 0) {
        std::cout << "  " << landAnimRel << "\n";
    }
    std::cout << "  " << blendRel << "\n"
              << "  " << characterRel << "\n";
    return true;
}

