#pragma once

#include "CoreTypes.h"
#include "RHIHandles.h"

/** GL_UNIFORM_BUFFER wrapper bound to a fixed binding point (OpenGL 3.3+). */
class RENDERER_API FUniformBuffer
{
public:
	FUniformBuffer() = default;
	~FUniformBuffer();

	FUniformBuffer(const FUniformBuffer&) = delete;
	FUniformBuffer& operator=(const FUniformBuffer&) = delete;

	bool Create(SIZE_T InSizeBytes, unsigned int InBindingPoint);
	void Destroy();

	void Update(const void* Data, SIZE_T InSizeBytes) const;
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
	SIZE_T SizeBytes = 0;
};
