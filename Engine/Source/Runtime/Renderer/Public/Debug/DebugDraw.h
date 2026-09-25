#pragma once

#include "CoreMinimal.h"
#include "Shader.h"

/** Immediate-mode colored line batch for 3D debug (AABBs, light frustum, etc.). Colors are linear RGB. */
class RENDERER_API FDebugDraw
{
public:
	bool Initialize(const FString& ShaderDirectory);
	void Shutdown();
	/** Reloads the line shader from disk if its timestamps changed (or when forced). */
	[[nodiscard]] EShaderReloadResult ReloadShader(bool bForce = false);

	void Clear();
	void AddLine(const FVector& A, const FVector& B, const FLinearColor& InColor);
	/** Shaft + V-shaped head for a world-space direction vector. */
	void AddArrow(const FVector& From, const FVector& To, const FLinearColor& InColor, float HeadLength = 0.28f,
		float HeadWidth = 0.14f);
	void AddAabb(const FVector& WorldMin, const FVector& WorldMax, const FLinearColor& InColor);
	/** RGB axes at a scene component's world location (editor / PIE debug). */
	void AddAxes(const FVector& Origin, float Size = 0.35f);
	/** Clip-space cube (+-1) transformed by inverse(LightSpace): the world-space ortho frustum (GL convention). */
	void AddLightFrustum(const FMatrix& LightSpace, const FLinearColor& InColor);

	/** Draws the batch with a GL-convention view-projection (LegacyGLMath.h). */
	void Flush(const FMatrix& ViewProjection) const;

	[[nodiscard]] bool IsValid() const
	{
		return Shader.Valid() && Vao != 0;
	}
	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.Num() == 0;
	}

private:
	struct FLineVertex
	{
		FVector Position = FVector::ZeroVector;
		FVector Color = FVector::ZeroVector;
	};

	FShader Shader;
	uint32 Vao = 0;
	uint32 Vbo = 0;
	TArray<FLineVertex> Vertices;
};
