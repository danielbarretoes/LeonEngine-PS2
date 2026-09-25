#pragma once

#include "CoreMinimal.h"
#include "Shader.h"

class FCanvas;

/**
 * Draws an FCanvas's triangles over the frame with the overlay shader, alpha blended, no depth test (Leon; UE's
 * canvas renders its batched elements itself through the RHI).
 */
class FCanvasRenderer
{
public:
	bool Initialize();
	void Shutdown();
	[[nodiscard]] EShaderReloadResult ReloadShader(bool bForce = false);

	/** Draws the canvas into the bound draw framebuffer (the canvas size is the viewport). */
	void Draw(const FCanvas& Canvas);

	[[nodiscard]] bool IsValid() const
	{
		return Shader.Valid() && Vao != 0;
	}

private:
	FShader Shader;
	uint32 Vao = 0;
	uint32 Vbo = 0;
};
