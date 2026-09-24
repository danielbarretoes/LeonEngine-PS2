#include "Level/LightmapIO.h"

#include "Misc/Paths.h"
#include "Texture2D.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

namespace
{

	[[nodiscard]] bool IsLm01Magic(const char Magic[4])
	{
		return Magic[0] == 'L' && Magic[1] == 'M' && Magic[2] == '0' && Magic[3] == '1';
	}

} // namespace

std::shared_ptr<UTexture2D> LoadLightmapFile(const std::filesystem::path& Path)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In)
	{
		return nullptr;
	}
	char Magic[4]{};
	std::uint32_t Width = 0;
	std::uint32_t Height = 0;
	In.read(Magic, 4);
	In.read(reinterpret_cast<char*>(&Width), 4);
	In.read(reinterpret_cast<char*>(&Height), 4);
	if (!IsLm01Magic(Magic) || Width == 0 || Height == 0 || Width > 4096 || Height > 4096)
	{
		return nullptr;
	}
	const std::size_t PixelBytes = static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height) * 4u;
	std::vector<unsigned char> Rgba(PixelBytes);
	In.read(reinterpret_cast<char*>(Rgba.data()), static_cast<std::streamsize>(Rgba.size()));
	if (!In)
	{
		return nullptr;
	}
	UTexture2D Tex = UTexture2D::Create(static_cast<int>(Width), static_cast<int>(Height), Rgba.data());
	if (!Tex.Valid())
	{
		return nullptr;
	}
	return std::make_shared<UTexture2D>(std::move(Tex));
}

std::filesystem::path ResolveLightmapAbsolutePath(const std::string& LevelPath, const std::string& LightmapRel)
{
	if (LightmapRel.empty())
	{
		return {};
	}
	std::filesystem::path Rel(LightmapRel);
	if (Rel.is_absolute())
	{
		return Rel;
	}
	if (!LevelPath.empty())
	{
		const std::filesystem::path BesideLevel =
			(std::filesystem::path(LevelPath).parent_path() / Rel).lexically_normal();
		std::error_code Ec;
		if (std::filesystem::is_regular_file(BesideLevel, Ec) && !Ec)
		{
			return BesideLevel;
		}
		// Legacy paths used lowercase "lightmaps/"; directory is "Lightmaps/".
		std::string Alt = LightmapRel;
		if (Alt.rfind("lightmaps/", 0) == 0)
		{
			Alt.replace(0, 10, "Lightmaps/");
			const std::filesystem::path Fixed =
				(std::filesystem::path(LevelPath).parent_path() / Alt).lexically_normal();
			if (std::filesystem::is_regular_file(Fixed, Ec) && !Ec)
			{
				return Fixed;
			}
		}
		return BesideLevel;
	}
	const std::string Resolved = FPaths::ResolveAssetPath(LightmapRel);
	return Resolved.empty() ? Rel : std::filesystem::path(Resolved);
}

int LoadLevelLightmaps(ULevel& Level, const std::string& LevelPath, std::string* OutMessage)
{
	int Loaded = 0;
	int Failed = 0;
	for (UStaticMeshComponent& Mesh : Level.GetStaticMeshes())
	{
		if (Mesh.LightmapPath.empty())
		{
			continue;
		}
		const auto Path = ResolveLightmapAbsolutePath(LevelPath, Mesh.LightmapPath);
		auto Tex = LoadLightmapFile(Path);
		if (Tex)
		{
			Mesh.Lightmap = std::move(Tex);
			++Loaded;
		}
		else
		{
			++Failed;
			std::cerr << "LightmapIO: failed to load '" << Path.generic_string() << "'\n";
		}
	}
	if (OutMessage != nullptr)
	{
		*OutMessage = "Lightmaps: loaded " + std::to_string(Loaded);
		if (Failed > 0)
		{
			*OutMessage += ", failed " + std::to_string(Failed);
		}
	}
	return Loaded;
}

std::string EnsureLightmapId(UStaticMeshComponent& Mesh)
{
	if (!Mesh.LightmapId.empty())
	{
		return Mesh.LightmapId;
	}
	static thread_local std::mt19937 Rng{std::random_device{}()};
	std::uniform_int_distribution<std::uint32_t> Dist;
	char Buf[17]{};
	std::snprintf(Buf, sizeof(Buf), "%08x%08x", Dist(Rng), Dist(Rng));
	Mesh.LightmapId = Buf;
	return Mesh.LightmapId;
}
