#include "Materials/Material.h"

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineLogs.h"
#include "LegacyAssetLoader.h"
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
	Values.Shading = ShadingModel == MSM_Unlit ? EMaterialLightingModel::Unlit : EMaterialLightingModel::BlinnPhong;
	Values.Albedo = FVector(BaseColor.R, BaseColor.G, BaseColor.B);
	Values.Specular = FVector(Specular.R, Specular.G, Specular.B);
	Values.Metallic = Metallic;
	Values.Alpha = Opacity;
	Values.Shininess = Shininess;
	Values.Roughness = Roughness;
	Values.UvScale = UVScale;
	Values.bCastsShadows = bCastsShadows;
	Values.bPlanarMirror = bPlanarMirror;
	Values.AlbedoMap = BaseColorMap;
	Values.NormalMap = NormalMap;
	return Values;
}

void UMaterial::GetUsedTextures(TArray<UTexture*>& OutTextures) const
{
	OutTextures.Reset();
	if (BaseColorMap != nullptr)
	{
		OutTextures.Add(BaseColorMap);
	}
	if (NormalMap != nullptr)
	{
		OutTextures.Add(NormalMap);
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
	UMaterial* Material = FLegacyAssetLoader::LoadEngineObject<UMaterial>(Engine.DefaultMaterialName);
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
	Specular = FLinearColor(Values.Specular.X, Values.Specular.Y, Values.Specular.Z, 1.0f);
	Metallic = Values.Metallic;
	Opacity = Values.Alpha;
	Shininess = Values.Shininess;
	Roughness = Values.Roughness;
	UVScale = Values.UvScale;
	bCastsShadows = Values.bCastsShadows;
	bPlanarMirror = Values.bPlanarMirror;
	BaseColorMap = Values.AlbedoMap;
	NormalMap = Values.NormalMap;
}
