#pragma once

#include "CoreMinimal.h"

class UTexture2D;

enum class EMaterialShadingModel
{
	BlinnPhong,
	Unlit,
};

/** Roughness from Blinn shininess (high Ns gives sharp reflections). */
[[nodiscard]] inline float RoughnessFromShininess(float InShininess)
{
	const float S = FMath::Max(InShininess, 1.0f);
	return FMath::Clamp(FMath::Sqrt(2.0f / (S + 2.0f)), 0.04f, 1.0f);
}

/**
 * Per-object surface for the forward lit pass.
 * Specular / Metallic + Roughness drive the Blinn highlights.
 * UvScale tiles the albedo / normal maps (UE-like material instance tiling).
 * Colors are linear RGB triples.
 */
struct RENDERCORE_API FMaterial
{
	EMaterialShadingModel Shading = EMaterialShadingModel::BlinnPhong;
	FVector Albedo = FVector(0.55f, 0.72f, 0.85f);
	FVector Specular = FVector(0.04f, 0.04f, 0.04f); // F0 / MTL Ks (dielectric default ~4%)
	float Metallic = 0.0f; // 0 = dielectric, 1 = metal (tints specular, kills diffuse)
	float Alpha = 1.0f; // < 1: transparent queue (back to front)
	float Shininess = 32.0f;
	float Roughness = RoughnessFromShininess(32.0f); // 0 = mirror, 1 = fully blurred
	FVector2D UvScale = FVector2D(1.0f, 1.0f); // multiplies mesh UVs when sampling maps
	bool bCastsShadows = true;
	bool bPlanarMirror = false; // horizontal ground mirror (scene planar reflection pass)
	TSharedPtr<UTexture2D> AlbedoMap; // optional; white if null
	TSharedPtr<UTexture2D> NormalMap; // optional; flat (+Z) if null

	[[nodiscard]] bool IsTransparent() const
	{
		return Alpha < 0.999f;
	}

	void SyncRoughnessFromShininess()
	{
		Roughness = RoughnessFromShininess(Shininess);
	}
};
