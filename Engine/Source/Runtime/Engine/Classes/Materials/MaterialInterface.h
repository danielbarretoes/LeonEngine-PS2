#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "UObject/Object.h"
#include "MaterialInterface.generated.h"

class UMaterial;
class UTexture;

/**
 * The base of the material assets (UE: UMaterialInterface): what a mesh slot and a component's override material
 * hold. Leon has one kind, UMaterial (no material instances yet).
 */
UCLASS(Abstract)
class ENGINE_API UMaterialInterface : public UObject
{
	GENERATED_BODY()

public:
	UMaterialInterface(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The base material (UE: GetMaterial); null for the abstract base. */
	[[nodiscard]] virtual UMaterial* GetMaterial()
	{
		return nullptr;
	}
	[[nodiscard]] virtual const UMaterial* GetMaterial() const
	{
		return nullptr;
	}

	/**
	 * The values the renderer draws with (Leon: RenderCore's FMaterial, by value; UE's GetRenderProxy returns the
	 * FMaterialRenderProxy the render thread reads). A scene proxy keeps one per mesh section.
	 */
	[[nodiscard]] virtual FMaterial GetRenderProxy() const
	{
		return FMaterial();
	}

	/** The textures the material samples (UE: GetUsedTextures). */
	virtual void GetUsedTextures(TArray<UTexture*>& OutTextures) const
	{
		OutTextures.Reset();
	}
};
