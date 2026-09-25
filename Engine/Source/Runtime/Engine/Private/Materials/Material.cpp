#include "Materials/Material.h"

#include "Engine/Texture2D.h"

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
