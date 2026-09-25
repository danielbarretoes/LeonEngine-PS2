#pragma once

// The render side of a material (UE: MaterialShared.h, which declares FMaterial and FMaterialRenderProxy; the
// material asset is Engine's UMaterial, Materials/Material.h).

#include "CoreMinimal.h"

class UTexture2D;

/**
 * How the forward pass lights a surface: the render side of a UMaterial's shading model (MSM_DefaultLit draws with
 * BlinnPhong, MSM_Unlit with Unlit). Leon keeps UE3's name for it so it does not clash with Engine's
 * EMaterialShadingModel.
 */
enum class EMaterialLightingModel
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
 * Per-object surface for the forward lit pass: the values a material gives the shaders (UE: what a
 * FMaterialRenderProxy passes; a UMaterial makes one with GetRenderProxy, and a scene proxy keeps one per section).
 * Specular / Metallic + Roughness drive the Blinn highlights.
 * UvScale tiles the albedo / normal maps (UE-like material instance tiling).
 * Colors are linear RGB triples.
 *
 * The maps are texture assets (UObjects) these values do not own: whoever made the values keeps the textures alive.
 */
struct RENDERCORE_API FMaterial
{
	EMaterialLightingModel Shading = EMaterialLightingModel::BlinnPhong;
	FVector Albedo = FVector(0.55f, 0.72f, 0.85f);
	FVector Specular = FVector(0.04f, 0.04f, 0.04f); // F0 / MTL Ks (dielectric default ~4%)
	float Metallic = 0.0f; // 0 = dielectric, 1 = metal (tints specular, kills diffuse)
	float Alpha = 1.0f; // < 1: transparent queue (back to front)
	float Shininess = 32.0f;
	float Roughness = RoughnessFromShininess(32.0f); // 0 = mirror, 1 = fully blurred
	FVector2D UvScale = FVector2D(1.0f, 1.0f); // multiplies mesh UVs when sampling maps
	bool bCastsShadows = true;
	bool bPlanarMirror = false; // horizontal ground mirror (scene planar reflection pass)
	UTexture2D* AlbedoMap = nullptr; // optional; white if null
	UTexture2D* NormalMap = nullptr; // optional; flat (+Z) if null

	[[nodiscard]] bool IsTransparent() const
	{
		return Alpha < 0.999f;
	}

	void SyncRoughnessFromShininess()
	{
		Roughness = RoughnessFromShininess(Shininess);
	}
};
