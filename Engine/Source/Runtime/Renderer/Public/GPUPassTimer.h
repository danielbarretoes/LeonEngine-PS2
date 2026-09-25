#pragma once

#include "CoreTypes.h"
#include "RHIHandles.h"

/// Double-buffered GL_TIME_ELAPSED queries so HUD reads last frame (no GPU stall).
class RENDERER_API FGPUPassTimer
{
public:
	enum class EPass : uint8
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
	static constexpr int32 BufferCount = 2;
	static constexpr int32 PassCount = static_cast<int32>(EPass::Count);

	struct FQueryBuffer
	{
		FRHIQueryId Ids[PassCount] = {};

		FRHIQueryId& operator[](int32 Index)
		{
			return Ids[Index];
		}
		const FRHIQueryId& operator[](int32 Index) const
		{
			return Ids[Index];
		}
	};

	[[nodiscard]] FQueryBuffer& BufferQueries(int Buffer);
	[[nodiscard]] bool& BufferPending(int Buffer);
	[[nodiscard]] FRHIQueryId& QuerySlot(int Buffer, EPass Pass);
	[[nodiscard]] bool& PassOpenSlot(EPass Pass);
	[[nodiscard]] float& MsSlot(EPass Pass);
	[[nodiscard]] const float& MsSlot(EPass Pass) const;
	/// Returns false if any query is still outstanding (no GPU stall).
	[[nodiscard]] bool ResolveBuffer(const FQueryBuffer& InQueries);

	FQueryBuffer Queries[BufferCount]{};
	float Ms[PassCount]{};
	int WriteBuffer = 0;
	bool Pending[BufferCount]{};
	bool bCreated = false;
	bool PassOpen[PassCount]{};
};
