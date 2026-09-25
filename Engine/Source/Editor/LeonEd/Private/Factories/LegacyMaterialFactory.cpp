#include "Factories/LegacyMaterialFactory.h"

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "LeonEdLog.h"
#include "Level/LegacyAssetKeys.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Misc/CString.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace
{

	/** A parsed `.lmat`: the parameters, and the map keys as they are written. */
	struct FLegacyMaterialDocument
	{
		FString Name = TEXT("Material");
		FMaterial Material{};
		FString BaseColorMapKey;
		FString NormalMapKey;
	};

	/** Drops a '#' or ';' comment and the surrounding whitespace. */
	FString StripComment(const FString& Line)
	{
		int32 Cut = Line.Len();
		int32 Hash = INDEX_NONE;
		if (Line.FindChar('#', Hash))
		{
			Cut = Hash;
		}
		int32 Semi = INDEX_NONE;
		if (Line.Left(Cut).FindChar(';', Semi))
		{
			Cut = Semi;
		}
		return Line.Left(Cut).TrimStartAndEnd();
	}

	bool ParseBool(const FString& Value, bool bFallback)
	{
		const FString S = Value.ToLower();
		if (S == "1" || S == "true" || S == "yes" || S == "on")
		{
			return true;
		}
		if (S == "0" || S == "false" || S == "no" || S == "off")
		{
			return false;
		}
		return bFallback;
	}

	/** A leading float (like std::stof): true when at least one character was consumed. */
	bool ParseFloatAt(const ANSICHAR*& Cursor, float& Out)
	{
		ANSICHAR* End = nullptr;
		const float Value = FCStringAnsi::Strtof(Cursor, &End);
		if (End == Cursor)
		{
			return false;
		}
		Out = Value;
		Cursor = End;
		return true;
	}

	bool ParseFloat(const FString& Value, float& Out)
	{
		const ANSICHAR* Cursor = *Value;
		return ParseFloatAt(Cursor, Out);
	}

	/** The next non-whitespace character (consumed), or '\0' at the end. */
	ANSICHAR NextChar(const ANSICHAR*& Cursor)
	{
		while (*Cursor != '\0' && FCharAnsi::IsWhitespace(*Cursor))
		{
			++Cursor;
		}
		return *Cursor != '\0' ? *Cursor++ : '\0';
	}

	/** "R,G,B" or a single scalar (grey). */
	bool ParseVec3(const FString& Value, FVector& Out)
	{
		const ANSICHAR* Cursor = *Value;
		float A = 0.0f;
		float B = 0.0f;
		float C = 0.0f;
		if (!ParseFloatAt(Cursor, A))
		{
			return false;
		}
		if (NextChar(Cursor) == ',')
		{
			if (!ParseFloatAt(Cursor, B) || NextChar(Cursor) != ',' || !ParseFloatAt(Cursor, C))
			{
				return false;
			}
			Out = FVector(A, B, C);
			return true;
		}
		Out = FVector(A, A, A);
		return true;
	}

	/** "U,V" or a single scalar for both. */
	bool ParseVec2(const FString& Value, FVector2D& Out)
	{
		const ANSICHAR* Cursor = *Value;
		float A = 0.0f;
		float B = 0.0f;
		if (!ParseFloatAt(Cursor, A))
		{
			return false;
		}
		if (NextChar(Cursor) == ',')
		{
			if (!ParseFloatAt(Cursor, B))
			{
				return false;
			}
			Out = FVector2D(A, B);
			return true;
		}
		Out = FVector2D(A, A);
		return true;
	}

	/** Parses a `.lmat` (the reader the runtime used before P14 part 2, unchanged). */
	void ParseDocument(const FString& Text, const FString& Path, FLegacyMaterialDocument& Doc)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		Doc.Material.Shading = EMaterialLightingModel::BlinnPhong;
		bool bHasRoughness = false;
		FString Section;
		for (const FString& RawLine : Lines)
		{
			const FString Line = StripComment(RawLine);
			if (Line.IsEmpty())
			{
				continue;
			}
			if (Line.StartsWith("[", ESearchCase::CaseSensitive) && Line.EndsWith("]", ESearchCase::CaseSensitive))
			{
				Section = Line.Mid(1, Line.Len() - 2).ToLower();
				continue;
			}
			int32 Eq = INDEX_NONE;
			if (!Line.FindChar('=', Eq))
			{
				UE_LOG(LogLeonEd, Warning, "LegacyMaterialFactory: ignoring a line without '=' in %s", *Path);
				continue;
			}
			const FString Key = Line.Left(Eq).TrimStartAndEnd();
			const FString Value = Line.Mid(Eq + 1).TrimStartAndEnd();
			const FString KeyLower = Key.ToLower();
			FMaterial& M = Doc.Material;

			if (Section == "info")
			{
				if (KeyLower == "name")
				{
					Doc.Name = Value;
				}
				else if (KeyLower == "shadingmodel" || KeyLower == "shading")
				{
					M.Shading =
						Value.ToLower() == "unlit" ? EMaterialLightingModel::Unlit : EMaterialLightingModel::BlinnPhong;
				}
				else
				{
					UE_LOG(LogLeonEd, Warning, "LegacyMaterialFactory: unknown [Info] key '%s' in %s", *Key, *Path);
				}
				continue;
			}
			if (Section == "textures" || KeyLower.Contains("map", ESearchCase::CaseSensitive))
			{
				if (KeyLower == "basecolormap" || KeyLower == "albedomap" || KeyLower == "diffusemap")
				{
					Doc.BaseColorMapKey = Value;
				}
				else if (KeyLower == "normalmap")
				{
					Doc.NormalMapKey = Value;
				}
				else if (Section == "textures")
				{
					UE_LOG(LogLeonEd, Warning, "LegacyMaterialFactory: unknown [Textures] key '%s' in %s", *Key, *Path);
				}
				continue;
			}

			float F = 0.0f;
			bool bParsed = true;
			if (KeyLower == "basecolor" || KeyLower == "albedo")
			{
				bParsed = ParseVec3(Value, M.Albedo);
			}
			else if (KeyLower == "specular")
			{
				bParsed = ParseVec3(Value, M.Specular);
			}
			else if (KeyLower == "metallic")
			{
				bParsed = ParseFloat(Value, F);
				M.Metallic = bParsed ? FMath::Clamp(F, 0.0f, 1.0f) : M.Metallic;
			}
			else if (KeyLower == "roughness")
			{
				bParsed = ParseFloat(Value, F);
				if (bParsed)
				{
					M.Roughness = FMath::Clamp(F, 0.04f, 1.0f);
					bHasRoughness = true;
				}
			}
			else if (KeyLower == "opacity" || KeyLower == "alpha")
			{
				bParsed = ParseFloat(Value, F);
				M.Alpha = bParsed ? FMath::Clamp(F, 0.0f, 1.0f) : M.Alpha;
			}
			else if (KeyLower == "shininess")
			{
				bParsed = ParseFloat(Value, F);
				M.Shininess = bParsed ? F : M.Shininess;
			}
			else if (KeyLower == "uvscale" || KeyLower == "tiling")
			{
				bParsed = ParseVec2(Value, M.UvScale);
			}
			else if (KeyLower == "castsshadows")
			{
				M.bCastsShadows = ParseBool(Value, M.bCastsShadows);
			}
			else if (KeyLower == "planarmirror")
			{
				M.bPlanarMirror = ParseBool(Value, M.bPlanarMirror);
			}
			else if (KeyLower == "unlit")
			{
				if (ParseBool(Value, false))
				{
					M.Shading = EMaterialLightingModel::Unlit;
				}
			}
			else
			{
				UE_LOG(LogLeonEd, Warning, "LegacyMaterialFactory: unknown key '%s' in %s", *Key, *Path);
			}
			if (!bParsed)
			{
				UE_LOG(LogLeonEd, Warning, "LegacyMaterialFactory: bad %s '%s' in %s", *Key, *Value, *Path);
			}
		}
		if (!bHasRoughness)
		{
			Doc.Material.SyncRoughnessFromShininess();
		}
		if (Doc.Name.IsEmpty())
		{
			Doc.Name = TEXT("Material");
		}
	}

	/** The texture a map key names: `checker` / `bump` are the engine's defaults, anything else a content key. */
	UTexture2D* ResolveMap(const FString& Key, const FString& ContentRootPath, const FString& Path)
	{
		if (Key.IsEmpty())
		{
			return nullptr;
		}
		const UEngine& EngineConfig = *GetDefault<UEngine>();
		FString ObjectPath;
		if (Key.Equals(TEXT("checker"), ESearchCase::CaseSensitive))
		{
			ObjectPath = EngineConfig.DefaultTextureName.ToString();
		}
		else if (Key.Equals(TEXT("bump"), ESearchCase::CaseSensitive))
		{
			ObjectPath = EngineConfig.DefaultBumpNormalTextureName.ToString();
		}
		else
		{
			ObjectPath = FLegacyAssetKeys::ResolveKey(ContentRootPath, Key);
		}
		UTexture2D* Texture =
			ObjectPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn);
		if (Texture == nullptr)
		{
			UE_LOG(LogLeonEd, Warning, "LegacyMaterialFactory: no texture package for the map '%s' of %s", *Key, *Path);
		}
		return Texture;
	}

} // namespace

ULegacyMaterialFactory::ULegacyMaterialFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMaterial::StaticClass();
	Formats.Add(TEXT("lmat;Leon legacy material"));
	bEditorImport = 1;
}

UObject* ULegacyMaterialFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd,
	bool& bOutOperationCanceled)
{
	(void)InClass;
	(void)Context;
	(void)Type;
	bOutOperationCanceled = false;
	FString Text;
	FFileHelper::BufferToString(Text, Buffer, static_cast<int32>(BufferEnd - Buffer));
	FLegacyMaterialDocument Doc;
	ParseDocument(Text, CurrentFilename, Doc);

	const FString Root = !ContentRootPath.IsEmpty()
		? ContentRootPath
		: FString(TEXT("/")) + FPackageName::GetPackageMountPoint(InParent->GetOutermost()->GetName()).ToString();
	Doc.Material.AlbedoMap = ResolveMap(Doc.BaseColorMapKey, Root, CurrentFilename);
	Doc.Material.NormalMap = ResolveMap(Doc.NormalMapKey, Root, CurrentFilename);

	UMaterial* Material = CreateOrOverwriteAsset<UMaterial>(InParent, InName, Flags);
	if (Material == nullptr)
	{
		return nullptr;
	}
	Material->SetFromRenderProxy(Doc.Material);
	Buffer = BufferEnd;
	return Material;
}
