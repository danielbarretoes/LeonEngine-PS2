#include <glad/glad.h>

#include "UniformBuffer.h"


FUniformBuffer::~FUniformBuffer() {
    Destroy();
}

bool FUniformBuffer::Create(std::size_t InSizeBytes, unsigned int InBindingPoint) {
    Destroy();
    if (InSizeBytes == 0) {
        return false;
    }

    glGenBuffers(1, &Id);
    glBindBuffer(GL_UNIFORM_BUFFER, Id);
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(InSizeBytes), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    BindingPoint = InBindingPoint;
    SizeBytes = InSizeBytes;
    glBindBufferBase(GL_UNIFORM_BUFFER, BindingPoint, Id);
    return true;
}

void FUniformBuffer::Destroy() {
    if (Id != 0) {
        glDeleteBuffers(1, &Id);
        Id = 0;
    }
    BindingPoint = 0;
    SizeBytes = 0;
}

void FUniformBuffer::Update(const void* Data, std::size_t InSizeBytes) const {
    if (!Valid() || Data == nullptr || InSizeBytes == 0 || InSizeBytes > SizeBytes) {
        return;
    }
    glBindBuffer(GL_UNIFORM_BUFFER, Id);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(InSizeBytes), Data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void FUniformBuffer::Bind() const {
    if (Valid()) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BindingPoint, Id);
    }
}

