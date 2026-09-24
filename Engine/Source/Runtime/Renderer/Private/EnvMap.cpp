#include <glad/glad.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include "EnvMap.h"
#include <numbers>
#include <stb_image.h>
#include <vector>

namespace leon {
namespace {

glm::vec3 faceDirection(int face, float u, float v) {
    // u,v in [-1, 1]; OpenGL cube face order.
    switch (face) {
    case 0:
        return glm::normalize(glm::vec3(1.0f, -v, -u)); // +X
    case 1:
        return glm::normalize(glm::vec3(-1.0f, -v, u)); // -X
    case 2:
        return glm::normalize(glm::vec3(u, 1.0f, v)); // +Y
    case 3:
        return glm::normalize(glm::vec3(u, -1.0f, -v)); // -Y
    case 4:
        return glm::normalize(glm::vec3(u, -v, 1.0f)); // +Z
    default:
        return glm::normalize(glm::vec3(-u, -v, -1.0f)); // -Z
    }
}

glm::vec3 sampleEquirect(const float* data, int width, int height, const glm::vec3& dir) {
    const float phi = std::atan2(dir.z, dir.x);
    const float theta = std::asin(std::clamp(dir.y, -1.0f, 1.0f));
    float uf = (phi / (2.0f * std::numbers::pi_v<float>)) + 0.5f;
    float vf = 0.5f - (theta / std::numbers::pi_v<float>);
    uf = uf - std::floor(uf);
    vf = std::clamp(vf, 0.0f, 1.0f);

    const float x = uf * static_cast<float>(width);
    const float y = vf * static_cast<float>(height - 1);
    const auto x0 = static_cast<int>(x) % width;
    const auto y0 = std::clamp(static_cast<int>(y), 0, height - 1);
    const int x1 = (x0 + 1) % width;
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - std::floor(x);
    const float ty = y - static_cast<float>(y0);

    const auto fetch = [&](int px, int py) {
        const std::size_t i = ((static_cast<std::size_t>(py) * static_cast<std::size_t>(width)) +
                               static_cast<std::size_t>(px)) *
                              3u;
        return glm::vec3(data[i + 0], data[i + 1], data[i + 2]);
    };

    const glm::vec3 c00 = fetch(x0, y0);
    const glm::vec3 c10 = fetch(x1, y0);
    const glm::vec3 c01 = fetch(x0, y1);
    const glm::vec3 c11 = fetch(x1, y1);
    const glm::vec3 c0 = glm::mix(c00, c10, tx);
    const glm::vec3 c1 = glm::mix(c01, c11, tx);
    return glm::mix(c0, c1, ty);
}

/// Lambertian irradiance for normal N (hemisphere integral over the equirect HDR).
glm::vec3 convolveIrradiance(const float* data, int width, int height, const glm::vec3& N) {
    glm::vec3 up =
        (std::abs(N.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(up, N));
    up = glm::cross(N, right);

    constexpr float kSampleDelta = 0.05f;
    const int phiSteps = static_cast<int>((2.0f * std::numbers::pi_v<float>) / kSampleDelta);
    const int thetaSteps = static_cast<int>((0.5f * std::numbers::pi_v<float>) / kSampleDelta);
    glm::vec3 irradiance(0.0f);
    int sampleCount = 0;
    for (int iPhi = 0; iPhi < phiSteps; ++iPhi) {
        const float phi = static_cast<float>(iPhi) * kSampleDelta;
        for (int iTheta = 0; iTheta < thetaSteps; ++iTheta) {
            const float theta = static_cast<float>(iTheta) * kSampleDelta;
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);
            const glm::vec3 tangentSample{sinTheta * std::cos(phi), sinTheta * std::sin(phi),
                                          cosTheta};
            const glm::vec3 sampleVec = glm::normalize(tangentSample.x * right +
                                                       tangentSample.y * up + tangentSample.z * N);
            irradiance += sampleEquirect(data, width, height, sampleVec) * cosTheta * sinTheta;
            ++sampleCount;
        }
    }
    return irradiance * (std::numbers::pi_v<float> / static_cast<float>(std::max(sampleCount, 1)));
}

unsigned int uploadCubeRgb16f(const std::vector<std::vector<float>>& faces, int faceSize,
                              bool generateMips) {
    unsigned int id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, id);
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, 0, GL_RGB16F, faceSize, faceSize,
                     0, GL_RGB, GL_FLOAT, faces[static_cast<std::size_t>(faceIndex)].data());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    if (generateMips) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    } else {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return id;
}

} // namespace

EnvMap::~EnvMap() {
    Destroy();
}

EnvMap::EnvMap(EnvMap&& other) noexcept
    : id_(other.id_), irradianceId_(other.irradianceId_), faceSize_(other.faceSize_),
      mipCount_(other.mipCount_) {
    other.id_ = 0;
    other.irradianceId_ = 0;
    other.faceSize_ = 0;
    other.mipCount_ = 0;
}

EnvMap& EnvMap::operator=(EnvMap&& other) noexcept {
    if (this != &other) {
        Destroy();
        id_ = other.id_;
        irradianceId_ = other.irradianceId_;
        faceSize_ = other.faceSize_;
        mipCount_ = other.mipCount_;
        other.id_ = 0;
        other.irradianceId_ = 0;
        other.faceSize_ = 0;
        other.mipCount_ = 0;
    }
    return *this;
}

EnvMap EnvMap::LoadFromHdr(const std::string& path, int faceSize, int irradianceSize) {
    EnvMap map;
    faceSize = std::max(16, faceSize);
    irradianceSize = std::max(8, irradianceSize);

    stbi_set_flip_vertically_on_load(0);
    int width = 0;
    int height = 0;
    int channels = 0;
    float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 3);
    if (data == nullptr || width <= 0 || height <= 0) {
        std::cerr << "Failed to load HDR: " << path << " (" << stbi_failure_reason() << ")\n";
        return map;
    }

    std::vector<std::vector<float>> envFaces(6);
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        envFaces[static_cast<std::size_t>(faceIndex)].assign(
            static_cast<std::size_t>(faceSize) * static_cast<std::size_t>(faceSize) * 3u, 0.0f);
        auto& face = envFaces[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < faceSize; ++y) {
            for (int x = 0; x < faceSize; ++x) {
                const float u =
                    ((((static_cast<float>(x) + 0.5f) / static_cast<float>(faceSize)) * 2.0f) -
                     1.0f);
                const float v =
                    ((((static_cast<float>(y) + 0.5f) / static_cast<float>(faceSize)) * 2.0f) -
                     1.0f);
                const glm::vec3 color =
                    sampleEquirect(data, width, height, faceDirection(faceIndex, u, v));
                const std::size_t i =
                    ((static_cast<std::size_t>(y) * static_cast<std::size_t>(faceSize)) +
                     static_cast<std::size_t>(x)) *
                    3u;
                face[i + 0] = color.r;
                face[i + 1] = color.g;
                face[i + 2] = color.b;
            }
        }
    }

    map.id_ = uploadCubeRgb16f(envFaces, faceSize, true);
    map.faceSize_ = faceSize;
    map.mipCount_ = 1 + static_cast<int>(std::floor(std::log2(static_cast<float>(faceSize))));

    std::cout << "EnvMap: convolving irradiance " << irradianceSize
              << "^2 (may take a moment)...\n";
    std::vector<std::vector<float>> irrFaces(6);
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        irrFaces[static_cast<std::size_t>(faceIndex)].assign(
            static_cast<std::size_t>(irradianceSize) * static_cast<std::size_t>(irradianceSize) *
                3u,
            0.0f);
        auto& face = irrFaces[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < irradianceSize; ++y) {
            for (int x = 0; x < irradianceSize; ++x) {
                const float u =
                    ((((static_cast<float>(x) + 0.5f) / static_cast<float>(irradianceSize)) *
                      2.0f) -
                     1.0f);
                const float v =
                    ((((static_cast<float>(y) + 0.5f) / static_cast<float>(irradianceSize)) *
                      2.0f) -
                     1.0f);
                const glm::vec3 N = faceDirection(faceIndex, u, v);
                const glm::vec3 color = convolveIrradiance(data, width, height, N);
                const std::size_t i =
                    ((static_cast<std::size_t>(y) * static_cast<std::size_t>(irradianceSize)) +
                     static_cast<std::size_t>(x)) *
                    3u;
                face[i + 0] = color.r;
                face[i + 1] = color.g;
                face[i + 2] = color.b;
            }
        }
    }
    map.irradianceId_ = uploadCubeRgb16f(irrFaces, irradianceSize, false);

    stbi_image_free(data);

    std::cout << "EnvMap: loaded '" << path << "' -> cubemap " << faceSize << "^2 ("
              << map.mipCount_ << " mips) + irradiance " << irradianceSize << "^2\n";
    return map;
}

void EnvMap::Bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, id_);
}

void EnvMap::BindIrradiance(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceId_);
}

void EnvMap::Destroy() {
    if (irradianceId_ != 0) {
        glDeleteTextures(1, &irradianceId_);
        irradianceId_ = 0;
    }
    if (id_ != 0) {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
    faceSize_ = 0;
    mipCount_ = 0;
}

} // namespace leon
