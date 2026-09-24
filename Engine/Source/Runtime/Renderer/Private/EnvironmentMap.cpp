#include <glad/glad.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include "EnvironmentMap.h"
#include <numbers>
#include <stb_image.h>
#include <vector>

namespace {

glm::vec3 FaceDirection(int Face, float U, float V) {
    // u,v in [-1, 1]; OpenGL cube face order.
    switch (Face) {
    case 0:
        return glm::normalize(glm::vec3(1.0f, -V, -U)); // +X
    case 1:
        return glm::normalize(glm::vec3(-1.0f, -V, U)); // -X
    case 2:
        return glm::normalize(glm::vec3(U, 1.0f, V)); // +Y
    case 3:
        return glm::normalize(glm::vec3(U, -1.0f, -V)); // -Y
    case 4:
        return glm::normalize(glm::vec3(U, -V, 1.0f)); // +Z
    default:
        return glm::normalize(glm::vec3(-U, -V, -1.0f)); // -Z
    }
}

glm::vec3 SampleEquirect(const float* Data, int Width, int Height, const glm::vec3& Dir) {
    const float Phi = std::atan2(Dir.z, Dir.x);
    const float Theta = std::asin(std::clamp(Dir.y, -1.0f, 1.0f));
    float Uf = (Phi / (2.0f * std::numbers::pi_v<float>)) + 0.5f;
    float Vf = 0.5f - (Theta / std::numbers::pi_v<float>);
    Uf = Uf - std::floor(Uf);
    Vf = std::clamp(Vf, 0.0f, 1.0f);

    const float X = Uf * static_cast<float>(Width);
    const float Y = Vf * static_cast<float>(Height - 1);
    const auto X0 = static_cast<int>(X) % Width;
    const auto Y0 = std::clamp(static_cast<int>(Y), 0, Height - 1);
    const int X1 = (X0 + 1) % Width;
    const int Y1 = std::min(Y0 + 1, Height - 1);
    const float Tx = X - std::floor(X);
    const float Ty = Y - static_cast<float>(Y0);

    const auto Fetch = [&](int Px, int Py) {
        const std::size_t I = ((static_cast<std::size_t>(Py) * static_cast<std::size_t>(Width)) +
                               static_cast<std::size_t>(Px)) *
                              3u;
        return glm::vec3(Data[I + 0], Data[I + 1], Data[I + 2]);
    };

    const glm::vec3 C00 = Fetch(X0, Y0);
    const glm::vec3 C10 = Fetch(X1, Y0);
    const glm::vec3 C01 = Fetch(X0, Y1);
    const glm::vec3 C11 = Fetch(X1, Y1);
    const glm::vec3 C0 = glm::mix(C00, C10, Tx);
    const glm::vec3 C1 = glm::mix(C01, C11, Tx);
    return glm::mix(C0, C1, Ty);
}

/// Lambertian irradiance for normal N (hemisphere integral over the equirect HDR).
glm::vec3 ConvolveIrradiance(const float* Data, int Width, int Height, const glm::vec3& N) {
    glm::vec3 Up =
        (std::abs(N.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 Right = glm::normalize(glm::cross(Up, N));
    Up = glm::cross(N, Right);

    constexpr float SampleDelta = 0.05f;
    const int PhiSteps = static_cast<int>((2.0f * std::numbers::pi_v<float>) / SampleDelta);
    const int ThetaSteps = static_cast<int>((0.5f * std::numbers::pi_v<float>) / SampleDelta);
    glm::vec3 Irradiance(0.0f);
    int SampleCount = 0;
    for (int IPhi = 0; IPhi < PhiSteps; ++IPhi) {
        const float Phi = static_cast<float>(IPhi) * SampleDelta;
        for (int ITheta = 0; ITheta < ThetaSteps; ++ITheta) {
            const float Theta = static_cast<float>(ITheta) * SampleDelta;
            const float SinTheta = std::sin(Theta);
            const float CosTheta = std::cos(Theta);
            const glm::vec3 TangentSample{SinTheta * std::cos(Phi), SinTheta * std::sin(Phi),
                                          CosTheta};
            const glm::vec3 SampleVec = glm::normalize(TangentSample.x * Right +
                                                       TangentSample.y * Up + TangentSample.z * N);
            Irradiance += SampleEquirect(Data, Width, Height, SampleVec) * CosTheta * SinTheta;
            ++SampleCount;
        }
    }
    return Irradiance * (std::numbers::pi_v<float> / static_cast<float>(std::max(SampleCount, 1)));
}

unsigned int UploadCubeRgb16f(const std::vector<std::vector<float>>& Faces, int InFaceSize,
                              bool bGenerateMips) {
    unsigned int LocalId = 0;
    glGenTextures(1, &LocalId);
    glBindTexture(GL_TEXTURE_CUBE_MAP, LocalId);
    for (int FaceIndex = 0; FaceIndex < 6; ++FaceIndex) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + FaceIndex, 0, GL_RGB16F, InFaceSize, InFaceSize,
                     0, GL_RGB, GL_FLOAT, Faces[static_cast<std::size_t>(FaceIndex)].data());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    if (bGenerateMips) {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    } else {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return LocalId;
}

} // namespace

FEnvironmentMap::~FEnvironmentMap() {
    Destroy();
}

FEnvironmentMap::FEnvironmentMap(FEnvironmentMap&& Other) noexcept
    : Id(Other.Id), IrradianceId(Other.IrradianceId), FaceSize(Other.FaceSize),
      MipCount(Other.MipCount) {
    Other.Id = 0;
    Other.IrradianceId = 0;
    Other.FaceSize = 0;
    Other.MipCount = 0;
}

FEnvironmentMap& FEnvironmentMap::operator=(FEnvironmentMap&& Other) noexcept {
    if (this != &Other) {
        Destroy();
        Id = Other.Id;
        IrradianceId = Other.IrradianceId;
        FaceSize = Other.FaceSize;
        MipCount = Other.MipCount;
        Other.Id = 0;
        Other.IrradianceId = 0;
        Other.FaceSize = 0;
        Other.MipCount = 0;
    }
    return *this;
}

FEnvironmentMap FEnvironmentMap::LoadFromHdr(const std::string& Path, int InFaceSize, int IrradianceSize) {
    FEnvironmentMap Map;
    InFaceSize = std::max(16, InFaceSize);
    IrradianceSize = std::max(8, IrradianceSize);

    stbi_set_flip_vertically_on_load(0);
    int Width = 0;
    int Height = 0;
    int Channels = 0;
    float* Data = stbi_loadf(Path.c_str(), &Width, &Height, &Channels, 3);
    if (Data == nullptr || Width <= 0 || Height <= 0) {
        std::cerr << "Failed to load HDR: " << Path << " (" << stbi_failure_reason() << ")\n";
        return Map;
    }

    std::vector<std::vector<float>> EnvFaces(6);
    for (int FaceIndex = 0; FaceIndex < 6; ++FaceIndex) {
        EnvFaces[static_cast<std::size_t>(FaceIndex)].assign(
            static_cast<std::size_t>(InFaceSize) * static_cast<std::size_t>(InFaceSize) * 3u, 0.0f);
        auto& Face = EnvFaces[static_cast<std::size_t>(FaceIndex)];
        for (int Y = 0; Y < InFaceSize; ++Y) {
            for (int X = 0; X < InFaceSize; ++X) {
                const float U =
                    ((((static_cast<float>(X) + 0.5f) / static_cast<float>(InFaceSize)) * 2.0f) -
                     1.0f);
                const float V =
                    ((((static_cast<float>(Y) + 0.5f) / static_cast<float>(InFaceSize)) * 2.0f) -
                     1.0f);
                const glm::vec3 Color =
                    SampleEquirect(Data, Width, Height, FaceDirection(FaceIndex, U, V));
                const std::size_t I =
                    ((static_cast<std::size_t>(Y) * static_cast<std::size_t>(InFaceSize)) +
                     static_cast<std::size_t>(X)) *
                    3u;
                Face[I + 0] = Color.r;
                Face[I + 1] = Color.g;
                Face[I + 2] = Color.b;
            }
        }
    }

    Map.Id = UploadCubeRgb16f(EnvFaces, InFaceSize, true);
    Map.FaceSize = InFaceSize;
    Map.MipCount = 1 + static_cast<int>(std::floor(std::log2(static_cast<float>(InFaceSize))));

    std::cout << "EnvMap: convolving irradiance " << IrradianceSize
              << "^2 (may take a moment)...\n";
    std::vector<std::vector<float>> IrrFaces(6);
    for (int FaceIndex = 0; FaceIndex < 6; ++FaceIndex) {
        IrrFaces[static_cast<std::size_t>(FaceIndex)].assign(
            static_cast<std::size_t>(IrradianceSize) * static_cast<std::size_t>(IrradianceSize) *
                3u,
            0.0f);
        auto& Face = IrrFaces[static_cast<std::size_t>(FaceIndex)];
        for (int Y = 0; Y < IrradianceSize; ++Y) {
            for (int X = 0; X < IrradianceSize; ++X) {
                const float U =
                    ((((static_cast<float>(X) + 0.5f) / static_cast<float>(IrradianceSize)) *
                      2.0f) -
                     1.0f);
                const float V =
                    ((((static_cast<float>(Y) + 0.5f) / static_cast<float>(IrradianceSize)) *
                      2.0f) -
                     1.0f);
                const glm::vec3 N = FaceDirection(FaceIndex, U, V);
                const glm::vec3 Color = ConvolveIrradiance(Data, Width, Height, N);
                const std::size_t I =
                    ((static_cast<std::size_t>(Y) * static_cast<std::size_t>(IrradianceSize)) +
                     static_cast<std::size_t>(X)) *
                    3u;
                Face[I + 0] = Color.r;
                Face[I + 1] = Color.g;
                Face[I + 2] = Color.b;
            }
        }
    }
    Map.IrradianceId = UploadCubeRgb16f(IrrFaces, IrradianceSize, false);

    stbi_image_free(Data);

    std::cout << "EnvMap: loaded '" << Path << "' -> cubemap " << InFaceSize << "^2 ("
              << Map.MipCount << " mips) + irradiance " << IrradianceSize << "^2\n";
    return Map;
}

void FEnvironmentMap::Bind(unsigned int Unit) const {
    glActiveTexture(GL_TEXTURE0 + Unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, Id);
}

void FEnvironmentMap::BindIrradiance(unsigned int Unit) const {
    glActiveTexture(GL_TEXTURE0 + Unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, IrradianceId);
}

void FEnvironmentMap::Destroy() {
    if (IrradianceId != 0) {
        glDeleteTextures(1, &IrradianceId);
        IrradianceId = 0;
    }
    if (Id != 0) {
        glDeleteTextures(1, &Id);
        Id = 0;
    }
    FaceSize = 0;
    MipCount = 0;
}

