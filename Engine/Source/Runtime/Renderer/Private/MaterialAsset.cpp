#include "MaterialAsset.h"

#include "Dom/JsonObject.h"
#include "LeonMaterialFormat.h"
#include "Misc/Paths.h"
#include "RendererLog.h"
#include "ResourceCache.h"

namespace
{

	/** A numeric [x, y, z] array, else Fallback. */
	FVector ReadVec3(const FJsonObject& Object, const FString& Field, const FVector& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.TryGetArrayField(Field, Values) || Values->Num() < 3)
		{
			return Fallback;
		}
		return FVector(static_cast<float>((*Values)[0]->AsNumber()), static_cast<float>((*Values)[1]->AsNumber()),
			static_cast<float>((*Values)[2]->AsNumber()));
	}

	void ApplyMaterialMaps(FResourceCache& Resources, FMaterial& Material, const FJsonObject& Object)
	{
		FString Key;
		if (Object.TryGetStringField("albedoMap", Key))
		{
			if (Key.Equals("checker", ESearchCase::CaseSensitive))
			{
				Material.AlbedoMap = Resources.CheckerTexture(64);
			}
			else
			{
				Material.AlbedoMap = Resources.LoadTexture(FPaths::ResolveLegacyContentPath(Key));
			}
		}
		if (Object.TryGetStringField("normalMap", Key))
		{
			if (Key.Equals("bump", ESearchCase::CaseSensitive))
			{
				Material.NormalMap = Resources.BumpNormalTexture(256);
			}
			else
			{
				Material.NormalMap = Resources.LoadTexture(FPaths::ResolveLegacyContentPath(Key));
			}
		}
	}

} // namespace

bool HasMaterialSurfaceFields(const FJsonObject& Spec)
{
	return Spec.HasField("albedo") || Spec.HasField("alpha") || Spec.HasField("specular") ||
		Spec.HasField("metallic") || Spec.HasField("shininess") || Spec.HasField("roughness") ||
		Spec.HasField("unlit") || Spec.HasField("albedoMap") || Spec.HasField("normalMap") ||
		Spec.HasField("uvScale") || Spec.HasField("tiling");
}

void PatchMaterialFromJson(FResourceCache& Resources, FMaterial& Material, const FJsonObject& Spec)
{
	bool bUnlit = false;
	if (Spec.TryGetBoolField("unlit", bUnlit) && bUnlit)
	{
		Material.Shading = EMaterialShadingModel::Unlit;
	}
	Material.Albedo = ReadVec3(Spec, "albedo", Material.Albedo);
	Material.Specular = ReadVec3(Spec, "specular", Material.Specular);
	(void)Spec.TryGetNumberField("metallic", Material.Metallic);
	(void)Spec.TryGetNumberField("alpha", Material.Alpha);
	if (Spec.TryGetNumberField("shininess", Material.Shininess) && !Spec.HasField("roughness"))
	{
		Material.SyncRoughnessFromShininess();
	}
	float Roughness = Material.Roughness;
	if (Spec.TryGetNumberField("roughness", Roughness))
	{
		Material.Roughness = FMath::Clamp(Roughness, 0.04f, 1.0f);
	}
	const FString UvField = Spec.HasField("uvScale") ? FString("uvScale") : FString("tiling");
	float UvScalar = 0.0f;
	const TArray<TSharedPtr<FJsonValue>>* UvValues = nullptr;
	if (Spec.TryGetNumberField(UvField, UvScalar))
	{
		Material.UvScale = FVector2D(UvScalar, UvScalar);
	}
	else if (Spec.TryGetArrayField(UvField, UvValues) && UvValues->Num() >= 2)
	{
		Material.UvScale =
			FVector2D(static_cast<float>((*UvValues)[0]->AsNumber()), static_cast<float>((*UvValues)[1]->AsNumber()));
	}
	(void)Spec.TryGetBoolField("castsShadows", Material.bCastsShadows);
	(void)Spec.TryGetBoolField("planarMirror", Material.bPlanarMirror);
	ApplyMaterialMaps(Resources, Material, Spec);
}

bool LoadMaterialFile(FResourceCache& Resources, const FString& Path, FMaterial& Out)
{
	if (!IsLeonMaterialPath(Path))
	{
		UE_LOG(LogRenderer, Error, "MaterialAsset: expected .lmat, got '%s'", *Path);
		return false;
	}
	return LoadLeonMaterialFile(Resources, Path, Out);
}

FMaterial MakeDefaultCheckerMaterial(FResourceCache& Resources)
{
	FMaterial Material;
	Material.Shading = EMaterialShadingModel::BlinnPhong;
	Material.Albedo = FVector(1.0f, 1.0f, 1.0f);
	Material.Specular = FVector(0.04f, 0.04f, 0.04f);
	Material.Metallic = 0.0f;
	Material.Shininess = 8.0f;
	Material.SyncRoughnessFromShininess();
	Material.bCastsShadows = true;
	Material.bPlanarMirror = false;
	Material.AlbedoMap = Resources.CheckerTexture(64);
	return Material;
}
