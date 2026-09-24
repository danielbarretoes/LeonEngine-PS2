#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <leon/render/Texture.h>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace leon {
namespace {

std::size_t pixelIndex(int x, int y, int size) {
    const auto sx = static_cast<std::size_t>(size);
    return (static_cast<std::size_t>(y) * sx) + static_cast<std::size_t>(x);
}

} // namespace

Texture::~Texture() {
    Destroy();
}

Texture::Texture(Texture&& other) noexcept : id_(other.id_) {
    other.id_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        Destroy();
        id_ = other.id_;
        other.id_ = 0;
    }
    return *this;
}

Texture Texture::Create(int width, int height, const unsigned char* rgba) {
    Texture texture;
    if (width <= 0 || height <= 0 || rgba == nullptr) {
        return texture;
    }

    glGenTextures(1, &texture.id_);
    glBindTexture(GL_TEXTURE_2D, texture.id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

Texture Texture::CreateChecker(int size) {
    size = std::max(2, size);

    const auto count = pixelIndex(0, size, size);
    std::vector<unsigned char> pixels(count * 4u);
    const int cell = std::max(1, size / 8);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool dark = ((x / cell) + (y / cell)) % 2 == 0;
            const unsigned char c =
                dark ? static_cast<unsigned char>(60) : static_cast<unsigned char>(220);
            const std::size_t i = pixelIndex(x, y, size) * 4u;
            pixels[i + 0] = c;
            pixels[i + 1] = c;
            pixels[i + 2] = c;
            pixels[i + 3] = 255;
        }
    }
    return Create(size, size, pixels.data());
}

Texture Texture::CreateFlatNormal(int size) {
    size = std::max(1, size);
    const auto count = pixelIndex(0, size, size);
    std::vector<unsigned char> pixels(count * 4u, 255);
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = 128; // X
        pixels[i + 1] = 128; // Y
        pixels[i + 2] = 255; // Z
        pixels[i + 3] = 255;
    }
    return Create(size, size, pixels.data());
}

Texture Texture::CreateBumpNormal(int size) {
    size = std::max(8, size);

    // Height field ÔåÆ finite-difference normal map (tileable, intentionally strong).
    const auto count = pixelIndex(0, size, size);
    std::vector<float> height(count);
    constexpr float kTwoPi = 6.28318530718f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const auto u = static_cast<float>(x) / static_cast<float>(size);
            const auto v = static_cast<float>(y) / static_cast<float>(size);
            // Dense ripples + circular dimples so lighting/reflections clearly warp.
            const float ripples =
                (0.55f * std::sin(u * kTwoPi * 8.0f) * std::cos(v * kTwoPi * 6.0f)) +
                (0.30f * std::sin((u + v) * kTwoPi * 10.0f));
            const float cx = std::fmod(u * 4.0f, 1.0f) - 0.5f;
            const float cy = std::fmod(v * 4.0f, 1.0f) - 0.5f;
            const float dimple = 0.45f * std::exp(-18.0f * ((cx * cx) + (cy * cy)));
            height[pixelIndex(x, y, size)] = ripples + dimple;
        }
    }

    std::vector<unsigned char> pixels(count * 4u);
    const float strength = 6.0f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const int x0 = (x + size - 1) % size;
            const int x1 = (x + 1) % size;
            const int y0 = (y + size - 1) % size;
            const int y1 = (y + 1) % size;
            const float hL = height[pixelIndex(x0, y, size)];
            const float hR = height[pixelIndex(x1, y, size)];
            const float hD = height[pixelIndex(x, y0, size)];
            const float hU = height[pixelIndex(x, y1, size)];
            float nx = (hL - hR) * strength;
            float ny = (hD - hU) * strength;
            float nz = 1.0f;
            const float invLen = 1.0f / std::sqrt(((nx * nx) + (ny * ny)) + (nz * nz));
            nx *= invLen;
            ny *= invLen;
            nz *= invLen;

            const std::size_t i = pixelIndex(x, y, size) * 4u;
            pixels[i + 0] = static_cast<unsigned char>(((nx * 0.5f) + 0.5f) * 255.0f);
            pixels[i + 1] = static_cast<unsigned char>(((ny * 0.5f) + 0.5f) * 255.0f);
            pixels[i + 2] = static_cast<unsigned char>(((nz * 0.5f) + 0.5f) * 255.0f);
            pixels[i + 3] = 255;
        }
    }
    return Create(size, size, pixels.data());
}

Texture Texture::LoadFromFile(const std::string& path) {
    stbi_set_flip_vertically_on_load(1);
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (data == nullptr) {
        std::cerr << "Failed to load texture: " << path << " (" << stbi_failure_reason() << ")\n";
        return {};
    }

    Texture texture = Create(width, height, data);
    stbi_image_free(data);
    return texture;
}

void Texture::Bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::Destroy() {
    if (id_ != 0) {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
}

} // namespace leon
