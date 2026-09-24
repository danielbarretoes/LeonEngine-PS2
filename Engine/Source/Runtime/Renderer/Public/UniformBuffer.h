#pragma once

#include <cstddef>
#include "RHIHandles.h"


/// GL_UNIFORM_BUFFER wrapper bound to a fixed binding point (OpenGL 3.3+).
class UniformBuffer {
public:
    UniformBuffer() = default;
    ~UniformBuffer();

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;

    bool Create(std::size_t sizeBytes, unsigned int bindingPoint);
    void Destroy();

    void Update(const void* data, std::size_t sizeBytes) const;
    void Bind() const;

    [[nodiscard]] bool Valid() const { return id_ != kInvalidBuffer; }
    [[nodiscard]] unsigned int BindingPoint() const { return bindingPoint_; }

private:
    RHIBufferId id_ = kInvalidBuffer;
    unsigned int bindingPoint_ = 0;
    std::size_t sizeBytes_ = 0;
};

