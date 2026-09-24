#pragma once

#include "RHIHandles.h"

#include <array>
#include <cstdint>

/// Double-buffered GL_TIME_ELAPSED queries so HUD reads last frame (no GPU stall).
class RENDERER_API FGPUPassTimer
{
public:
	enum class EPass : std::uint8_t
	{
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
	void Begin(EPass Pass);
	void End(EPass Pass);

	[[nodiscard]] bool Valid() const
	{
		return bCreated;
	}
	[[nodiscard]] float Milliseconds(EPass Pass) const;

private:
	static constexpr int BufferCount = 2;
	static constexpr auto PassCount = static_cast<int>(EPass::Count);

	using FQueryBuffer = std::array<FRHIQueryId, PassCount>;

	[[nodiscard]] FQueryBuffer& BufferQueries(int Buffer);
	[[nodiscard]] bool& BufferPending(int Buffer);
	[[nodiscard]] FRHIQueryId& QuerySlot(int Buffer, EPass Pass);
	[[nodiscard]] bool& PassOpenSlot(EPass Pass);
	[[nodiscard]] float& MsSlot(EPass Pass);
	[[nodiscard]] const float& MsSlot(EPass Pass) const;
	/// Returns false if any query is still outstanding (no GPU stall).
	[[nodiscard]] bool ResolveBuffer(const FQueryBuffer& InQueries);

	std::array<FQueryBuffer, BufferCount> Queries{};
	std::array<float, PassCount> Ms{};
	int WriteBuffer = 0;
	std::array<bool, BufferCount> Pending{};
	bool bCreated = false;
	std::array<bool, PassCount> PassOpen{};
};
