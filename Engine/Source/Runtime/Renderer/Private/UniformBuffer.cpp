#include <glad/glad.h>

#include "UniformBuffer.h"

namespace leon {

UniformBuffer::~UniformBuffer() {
    Destroy();
}

bool UniformBuffer::Create(std::size_t sizeBytes, unsigned int bindingPoint) {
    Destroy();
    if (sizeBytes == 0) {
        return false;
    }

    glGenBuffers(1, &id_);
    glBindBuffer(GL_UNIFORM_BUFFER, id_);
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(sizeBytes), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    bindingPoint_ = bindingPoint;
    sizeBytes_ = sizeBytes;
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint_, id_);
    return true;
}

void UniformBuffer::Destroy() {
    if (id_ != 0) {
        glDeleteBuffers(1, &id_);
        id_ = 0;
    }
    bindingPoint_ = 0;
    sizeBytes_ = 0;
}

void UniformBuffer::Update(const void* data, std::size_t sizeBytes) const {
    if (!Valid() || data == nullptr || sizeBytes == 0 || sizeBytes > sizeBytes_) {
        return;
    }
    glBindBuffer(GL_UNIFORM_BUFFER, id_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(sizeBytes), data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UniformBuffer::Bind() const {
    if (Valid()) {
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint_, id_);
    }
}

} // namespace leon
