#include "LeonMaterialFormat.h"

#include "Migration/LegacyContentPath.h"
#include "Misc/FileHelper.h"
#include "ResourceCache.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{

	[[nodiscard]] std::string ExtLower(const std::string& Path)
	{
		const auto Pos = Path.find_last_of('.');
		if (Pos == std::string::npos)
		{
			return {};
		}
		std::string E = Path.substr(Pos);
		for (char& C : E)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		return E;
	}

	[[nodiscard]] std::string Trim(std::string S)
	{
		while (!S.empty() && std::isspace(static_cast<unsigned char>(S.front())))
		{
			S.erase(S.begin());
		}
		while (!S.empty() && std::isspace(static_cast<unsigned char>(S.back())))
		{
			S.pop_back();
		}
		return S;
	}

	[[nodiscard]] std::string StripComment(std::string Line)
	{
		const auto Hash = Line.find('#');
		if (Hash != std::string::npos)
		{
			Line = Line.substr(0, Hash);
		}
		const auto Semi = Line.find(';');
		if (Semi != std::string::npos)
		{
			Line = Line.substr(0, Semi);
		}
		return Trim(std::move(Line));
	}

	[[nodiscard]] bool ParseBool(const std::string& V, bool bFallback)
	{
		std::string S = V;
		for (char& C : S)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
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

	[[nodiscard]] bool ParseFloat(const std::string& V, float& Out)
	{
		try
		{
			size_t Idx = 0;
			Out = std::stof(V, &Idx);
			return Idx > 0;
		}
		catch (...)
		{
			return false;
		}
	}

	[[nodiscard]] bool ParseVec3(const std::string& V, glm::vec3& Out)
	{
		std::stringstream Ss(V);
		char Comma = 0;
		float A = 0.0f;
		float B = 0.0f;
		float C = 0.0f;
		if (!(Ss >> A))
		{
			return false;
		}
		if (Ss >> Comma && Comma == ',')
		{
			if (!(Ss >> B))
			{
				return false;
			}
			if (!(Ss >> Comma) || Comma != ',')
			{
				return false;
			}
			if (!(Ss >> C))
			{
				return false;
			}
			Out = {A, B, C};
			return true;
		}
		// Single scalar → gray
		Out = {A, A, A};
		return true;
	}

	[[nodiscard]] bool ParseVec2(const std::string& V, glm::vec2& Out)
	{
		std::stringstream Ss(V);
		char Comma = 0;
		float A = 0.0f;
		float B = 0.0f;
		if (!(Ss >> A))
		{
			return false;
		}
		if (Ss >> Comma && Comma == ',')
		{
			if (!(Ss >> B))
			{
				return false;
			}
			Out = {A, B};
			return true;
		}
		Out = {A, A};
		return true;
	}

	void ApplyTextureKey(
		FResourceCache& Resources, FMaterial& InMaterial, const std::string& Key, const std::string& Value)
	{
		if (Value.empty())
		{
			return;
		}
		std::string K = Key;
		for (char& C : K)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		if (K == "basecolormap" || K == "albedomap" || K == "diffusemap")
		{
			if (Value == "checker")
			{
				InMaterial.AlbedoMap = Resources.CheckerTexture(64);
			}
			else
			{
				InMaterial.AlbedoMap = Resources.LoadTexture(ResolveLegacyContentPath(Value));
			}
		}
		else if (K == "normalmap")
		{
			if (Value == "bump")
			{
				InMaterial.NormalMap = Resources.BumpNormalTexture(256);
			}
			else
			{
				InMaterial.NormalMap = Resources.LoadTexture(ResolveLegacyContentPath(Value));
			}
		}
	}

} // namespace

bool IsLeonMaterialPath(const std::string& Path)
{
	return ExtLower(Path) == ".lmat";
}

bool LoadLeonMaterialDocument(const std::string& Path, FLeonMaterialDocument& Out)
{
	std::ifstream In(Path);
	if (!In.is_open())
	{
		std::cerr << "LeonMaterial: cannot open " << Path << '\n';
		return false;
	}

	FLeonMaterialDocument Doc{};
	Doc.Material.Shading = EMaterialShadingModel::BlinnPhong;
	bool bHasRoughness = false;
	bool bHasShininess = false;
	std::string Section;

	std::string Line;
	while (std::getline(In, Line))
	{
		Line = StripComment(std::move(Line));
		if (Line.empty())
		{
			continue;
		}
		if (Line.front() == '[' && Line.back() == ']')
		{
			Section = Line.substr(1, Line.size() - 2);
			for (char& C : Section)
			{
				C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
			}
			continue;
		}
		const auto Eq = Line.find('=');
		if (Eq == std::string::npos)
		{
			std::cerr << "LeonMaterial: ignoring line without '=': " << Path << '\n';
			continue;
		}
		std::string Key = Trim(Line.substr(0, Eq));
		std::string Value = Trim(Line.substr(Eq + 1));
		std::string KeyLower = Key;
		for (char& C : KeyLower)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}

		if (Section == "info")
		{
			if (KeyLower == "name")
			{
				Doc.Name = Value;
			}
			else if (KeyLower == "shadingmodel" || KeyLower == "shading")
			{
				std::string V = Value;
				for (char& C : V)
				{
					C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
				}
				Doc.Material.Shading =
					(V == "unlit") ? EMaterialShadingModel::Unlit : EMaterialShadingModel::BlinnPhong;
			}
			else
			{
				std::cerr << "LeonMaterial: unknown [Info] key '" << Key << "' in " << Path << '\n';
			}
			continue;
		}

		if (Section == "textures" || KeyLower.find("map") != std::string::npos)
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
				std::cerr << "LeonMaterial: unknown [Textures] key '" << Key << "' in " << Path << '\n';
			}
			continue;
		}

		if (KeyLower == "basecolor" || KeyLower == "albedo")
		{
			if (!ParseVec3(Value, Doc.Material.Albedo))
			{
				std::cerr << "LeonMaterial: bad BaseColor '" << Value << "' in " << Path << '\n';
			}
		}
		else if (KeyLower == "specular")
		{
			if (!ParseVec3(Value, Doc.Material.Specular))
			{
				std::cerr << "LeonMaterial: bad Specular '" << Value << "' in " << Path << '\n';
			}
		}
		else if (KeyLower == "metallic")
		{
			float F = Doc.Material.Metallic;
			if (ParseFloat(Value, F))
			{
				Doc.Material.Metallic = std::clamp(F, 0.0f, 1.0f);
			}
			else
			{
				std::cerr << "LeonMaterial: bad Metallic '" << Value << "' in " << Path << '\n';
			}
		}
		else if (KeyLower == "roughness")
		{
			float F = Doc.Material.Roughness;
			if (ParseFloat(Value, F))
			{
				Doc.Material.Roughness = std::clamp(F, 0.04f, 1.0f);
				bHasRoughness = true;
			}
			else
			{
				std::cerr << "LeonMaterial: bad Roughness '" << Value << "' in " << Path << '\n';
			}
		}
		else if (KeyLower == "opacity" || KeyLower == "alpha")
		{
			float F = Doc.Material.Alpha;
			if (ParseFloat(Value, F))
			{
				Doc.Material.Alpha = std::clamp(F, 0.0f, 1.0f);
			}
			else
			{
				std::cerr << "LeonMaterial: bad Opacity '" << Value << "' in " << Path << '\n';
			}
		}
		else if (KeyLower == "shininess")
		{
			float F = Doc.Material.Shininess;
			if (ParseFloat(Value, F))
			{
				Doc.Material.Shininess = F;
				bHasShininess = true;
			}
			else
			{
				std::cerr << "LeonMaterial: bad Shininess '" << Value << "' in " << Path << '\n';
			}
		}
		else if (KeyLower == "uvscale" || KeyLower == "tiling")
		{
			if (!ParseVec2(Value, Doc.Material.UvScale))
			{
				std::cerr << "LeonMaterial: bad UVScale '" << Value << "' in " << Path << '\n';
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
			std::cerr << "LeonMaterial: unknown key '" << Key << "' in " << Path << '\n';
		}
	}

	if (!bHasRoughness)
	{
		Doc.Material.SyncRoughnessFromShininess();
	}
	if (Doc.Name.empty())
	{
		Doc.Name = "Material";
	}
	Out = std::move(Doc);
	return true;
}

bool LoadLeonMaterialFile(FResourceCache& Resources, const std::string& Path, FMaterial& Out)
{
	FLeonMaterialDocument Doc;
	if (!LoadLeonMaterialDocument(Path, Doc))
	{
		return false;
	}
	if (!Doc.BaseColorMapPath.empty())
	{
		ApplyTextureKey(Resources, Doc.Material, "basecolormap", Doc.BaseColorMapPath);
	}
	if (!Doc.NormalMapPath.empty())
	{
		ApplyTextureKey(Resources, Doc.Material, "normalmap", Doc.NormalMapPath);
	}
	Out = std::move(Doc.Material);
	return true;
}

bool SaveLeonMaterialFile(const std::string& Path, const std::string& InName, const FMaterial& InMaterial,
	const std::string& InBaseColorMapPath, const std::string& InNormalMapPath)
{
	std::ostringstream Out;
	Out << "# Leon Material (.lmat) — Unreal Material Instance–like parameters\n";
	Out << "# version 1\n\n";
	Out << "[Info]\n";
	Out << "Name=" << (InName.empty() ? "Material" : InName) << '\n';
	Out << "ShadingModel=" << (InMaterial.Shading == EMaterialShadingModel::Unlit ? "Unlit" : "DefaultLit") << "\n\n";
	Out << "[Parameters]\n";
	Out << "BaseColor=" << InMaterial.Albedo.x << ',' << InMaterial.Albedo.y << ',' << InMaterial.Albedo.z << '\n';
	Out << "Specular=" << InMaterial.Specular.x << ',' << InMaterial.Specular.y << ',' << InMaterial.Specular.z << '\n';
	Out << "Metallic=" << InMaterial.Metallic << '\n';
	Out << "Roughness=" << InMaterial.Roughness << '\n';
	Out << "Opacity=" << InMaterial.Alpha << '\n';
	Out << "Shininess=" << InMaterial.Shininess << '\n';
	Out << "UVScale=" << InMaterial.UvScale.x << ',' << InMaterial.UvScale.y << '\n';
	Out << "CastsShadows=" << (InMaterial.bCastsShadows ? "true" : "false") << '\n';
	Out << "PlanarMirror=" << (InMaterial.bPlanarMirror ? "true" : "false") << "\n\n";
	Out << "[Textures]\n";
	Out << "BaseColorMap=" << InBaseColorMapPath << '\n';
	Out << "NormalMap=" << InNormalMapPath << '\n';
	if (!FFileHelper::SaveStringToFile(FString(Out.str().c_str()), *FString(Path.c_str())))
	{
		std::cerr << "LeonMaterial: cannot write " << Path << '\n';
		return false;
	}
	return true;
}

std::string MakeDefaultLeonMaterialText(
	const std::string& InName, const glm::vec3& BaseColor, float Metallic, float Roughness)
{
	FMaterial M{};
	M.Albedo = BaseColor;
	M.Metallic = Metallic;
	M.Roughness = std::clamp(Roughness, 0.04f, 1.0f);
	M.Shininess = 32.0f;
	std::ostringstream Oss;
	// Reuse writer via temp logic inline
	Oss << "# Leon Material (.lmat) — Unreal Material Instance–like parameters\n";
	Oss << "# version 1\n\n";
	Oss << "[Info]\nName=" << (InName.empty() ? "M_New" : InName) << "\n";
	Oss << "ShadingModel=DefaultLit\n\n";
	Oss << "[Parameters]\n";
	Oss << "BaseColor=" << BaseColor.x << ',' << BaseColor.y << ',' << BaseColor.z << '\n';
	Oss << "Specular=0.04,0.04,0.04\n";
	Oss << "Metallic=" << Metallic << '\n';
	Oss << "Roughness=" << M.Roughness << '\n';
	Oss << "Opacity=1\n";
	Oss << "UVScale=1,1\n";
	Oss << "CastsShadows=true\n\n";
	Oss << "[Textures]\n";
	Oss << "BaseColorMap=\n";
	Oss << "NormalMap=\n";
	return Oss.str();
}
