#pragma once

// The render side of a material (UE: MaterialShared.h, which declares FMaterial and FMaterialRenderProxy; the
// material asset is Engine's UMaterial, Materials/Material.h).

#include "CoreMinimal.h"

class UTexture2D;

/**
 * How the scene renderer lights a surface: the render side of a UMaterial's shading model (MSM_DefaultLit draws Lit,
 * MSM_Unlit Unlit). Leon keeps UE3's name for it so it does not clash with Engine's EMaterialShadingModel.
 */
enum class EMaterialLightingModel
{
	/** Per-vertex diffuse light: the ambient share plus the directional and point lights. */
	Lit,
	/** The albedo as it is. */
	Unlit,
};

/**
 * The values a material draws a section with (UE: what a FMaterialRenderProxy passes; a UMaterial makes one with
 * GetRenderProxy, and a scene proxy keeps one per section). The GS scene renderer reads all of them.
 * Colors are linear RGB triples.
 *
 * The albedo map is a texture asset (a UObject) these values do not own: whoever made the values keeps it alive.
 */
struct RENDERCORE_API FMaterial
{
	EMaterialLightingModel Shading = EMaterialLightingModel::Lit;
	FVector Albedo = FVector(0.55f, 0.72f, 0.85f);
	/** Below 1 the section is drawn translucent, back to front. */
	float Alpha = 1.0f;
	/** Multiplies the mesh UVs when sampling the albedo map (UE-like material instance tiling). */
	FVector2D UvScale = FVector2D(1.0f, 1.0f);
	/** Optional; the albedo alone when null. */
	UTexture2D* AlbedoMap = nullptr;
	/** The albedo map samples its MIPMAP levels (trilinear); off: level 0 only, bilinear. */
	bool bMipmaps = true;
	/** Added to the albedo map's level of detail (in levels: +1 is half the texels, blurrier; -1 sharper). */
	float LodBias = 0.0f;

	[[nodiscard]] bool IsTransparent() const
	{
		return Alpha < 0.999f;
	}
};
