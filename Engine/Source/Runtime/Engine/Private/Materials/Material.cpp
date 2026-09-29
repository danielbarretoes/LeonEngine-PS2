#include "Materials/Material.h"

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineLogs.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtrTemplates.h"

UMaterialInterface::UMaterialInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UMaterial::UMaterial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FMaterial UMaterial::GetRenderProxy() const
{
	FMaterial Values;
	Values.Shading = ShadingModel == MSM_Unlit ? EMaterialLightingModel::Unlit : EMaterialLightingModel::Lit;
	Values.Albedo = FVector(BaseColor.R, BaseColor.G, BaseColor.B);
	Values.Alpha = Opacity;
	Values.UvScale = UVScale;
	Values.AlbedoMap = BaseColorMap;
	Values.bMipmaps = bMipmaps;
	Values.LodBias = LodBias;
	return Values;
}

void UMaterial::GetUsedTextures(TArray<UTexture*>& OutTextures) const
{
	OutTextures.Reset();
	if (BaseColorMap != nullptr)
	{
		OutTextures.Add(BaseColorMap);
	}
}

UMaterial* UMaterial::GetDefaultMaterial(EMaterialDomain Domain)
{
	(void)Domain;
	static TWeakObjectPtr<UMaterial> DefaultMaterial;
	if (UMaterial* Existing = DefaultMaterial.Get())
	{
		return Existing;
	}
	// GEngine's config, or its class default object's before the engine exists (tests, tools): the same values.
	const UEngine& Engine = GEngine != nullptr ? *GEngine : *GetDefault<UEngine>();
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *Engine.DefaultMaterialName.ToString());
	if (Material == nullptr)
	{
		UE_LOG(LogEngine, Error, "The default material '%s' cannot be loaded; a plain material stands in for it",
			*Engine.DefaultMaterialName.ToString());
		Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
	}
	Material->AddToRoot();
	DefaultMaterial = Material;
	return Material;
}

void UMaterial::SetFromRenderProxy(const FMaterial& Values)
{
	ShadingModel = Values.Shading == EMaterialLightingModel::Unlit ? MSM_Unlit : MSM_DefaultLit;
	BaseColor = FLinearColor(Values.Albedo.X, Values.Albedo.Y, Values.Albedo.Z, 1.0f);
	Opacity = Values.Alpha;
	UVScale = Values.UvScale;
	BaseColorMap = Values.AlbedoMap;
	bMipmaps = Values.bMipmaps;
	LodBias = Values.LodBias;
}
