#include "ProjectDescriptor.h"

#include "Misc/Paths.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

FProjectDescriptor FProjectDescriptor::Resolve(const char* PackName)
{
	FProjectDescriptor Pack;
	Pack.Name = PackName != nullptr ? PackName : "";
	if (Pack.Name.empty())
	{
		return Pack;
	}
	Pack.RootDirectory = FPaths::ResolveAssetPath(std::string("Projects/") + Pack.Name);
	if (Pack.RootDirectory.empty())
	{
		return Pack;
	}

	const auto Marker = std::filesystem::path(Pack.RootDirectory) / "leon.game.json";
	std::ifstream In(Marker);
	if (!In.is_open())
	{
		return Pack;
	}
	try
	{
		nlohmann::json Doc;
		In >> Doc;
		Pack.DefaultLevel = Doc.value("defaultLevel", "");
	}
	catch (...)
	{
		// Keep pack root; startup falls back to first catalog entry.
	}
	return Pack;
}

std::string FProjectDescriptor::DefaultLevelKey() const
{
	if (DefaultLevel.empty())
	{
		return {};
	}
	return std::filesystem::path(DefaultLevel).stem().string();
}
