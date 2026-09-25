#include "LeonMaterialFormat.h"

#include "Misc/CString.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogLeonMaterial, Log, All);

namespace
{

	/** Drops a '#' or ';' comment and the surrounding whitespace. */
	[[nodiscard]] FString StripComment(const FString& Line)
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

	[[nodiscard]] bool ParseBool(const FString& Value, bool bFallback)
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
	[[nodiscard]] bool ParseFloatAt(const ANSICHAR*& Cursor, float& Out)
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

	[[nodiscard]] bool ParseFloat(const FString& Value, float& Out)
	{
		const ANSICHAR* Cursor = *Value;
		return ParseFloatAt(Cursor, Out);
	}

	/** The next non-whitespace character (consumed), or '\0' at the end. */
	[[nodiscard]] ANSICHAR NextChar(const ANSICHAR*& Cursor)
	{
		while (*Cursor != '\0' && FCharAnsi::IsWhitespace(*Cursor))
		{
			++Cursor;
		}
		return *Cursor != '\0' ? *Cursor++ : '\0';
	}

	/** "R,G,B" or a single scalar (gray), like the stream parser it replaces. */
	[[nodiscard]] bool ParseVec3(const FString& Value, FVector& Out)
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
		// Single scalar -> gray
		Out = FVector(A, A, A);
		return true;
	}

	[[nodiscard]] bool ParseVec2(const FString& Value, FVector2D& Out)
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

	/** %g, like the default formatting of the stream writer it replaces. */
	[[nodiscard]] FString FormatFloat(float Value)
	{
		return FString::Printf("%g", static_cast<double>(Value));
	}

	[[nodiscard]] FString FormatVector(const FVector& Value)
	{
		return FormatFloat(Value.X) + "," + FormatFloat(Value.Y) + "," + FormatFloat(Value.Z);
	}

	const TCHAR* const MaterialHeader = "# Leon Material (.lmat) — Unreal Material Instance–like parameters\n"
										"# version 1\n\n";

} // namespace

bool IsLeonMaterialPath(const FString& Path)
{
	// FString == ignores case, like the lowered comparison it replaces.
	return FPaths::GetExtension(Path, true) == ".lmat";
}

bool LoadLeonMaterialDocument(const FString& Path, FLeonMaterialDocument& Out)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		UE_LOG(LogLeonMaterial, Error, "LeonMaterial: cannot open %s", *Path);
		return false;
	}

	FLeonMaterialDocument Doc;
	Doc.Material.Shading = EMaterialShadingModel::BlinnPhong;
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
			UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: ignoring line without '=': %s", *Path);
			continue;
		}
		const FString Key = Line.Left(Eq).TrimStartAndEnd();
		const FString Value = Line.Mid(Eq + 1).TrimStartAndEnd();
		const FString KeyLower = Key.ToLower();

		if (Section == "info")
		{
			if (KeyLower == "name")
			{
				Doc.Name = Value;
			}
			else if (KeyLower == "shadingmodel" || KeyLower == "shading")
			{
				Doc.Material.Shading =
					(Value.ToLower() == "unlit") ? EMaterialShadingModel::Unlit : EMaterialShadingModel::BlinnPhong;
			}
			else
			{
				UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: unknown [Info] key '%s' in %s", *Key, *Path);
			}
			continue;
		}

		if (Section == "textures" || KeyLower.Contains("map", ESearchCase::CaseSensitive))
		{
			if (KeyLower == "basecolormap" || KeyLower == "albedomap" || KeyLower == "diffusemap")
			{
				Doc.BaseColorMapPath = Value;
			}
			else if (KeyLower == "normalmap")
			{
				Doc.NormalMapPath = Value;
			}
			else if (Section == "textures")
			{
				UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: unknown [Textures] key '%s' in %s", *Key, *Path);
			}
			continue;
		}

		if (KeyLower == "basecolor" || KeyLower == "albedo")
		{
			if (!ParseVec3(Value, Doc.Material.Albedo))
			{
				UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: bad BaseColor '%s' in %s", *Value, *Path);
			}
		}
		else if (KeyLower == "specular")
		{
			if (!ParseVec3(Value, Doc.Material.Specular))
			{
				UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: bad Specular '%s' in %s", *Value, *Path);
			}
		}
		else if (KeyLower == "metallic")
		{
			float F = Doc.Material.Metallic;
			if (ParseFloat(Value, F))
			{
				Doc.Material.Metallic = FMath::Clamp(F, 0.0f, 1.0f);
			}
			else
			{
				UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: bad Metallic '%s' in %s", *Value, *Path);
			}
		}
		else if (KeyLower == "roughness")
		{
			float F = Doc.Material.Roughness;
			if (ParseFloat(Value, F))
			{
				Doc.Material.Roughness = FMath::Clamp(F, 0.04f, 1.0f);
				bHasRoughness = true;
			}
			else
			{
				UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: bad Roughness '%s' in %s", *Value, *Path);
			}
		}
		else if (KeyLower == "opacity" || KeyLower == "alpha")
		{
			float F = Doc.Material.Alpha;
			if (ParseFloat(Value, F))
			{
				Doc.Material.Alpha = FMath::Clamp(F, 0.0f, 1.0f);
			}
			else
			{
				UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: bad Opacity '%s' in %s", *Value, *Path);
			}
		}
		else if (KeyLower == "shininess")
		{
			float F = Doc.Material.Shininess;
			if (ParseFloat(Value, F))
			{
				Doc.Material.Shininess = F;
			}
			else
			{
				UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: bad Shininess '%s' in %s", *Value, *Path);
			}
		}
		else if (KeyLower == "uvscale" || KeyLower == "tiling")
		{
			if (!ParseVec2(Value, Doc.Material.UvScale))
			{
				UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: bad UVScale '%s' in %s", *Value, *Path);
			}
		}
		else if (KeyLower == "castsshadows")
		{
			Doc.Material.bCastsShadows = ParseBool(Value, Doc.Material.bCastsShadows);
		}
		else if (KeyLower == "planarmirror")
		{
			Doc.Material.bPlanarMirror = ParseBool(Value, Doc.Material.bPlanarMirror);
		}
		else if (KeyLower == "unlit")
		{
			if (ParseBool(Value, false))
			{
				Doc.Material.Shading = EMaterialShadingModel::Unlit;
			}
		}
		else
		{
			UE_LOG(LogLeonMaterial, Warning, "LeonMaterial: unknown key '%s' in %s", *Key, *Path);
		}
	}

	if (!bHasRoughness)
	{
		Doc.Material.SyncRoughnessFromShininess();
	}
	if (Doc.Name.IsEmpty())
	{
		Doc.Name = "Material";
	}
	Out = MoveTemp(Doc);
	return true;
}

bool SaveLeonMaterialFile(const FString& Path, const FString& InName, const FMaterial& InMaterial,
	const FString& InBaseColorMapPath, const FString& InNormalMapPath)
{
	FString Out = MaterialHeader;
	Out += "[Info]\n";
	Out += "Name=" + (InName.IsEmpty() ? FString("Material") : InName) + "\n";
	Out += FString("ShadingModel=") + (InMaterial.Shading == EMaterialShadingModel::Unlit ? "Unlit" : "DefaultLit") +
		"\n\n";
	Out += "[Parameters]\n";
	Out += "BaseColor=" + FormatVector(InMaterial.Albedo) + "\n";
	Out += "Specular=" + FormatVector(InMaterial.Specular) + "\n";
	Out += "Metallic=" + FormatFloat(InMaterial.Metallic) + "\n";
	Out += "Roughness=" + FormatFloat(InMaterial.Roughness) + "\n";
	Out += "Opacity=" + FormatFloat(InMaterial.Alpha) + "\n";
	Out += "Shininess=" + FormatFloat(InMaterial.Shininess) + "\n";
	Out += "UVScale=" + FormatFloat(InMaterial.UvScale.X) + "," + FormatFloat(InMaterial.UvScale.Y) + "\n";
	Out += FString("CastsShadows=") + (InMaterial.bCastsShadows ? "true" : "false") + "\n";
	Out += FString("PlanarMirror=") + (InMaterial.bPlanarMirror ? "true" : "false") + "\n\n";
	Out += "[Textures]\n";
	Out += "BaseColorMap=" + InBaseColorMapPath + "\n";
	Out += "NormalMap=" + InNormalMapPath + "\n";
	if (!FFileHelper::SaveStringToFile(Out, *Path))
	{
		UE_LOG(LogLeonMaterial, Error, "LeonMaterial: cannot write %s", *Path);
		return false;
	}
	return true;
}

FString MakeDefaultLeonMaterialText(const FString& InName, const FVector& BaseColor, float Metallic, float Roughness)
{
	FString Out = MaterialHeader;
	Out += "[Info]\nName=" + (InName.IsEmpty() ? FString("M_New") : InName) + "\n";
	Out += "ShadingModel=DefaultLit\n\n";
	Out += "[Parameters]\n";
	Out += "BaseColor=" + FormatVector(BaseColor) + "\n";
	Out += "Specular=0.04,0.04,0.04\n";
	Out += "Metallic=" + FormatFloat(Metallic) + "\n";
	Out += "Roughness=" + FormatFloat(FMath::Clamp(Roughness, 0.04f, 1.0f)) + "\n";
	Out += "Opacity=1\n";
	Out += "UVScale=1,1\n";
	Out += "CastsShadows=true\n\n";
	Out += "[Textures]\n";
	Out += "BaseColorMap=\n";
	Out += "NormalMap=\n";
	return Out;
}
