#pragma once

#include "RHIHandles.h"

#include <string>


/// 2D GPU texture (RGBA8).
class UTexture2D {
public:
    UTexture2D() = default;
    ~UTexture2D();

    UTexture2D(const UTexture2D&) = delete;
    UTexture2D& operator=(const UTexture2D&) = delete;
    UTexture2D(UTexture2D&& other) noexcept;
    UTexture2D& operator=(UTexture2D&& other) noexcept;

    [[nodiscard]] static UTexture2D Create(int width, int height, const unsigned char* rgba);
    [[nodiscard]] static UTexture2D CreateChecker(int size = 64);
    /// Flat normal map in tangent space (points along +Z).
    [[nodiscard]] static UTexture2D CreateFlatNormal(int size = 4);
    /// Strong procedural bumps for demo normal mapping (tileable).
    [[nodiscard]] static UTexture2D CreateBumpNormal(int size = 256);
    [[nodiscard]] static UTexture2D LoadFromFile(const std::string& path);

    void Bind(unsigned int unit = 0) const;
    [[nodiscard]] bool Valid() const { return id_ != 0; }

private:
    void Destroy();

    FRHITextureId id_ = kInvalidTexture;
};

