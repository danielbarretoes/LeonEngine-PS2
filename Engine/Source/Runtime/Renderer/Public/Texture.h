#pragma once

#include "RHIHandles.h"

#include <string>


/// 2D GPU texture (RGBA8).
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    [[nodiscard]] static Texture Create(int width, int height, const unsigned char* rgba);
    [[nodiscard]] static Texture CreateChecker(int size = 64);
    /// Flat normal map in tangent space (points along +Z).
    [[nodiscard]] static Texture CreateFlatNormal(int size = 4);
    /// Strong procedural bumps for demo normal mapping (tileable).
    [[nodiscard]] static Texture CreateBumpNormal(int size = 256);
    [[nodiscard]] static Texture LoadFromFile(const std::string& path);

    void Bind(unsigned int unit = 0) const;
    [[nodiscard]] bool Valid() const { return id_ != 0; }

private:
    void Destroy();

    RHITextureId id_ = kInvalidTexture;
};

