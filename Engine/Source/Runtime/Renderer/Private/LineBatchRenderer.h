#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDraw.h"
#include "Shader.h"

/**
 * Draws FDebugDraw line batches with the debug line shader (Leon; UE's line batch proxies render through the
 * dynamic mesh pass): one dynamic vertex buffer refilled per flush.
 */
class FLineBatchRenderer
{
public:
	bool Initialize();
	void Shutdown();
	/** Reloads the line shader from disk if its timestamps changed (or when forced). */
	[[nodiscard]] EShaderReloadResult ReloadShader(bool bForce = false);

	/** Draws the batch with a world to GL clip space view-projection (GLClipSpace.h). */
	void Flush(const FDebugDraw& Batch, const FMatrix& ViewProjection, bool bDepthTest = true) const;

	[[nodiscard]] bool IsValid() const
	{
		return Shader.Valid() && Vao != 0;
	}

private:
	FShader Shader;
	uint32 Vao = 0;
	uint32 Vbo = 0;
};
