#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialInterface.h"
#include "UObject/NoExportTypes.h"
#include "Material.generated.h"

class UPhysicalMaterial;
class UTexture2D;

/**
 * A material asset (UE: UMaterial). Leon's materials are a fixed shading model with the parameters the GS scene
 * renderer draws with (a colour, an opacity, an albedo map and its tiling): UE builds a material from a graph of
 * expressions compiled to shaders, which the PS2 does not have (a documented deviation). Every default equals the
 * renderer's default FMaterial, so GetRenderProxy of a new material draws what an unset material slot always drew.
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

	/** Below 1 the surface is drawn translucent, back to front (UE: the Opacity input). */
	UPROPERTY()
	float Opacity = 1.0f;

	/** Multiplies the mesh UVs when sampling the map (Leon: a material instance's tiling parameter). */
	UPROPERTY()
	FVector2D UVScale = FVector2D(1.0f, 1.0f);

	/** The albedo map, the base colour alone when null. */
	UPROPERTY()
	UTexture2D* BaseColorMap = nullptr;

	/**
	 * BaseColorMap samples its mip chain, trilinear (UE: the texture's MipGenSettings and the sampler's filter);
	 * off: its level 0 only, bilinear.
	 */
	UPROPERTY()
	bool bMipmaps = true;

	/** Added to BaseColorMap's level of detail, in mip levels (UE: the MipBias of a texture sample; +1 blurrier). */
	UPROPERTY()
	float LodBias = 0.0f;

	/**
	 * What the surfaces drawn with it are made of (UE: PhysMaterial): the traces that ask for it report it
	 * (FHitResult::PhysMaterial). The glTF import sets it from the source material's extras (`physMaterial`).
	 */
	UPROPERTY()
	UPhysicalMaterial* PhysMaterial = nullptr;

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
	[[nodiscard]] UPhysicalMaterial* GetPhysicalMaterial() const override
	{
		return PhysMaterial;
	}

	/** Takes every value of a renderer FMaterial, its map included (Leon: what the importers read). */
	void SetFromRenderProxy(const FMaterial& Values);

	/** True when the material is drawn in the transparent pass (UE: IsTranslucentBlendMode of its blend mode). */
	[[nodiscard]] bool IsTranslucent() const
	{
		return GetRenderProxy().IsTransparent();
	}

	/**
	 * The engine's default material (UE: GetDefaultMaterial): UEngine::DefaultMaterialName, `[/Script/Engine.Engine]
	 * DefaultMaterialName=` in the engine config (`/Engine/EngineMaterials/M_Default`), loaded once from its package
	 * and kept in the root set. What a mesh slot without a material draws with; the basic shapes use it. Leon
	 * has one domain, MD_Surface.
	 */
	[[nodiscard]] static UMaterial* GetDefaultMaterial(EMaterialDomain Domain);
};
