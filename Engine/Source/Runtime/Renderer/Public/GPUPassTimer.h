#pragma once

#include <array>
#include <cstdint>
#include "RHIHandles.h"


/// Double-buffered GL_TIME_ELAPSED queries so HUD reads last frame (no GPU stall).
class FGPUPassTimer {
public:
    enum class EPass : std::uint8_t {
        Shadow = 0,
        Planar = 1,
        Color = 2,
        Ssao = 3,
        Post = 4,
        Count = 5,
    };

    FGPUPassTimer() = default;
    ~FGPUPassTimer();

    FGPUPassTimer(const FGPUPassTimer&) = delete;
    FGPUPassTimer& operator=(const FGPUPassTimer&) = delete;

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

    using FQueryBuffer = std::array<FRHIQueryId, kPassCount>;

    [[nodiscard]] FQueryBuffer& bufferQueries(int buffer);
    [[nodiscard]] bool& bufferPending(int buffer);
    [[nodiscard]] FRHIQueryId& querySlot(int buffer, EPass pass);
    [[nodiscard]] bool& passOpenSlot(EPass pass);
    [[nodiscard]] float& msSlot(EPass pass);
    [[nodiscard]] const float& msSlot(EPass pass) const;
    /// Returns false if any query is still outstanding (no GPU stall).
    [[nodiscard]] bool resolveBuffer(const FQueryBuffer& queries);

    std::array<FQueryBuffer, kBufferCount> queries_{};
    std::array<float, kPassCount> ms_{};
    int writeBuffer_ = 0;
    std::array<bool, kBufferCount> pending_{};
    bool created_ = false;
    std::array<bool, kPassCount> passOpen_{};
};

