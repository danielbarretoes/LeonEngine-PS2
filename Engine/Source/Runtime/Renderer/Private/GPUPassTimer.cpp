#include <glad/glad.h>

#include "GpuPassTimer.h"

namespace {

bool isTimedPass(FGPUPassTimer::EPass pass) {
    return pass == FGPUPassTimer::EPass::Shadow || pass == FGPUPassTimer::EPass::Planar ||
           pass == FGPUPassTimer::EPass::Color || pass == FGPUPassTimer::EPass::Ssao ||
           pass == FGPUPassTimer::EPass::Post;
}

int passIndex(FGPUPassTimer::EPass pass) {
    switch (pass) {
    case FGPUPassTimer::EPass::Shadow:
        return 0;
    case FGPUPassTimer::EPass::Planar:
        return 1;
    case FGPUPassTimer::EPass::Color:
        return 2;
    case FGPUPassTimer::EPass::Ssao:
        return 3;
    case FGPUPassTimer::EPass::Post:
        return 4;
    case FGPUPassTimer::EPass::Count:
        break;
    }
    return 0;
}

} // namespace

FGPUPassTimer::FQueryBuffer& FGPUPassTimer::bufferQueries(int buffer) {
    return buffer == 0 ? queries_[0] : queries_[1];
}

bool& FGPUPassTimer::bufferPending(int buffer) {
    return buffer == 0 ? pending_[0] : pending_[1];
}

FRHIQueryId& FGPUPassTimer::querySlot(int buffer, EPass pass) {
    return bufferQueries(buffer)[static_cast<std::size_t>(passIndex(pass))];
}

bool& FGPUPassTimer::passOpenSlot(EPass pass) {
    return passOpen_[static_cast<std::size_t>(passIndex(pass))];
}

float& FGPUPassTimer::msSlot(EPass pass) {
    return ms_[static_cast<std::size_t>(passIndex(pass))];
}

const float& FGPUPassTimer::msSlot(EPass pass) const {
    return ms_[static_cast<std::size_t>(passIndex(pass))];
}

bool FGPUPassTimer::resolveBuffer(const FQueryBuffer& queries) {
    // Non-blocking: skip until all queries are ready (keeps last frame's ms_).
    for (int i = 0; i < kPassCount; ++i) {
        GLint available = 0;
        glGetQueryObjectiv(queries[static_cast<std::size_t>(i)], GL_QUERY_RESULT_AVAILABLE,
                           &available);
        if (available != GL_TRUE) {
            return false;
        }
    }
    GLuint64 nanoseconds = 0;
    for (int i = 0; i < kPassCount; ++i) {
        glGetQueryObjectui64v(queries[static_cast<std::size_t>(i)], GL_QUERY_RESULT, &nanoseconds);
        ms_[static_cast<std::size_t>(i)] = static_cast<float>(nanoseconds) / 1.0e6f;
    }
    return true;
}

FGPUPassTimer::~FGPUPassTimer() {
    Destroy();
}

bool FGPUPassTimer::Create() {
    Destroy();
    glGenQueries(kBufferCount * kPassCount, queries_[0].data());
    ms_.fill(0.0f);
    pending_.fill(false);
    passOpen_.fill(false);
    writeBuffer_ = 0;
    created_ = true;
    return true;
}

void FGPUPassTimer::Destroy() {
    if (!created_) {
        return;
    }
    glDeleteQueries(kBufferCount * kPassCount, queries_[0].data());
    for (FQueryBuffer& buffer : queries_) {
        buffer.fill(0);
    }
    created_ = false;
}

void FGPUPassTimer::BeginFrame() {
    if (!created_) {
        return;
    }

    // Resolve the buffer completed on the previous frame (still selected as writeBuffer_).
    if (bufferPending(writeBuffer_)) {
        if (!resolveBuffer(bufferQueries(writeBuffer_))) {
            // GPU still working — skip issuing new queries this frame (no stall).
            passOpen_.fill(false);
            return;
        }
        bufferPending(writeBuffer_) = false;
    }

    writeBuffer_ = 1 - writeBuffer_;
    bufferPending(writeBuffer_) = false;
    passOpen_.fill(false);
}

void FGPUPassTimer::Begin(EPass pass) {
    if (!created_ || !isTimedPass(pass) || passOpenSlot(pass)) {
        return;
    }
    glBeginQuery(GL_TIME_ELAPSED, querySlot(writeBuffer_, pass));
    passOpenSlot(pass) = true;
}

void FGPUPassTimer::End(EPass pass) {
    if (!created_ || !isTimedPass(pass) || !passOpenSlot(pass)) {
        return;
    }
    glEndQuery(GL_TIME_ELAPSED);
    passOpenSlot(pass) = false;
    bufferPending(writeBuffer_) = true;
}

float FGPUPassTimer::Milliseconds(EPass pass) const {
    if (!isTimedPass(pass)) {
        return 0.0f;
    }
    return msSlot(pass);
}

