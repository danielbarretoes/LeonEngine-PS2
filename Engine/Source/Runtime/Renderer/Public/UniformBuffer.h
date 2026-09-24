#pragma once

#include "RHIHandles.h"

#include <cstddef>

/// GL_UNIFORM_BUFFER wrapper bound to a fixed binding point (OpenGL 3.3+).
class RENDERER_API FUniformBuffer
{
public:
	FUniformBuffer() = default;
	~FUniformBuffer();

	FUniformBuffer(const FUniformBuffer&) = delete;
	FUniformBuffer& operator=(const FUniformBuffer&) = delete;

	bool Create(std::size_t InSizeBytes, unsigned int InBindingPoint);
	void Destroy();

	void Update(const void* Data, std::size_t InSizeBytes) const;
	void Bind() const;

	[[nodiscard]] bool Valid() const
	{
		return Id != InvalidBuffer;
	}
	[[nodiscard]] unsigned int GetBindingPoint() const
	{
		return BindingPoint;
	}

private:
	FRHIBufferId Id = InvalidBuffer;
	unsigned int BindingPoint = 0;
	std::size_t SizeBytes = 0;
};
