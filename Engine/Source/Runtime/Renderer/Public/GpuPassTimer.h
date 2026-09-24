#pragma once

#include <array>
#include <cstdint>
#include "RHIHandles.h"

namespace leon {

/// Double-buffered GL_TIME_ELAPSED queries so HUD reads last frame (no GPU stall).
class GpuPassTimer {
public:
    enum class EPass : std::uint8_t {
        Shadow = 0,
        Planar = 1,
        Color = 2,
        Ssao = 3,
        Post = 4,
        Count = 5,
    };

    GpuPassTimer() = default;
    ~GpuPassTimer();

    GpuPassTimer(const GpuPassTimer&) = delete;
    GpuPassTimer& operator=(const GpuPassTimer&) = delete;

    [[nodiscard]] bool Create();
    void Destroy();

    /// Swap buffers and resolve the previous frame into Milliseconds().
    void BeginFrame();
    void Begin(EPass pass);
    void End(EPass pass);

    [[nodiscard]] bool Valid() const { return created_; }
    [[nodiscard]] float Milliseconds(EPass pass) const;

private:
    static constexpr int kBufferCount = 2;
    static constexpr auto kPassCount = static_cast<int>(EPass::Count);

    using QueryBuffer = std::array<rhi::RHIQueryId, kPassCount>;

    [[nodiscard]] QueryBuffer& bufferQueries(int buffer);
    [[nodiscard]] bool& bufferPending(int buffer);
    [[nodiscard]] rhi::RHIQueryId& querySlot(int buffer, EPass pass);
    [[nodiscard]] bool& passOpenSlot(EPass pass);
    [[nodiscard]] float& msSlot(EPass pass);
    [[nodiscard]] const float& msSlot(EPass pass) const;
    /// Returns false if any query is still outstanding (no GPU stall).
    [[nodiscard]] bool resolveBuffer(const QueryBuffer& queries);

    std::array<QueryBuffer, kBufferCount> queries_{};
    std::array<float, kPassCount> ms_{};
    int writeBuffer_ = 0;
    std::array<bool, kBufferCount> pending_{};
    bool created_ = false;
    std::array<bool, kPassCount> passOpen_{};
};

} // namespace leon
