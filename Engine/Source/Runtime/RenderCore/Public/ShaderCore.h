#pragma once

#include "CoreMinimal.h"

/**
 * What a shader hot reload did (Leon; UE recompiles through the shader compiling manager). Engine asks the renderer
 * for it (IRendererModule::ReloadShaders) and the Renderer's shaders report it.
 */
enum class EShaderReloadResult : uint8
{
	Unchanged = 0,
	Reloaded = 1,
	Failed = 2,
};

[[nodiscard]] inline EShaderReloadResult MergeShaderReload(EShaderReloadResult A, EShaderReloadResult B)
{
	if (A == EShaderReloadResult::Failed || B == EShaderReloadResult::Failed)
	{
		return EShaderReloadResult::Failed;
	}
	if (A == EShaderReloadResult::Reloaded || B == EShaderReloadResult::Reloaded)
	{
		return EShaderReloadResult::Reloaded;
	}
	return EShaderReloadResult::Unchanged;
}
