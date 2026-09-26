#include "GPUPassTimer.h"

#include "HAL/UnrealMemory.h"

#include <glad/glad.h>

namespace
{

	bool IsTimedPass(FGPUPassTimer::EPass Pass)
	{
		return Pass == FGPUPassTimer::EPass::Shadow || Pass == FGPUPassTimer::EPass::Planar ||
			Pass == FGPUPassTimer::EPass::Color;
	}

	int PassIndex(FGPUPassTimer::EPass Pass)
	{
		switch (Pass)
		{
			case FGPUPassTimer::EPass::Shadow:
				return 0;
			case FGPUPassTimer::EPass::Planar:
				return 1;
			case FGPUPassTimer::EPass::Color:
				return 2;
			case FGPUPassTimer::EPass::Count:
				break;
		}
		return 0;
	}

} // namespace

FGPUPassTimer::FQueryBuffer& FGPUPassTimer::BufferQueries(int Buffer)
{
	return Buffer == 0 ? Queries[0] : Queries[1];
}

bool& FGPUPassTimer::BufferPending(int Buffer)
{
	return Buffer == 0 ? Pending[0] : Pending[1];
}

FRHIQueryId& FGPUPassTimer::QuerySlot(int Buffer, EPass Pass)
{
	return BufferQueries(Buffer)[static_cast<int32>(PassIndex(Pass))];
}

bool& FGPUPassTimer::PassOpenSlot(EPass Pass)
{
	return PassOpen[static_cast<int32>(PassIndex(Pass))];
}

float& FGPUPassTimer::MsSlot(EPass Pass)
{
	return Ms[static_cast<int32>(PassIndex(Pass))];
}

const float& FGPUPassTimer::MsSlot(EPass Pass) const
{
	return Ms[static_cast<int32>(PassIndex(Pass))];
}

bool FGPUPassTimer::ResolveBuffer(const FQueryBuffer& InQueries)
{
	// Non-blocking: skip until all queries are ready (keeps last frame's Ms).
	for (int I = 0; I < PassCount; ++I)
	{
		GLint Available = 0;
		glGetQueryObjectiv(InQueries[static_cast<int32>(I)], GL_QUERY_RESULT_AVAILABLE, &Available);
		if (Available != GL_TRUE)
		{
			return false;
		}
	}
	GLuint64 Nanoseconds = 0;
	for (int I = 0; I < PassCount; ++I)
	{
		glGetQueryObjectui64v(InQueries[static_cast<int32>(I)], GL_QUERY_RESULT, &Nanoseconds);
		Ms[static_cast<int32>(I)] = static_cast<float>(Nanoseconds) / 1.0e6f;
	}
	return true;
}

FGPUPassTimer::~FGPUPassTimer()
{
	Destroy();
}

bool FGPUPassTimer::Create()
{
	Destroy();
	glGenQueries(BufferCount * PassCount, Queries[0].Ids);
	FMemory::Memzero(Ms, sizeof(Ms));
	FMemory::Memzero(Pending, sizeof(Pending));
	FMemory::Memzero(PassOpen, sizeof(PassOpen));
	WriteBuffer = 0;
	bCreated = true;
	return true;
}

void FGPUPassTimer::Destroy()
{
	if (!bCreated)
	{
		return;
	}
	glDeleteQueries(BufferCount * PassCount, Queries[0].Ids);
	FMemory::Memzero(Queries, sizeof(Queries));
	bCreated = false;
}

void FGPUPassTimer::BeginFrame()
{
	if (!bCreated)
	{
		return;
	}

	// Resolve the buffer completed on the previous frame (still selected as WriteBuffer).
	if (BufferPending(WriteBuffer))
	{
		if (!ResolveBuffer(BufferQueries(WriteBuffer)))
		{
			// GPU still working — skip issuing new queries this frame (no stall).
			FMemory::Memzero(PassOpen, sizeof(PassOpen));
			return;
		}
		BufferPending(WriteBuffer) = false;
	}

	WriteBuffer = 1 - WriteBuffer;
	BufferPending(WriteBuffer) = false;
	FMemory::Memzero(PassOpen, sizeof(PassOpen));
}

void FGPUPassTimer::Begin(EPass Pass)
{
	if (!bCreated || !IsTimedPass(Pass) || PassOpenSlot(Pass))
	{
		return;
	}
	glBeginQuery(GL_TIME_ELAPSED, QuerySlot(WriteBuffer, Pass));
	PassOpenSlot(Pass) = true;
}

void FGPUPassTimer::End(EPass Pass)
{
	if (!bCreated || !IsTimedPass(Pass) || !PassOpenSlot(Pass))
	{
		return;
	}
	glEndQuery(GL_TIME_ELAPSED);
	PassOpenSlot(Pass) = false;
	BufferPending(WriteBuffer) = true;
}

float FGPUPassTimer::Milliseconds(EPass Pass) const
{
	if (!IsTimedPass(Pass))
	{
		return 0.0f;
	}
	return MsSlot(Pass);
}
