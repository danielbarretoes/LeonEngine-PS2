#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialInterface.h"
#include "UObject/NoExportTypes.h"
#include "Material.generated.h"

class UTexture2D;

/**
 * A material asset (UE: UMaterial). Leon's materials are fixed shading models with parameters and textures, the
 * parameters the legacy `.lmat` files had (plan decision in P14): UE builds a material from a graph of expressions
 * compiled to shaders, which Leon does not have (a documented deviation). Every default equals the renderer's default
 * FMaterial, so GetRenderProxy of a new material draws what an unset material slot always drew.
 */
UCLASS()
class ENGINE_API UMaterial : public UMaterialInterface
{
	GENERATED_BODY()

public:
	UMaterial(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** How the material is lit (UE: ShadingModel). */
	UPROPERTY()
	TEnumAsByte<EMaterialShadingModel> ShadingModel = MSM_DefaultLit;

	/** Linear RGB albedo; multiplies BaseColorMap (UE: the BaseColor input). */
	UPROPERTY()
	FLinearColor BaseColor = FLinearColor(0.55f, 0.72f, 0.85f, 1.0f);

	/** Linear RGB specular reflectance at normal incidence, F0 (UE: the Specular input, a scalar there). */
	UPROPERTY()
	FLinearColor Specular = FLinearColor(0.04f, 0.04f, 0.04f, 1.0f);

	/** 0 = dielectric, 1 = metal: tints the specular and removes the diffuse (UE: the Metallic input). */
	UPROPERTY()
	float Metallic = 0.0f;

	/** 0 = mirror, 1 = fully blurred; the importers clamp it to [0.04, 1] (UE: the Roughness input). */
	UPROPERTY()
	float Roughness = RoughnessFromShininess(32.0f);

	/** Below 1 the surface is drawn in the transparent pass, back to front (UE: the Opacity input). */
	UPROPERTY()
	float Opacity = 1.0f;

	/** Blinn-Phong exponent the shaders also read (Leon). */
	UPROPERTY()
	float Shininess = 32.0f;

	/** Multiplies the mesh UVs when sampling the maps (Leon: a material instance's tiling parameter). */
	UPROPERTY()
	FVector2D UVScale = FVector2D(1.0f, 1.0f);

	/** Opaque lit sections with this material cast shadows (Leon; UE decides per primitive). */
	UPROPERTY()
	bool bCastsShadows = true;

	/** A horizontal mirror: the scene's planar reflection pass draws into it (Leon). */
	UPROPERTY()
	bool bPlanarMirror = false;

	/** The albedo map, white when null. */
	UPROPERTY()
	UTexture2D* BaseColorMap = nullptr;

	/** The tangent-space normal map, flat when null. */
	UPROPERTY()
	UTexture2D* NormalMap = nullptr;

	// UMaterialInterface
	UMaterial* GetMaterial() override
	{
		return this;
	}
	const UMaterial* GetMaterial() const override
	{
		return this;
	}
	/** The parameters above as the renderer's FMaterial (every value copied as it is). */
	FMaterial GetRenderProxy() const override;
	void GetUsedTextures(TArray<UTexture*>& OutTextures) const override;

	/** Takes every value of a renderer FMaterial, its maps included (Leon: what the importers read). */
	void SetFromRenderProxy(const FMaterial& Values);

	/** True when the material is drawn in the transparent pass (UE: IsTranslucentBlendMode of its blend mode). */
	[[nodiscard]] bool IsTranslucent() const
	{
		return GetRenderProxy().IsTransparent();
	}

	/**
	 * The engine's default material (UE: GetDefaultMaterial): UEngine::DefaultMaterialName, `[/Script/Engine.Engine]
	 * DefaultMaterialName=` in the engine config (`/Engine/EngineMaterials/M_Default`), loaded once from its package
	 * and kept in the root set. What a mesh slot without a material draws with; the `.llev` basic shapes use it. Leon
	 * has one domain, MD_Surface.
	 */
	[[nodiscard]] static UMaterial* GetDefaultMaterial(EMaterialDomain Domain);
};
