#include <glad/glad.h>

#include <leon/render/GpuPassTimer.h>

namespace leon {
namespace {

bool isTimedPass(GpuPassTimer::EPass pass) {
    return pass == GpuPassTimer::EPass::Shadow || pass == GpuPassTimer::EPass::Planar ||
           pass == GpuPassTimer::EPass::Color || pass == GpuPassTimer::EPass::Ssao ||
           pass == GpuPassTimer::EPass::Post;
}

int passIndex(GpuPassTimer::EPass pass) {
    switch (pass) {
    case GpuPassTimer::EPass::Shadow:
        return 0;
    case GpuPassTimer::EPass::Planar:
        return 1;
    case GpuPassTimer::EPass::Color:
        return 2;
    case GpuPassTimer::EPass::Ssao:
        return 3;
    case GpuPassTimer::EPass::Post:
        return 4;
    case GpuPassTimer::EPass::Count:
        break;
    }
    return 0;
}

} // namespace

GpuPassTimer::QueryBuffer& GpuPassTimer::bufferQueries(int buffer) {
    return buffer == 0 ? queries_[0] : queries_[1];
}

bool& GpuPassTimer::bufferPending(int buffer) {
    return buffer == 0 ? pending_[0] : pending_[1];
}

rhi::RHIQueryId& GpuPassTimer::querySlot(int buffer, EPass pass) {
    return bufferQueries(buffer)[static_cast<std::size_t>(passIndex(pass))];
}

bool& GpuPassTimer::passOpenSlot(EPass pass) {
    return passOpen_[static_cast<std::size_t>(passIndex(pass))];
}

float& GpuPassTimer::msSlot(EPass pass) {
    return ms_[static_cast<std::size_t>(passIndex(pass))];
}

const float& GpuPassTimer::msSlot(EPass pass) const {
    return ms_[static_cast<std::size_t>(passIndex(pass))];
}

bool GpuPassTimer::resolveBuffer(const QueryBuffer& queries) {
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

GpuPassTimer::~GpuPassTimer() {
    Destroy();
}

bool GpuPassTimer::Create() {
    Destroy();
    glGenQueries(kBufferCount * kPassCount, queries_[0].data());
    ms_.fill(0.0f);
    pending_.fill(false);
    passOpen_.fill(false);
    writeBuffer_ = 0;
    created_ = true;
    return true;
}

void GpuPassTimer::Destroy() {
    if (!created_) {
        return;
    }
    glDeleteQueries(kBufferCount * kPassCount, queries_[0].data());
    for (QueryBuffer& buffer : queries_) {
        buffer.fill(0);
    }
    created_ = false;
}

void GpuPassTimer::BeginFrame() {
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

void GpuPassTimer::Begin(EPass pass) {
    if (!created_ || !isTimedPass(pass) || passOpenSlot(pass)) {
        return;
    }
    glBeginQuery(GL_TIME_ELAPSED, querySlot(writeBuffer_, pass));
    passOpenSlot(pass) = true;
}

void GpuPassTimer::End(EPass pass) {
    if (!created_ || !isTimedPass(pass) || !passOpenSlot(pass)) {
        return;
    }
    glEndQuery(GL_TIME_ELAPSED);
    passOpenSlot(pass) = false;
    bufferPending(writeBuffer_) = true;
}

float GpuPassTimer::Milliseconds(EPass pass) const {
    if (!isTimedPass(pass)) {
        return 0.0f;
    }
    return msSlot(pass);
}

} // namespace leon
