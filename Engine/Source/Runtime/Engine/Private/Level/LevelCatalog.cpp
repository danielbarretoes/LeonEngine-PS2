#include "Level/LevelCatalog.h"

#include "Level/LeonLevelFormat.h"
#include "Misc/CString.h"
#include "Misc/Paths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{

	bool IsLeonLevelFile(const std::filesystem::directory_entry& Entry)
	{
		if (!Entry.is_regular_file())
		{
			return false;
		}
		std::string Extension = Entry.path().extension().string();
		for (char& C : Extension)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		return Extension == LeonLevelExtension;
	}

	void AppendLevelsFromDirectory(
		std::vector<FLevelEntry>& Out, const std::filesystem::path& LevelsDir, const std::string& PackName)
	{
		std::error_code Ec;
		if (!std::filesystem::is_directory(LevelsDir, Ec) || Ec)
		{
			return;
		}

		for (const auto& Entry : std::filesystem::directory_iterator(LevelsDir, Ec))
		{
			if (Ec)
			{
				break;
			}
			if (!IsLeonLevelFile(Entry))
			{
				continue;
			}

			FLevelEntry Item;
			Item.Path = Entry.path().lexically_normal().string();
			Item.Name = Entry.path().stem().string();
			Item.Pack = PackName;

			// Display name + GameMode Override are read once at catalog scan (FGameplayRouter uses it).
			FLevelDocument Doc;
			if (LoadLeonLevelFile(Item.Path, Doc))
			{
				if (!Doc.Name.empty())
				{
					Item.Name = Doc.Name;
				}
				Item.GameMode = Doc.GameMode;
			}

			Out.push_back(std::move(Item));
		}
	}

	bool DirectoryHasLeonLevels(const std::filesystem::path& LevelsDir)
	{
		std::error_code Ec;
		if (!std::filesystem::is_directory(LevelsDir, Ec) || Ec)
		{
			return false;
		}
		for (const auto& Entry : std::filesystem::directory_iterator(LevelsDir, Ec))
		{
			if (Ec)
			{
				break;
			}
			if (IsLeonLevelFile(Entry))
			{
				return true;
			}
		}
		return false;
	}

} // namespace

bool FLevelCatalog::Scan(const std::string& InDirectory)
{
	Entries.clear();
	Directory = InDirectory;

	std::error_code Ec;
	const std::filesystem::path Dir(InDirectory);
	if (!std::filesystem::is_directory(Dir, Ec) || Ec)
	{
		std::cerr << "LevelCatalog: not a directory: " << InDirectory << '\n';
		return false;
	}

	AppendLevelsFromDirectory(Entries, Dir, {});
	std::sort(
		Entries.begin(), Entries.end(), [](const FLevelEntry& A, const FLevelEntry& B) { return A.Path < B.Path; });

	std::cout << "LevelCatalog: " << Entries.size() << " Level(s) in " << InDirectory << '\n';
	for (std::size_t I = 0; I < Entries.size(); ++I)
	{
		std::cout << "  [" << I << "] " << Entries[I].Name << " (" << Entries[I].Path << ")\n";
	}
	return !Entries.empty();
}

bool FLevelCatalog::ScanPack(const std::string& PackDirectory)
{
	Entries.clear();
	Directory = PackDirectory;

	std::error_code Ec;
	const std::filesystem::path PackDir(PackDirectory);
	if (!std::filesystem::is_directory(PackDir, Ec) || Ec)
	{
		std::cerr << "LevelCatalog: pack is not a directory: " << PackDirectory << '\n';
		return false;
	}

	const std::string PackName = PackDir.filename().string();
	// Unreal-like: `<pack>/Content/Levels` (via FPaths::ProjectContentDir).
	const std::filesystem::path LevelsDir = FPaths::ProjectContentDir(PackDir) / "Levels";

	if (DirectoryHasLeonLevels(LevelsDir))
	{
		AppendLevelsFromDirectory(Entries, LevelsDir, PackName);
	}

	std::sort(
		Entries.begin(), Entries.end(), [](const FLevelEntry& A, const FLevelEntry& B) { return A.Path < B.Path; });

	std::cout << "LevelCatalog: " << Entries.size() << " Level(s) in pack " << PackName << '\n';
	for (std::size_t I = 0; I < Entries.size(); ++I)
	{
		const FLevelEntry& E = Entries[I];
		std::cout << "  [" << I << "] " << E.Pack << "/" << E.Name << " (" << E.Path << ")\n";
	}
	return !Entries.empty();
}

bool FLevelCatalog::ScanProjectPacks(const std::string& ProjectsRoot)
{
	Entries.clear();
	Directory = ProjectsRoot;

	std::error_code Ec;
	const std::filesystem::path Root(ProjectsRoot);
	if (!std::filesystem::is_directory(Root, Ec) || Ec)
	{
		std::cerr << "LevelCatalog: projects root is not a directory: " << ProjectsRoot << '\n';
		return false;
	}

	for (const auto& PackEntry : std::filesystem::directory_iterator(Root, Ec))
	{
		if (Ec)
		{
			break;
		}
		if (!PackEntry.is_directory())
		{
			continue;
		}
		const std::string PackName = PackEntry.path().filename().string();
		// Skip shared host / cmake helpers that are not game projects.
		if (PackName == "_host" || PackName.starts_with('.'))
		{
			continue;
		}
		const std::filesystem::path LevelsDir = FPaths::ProjectContentDir(PackEntry.path()) / "Levels";

		if (DirectoryHasLeonLevels(LevelsDir))
		{
			AppendLevelsFromDirectory(Entries, LevelsDir, PackName);
		}
	}

	std::sort(
		Entries.begin(), Entries.end(), [](const FLevelEntry& A, const FLevelEntry& B) { return A.Path < B.Path; });

	std::cout << "LevelCatalog: " << Entries.size() << " Level(s) under " << ProjectsRoot << '\n';
	for (std::size_t I = 0; I < Entries.size(); ++I)
	{
		const FLevelEntry& E = Entries[I];
		std::cout << "  [" << I << "] " << E.Pack << "/" << E.Name << " (" << E.Path << ")\n";
	}
	return !Entries.empty();
}

std::size_t FLevelCatalog::FindIndexByGameModeOrPack(const std::string& Id) const
{
	if (Id.empty())
	{
		return Entries.size();
	}
	for (std::size_t I = 0; I < Entries.size(); ++I)
	{
		if (Entries[I].GameMode == Id || Entries[I].Pack == Id)
		{
			return I;
		}
	}
	return Entries.size();
}

std::size_t FLevelCatalog::FindIndexByLevelKey(std::string_view Key) const
{
	if (Key.empty())
	{
		return Entries.size();
	}
	const std::string Needle = FCString::ToLower(Key);
	for (std::size_t I = 0; I < Entries.size(); ++I)
	{
		const FLevelEntry& E = Entries[I];
		if (FCString::ToLower(E.Name) == Needle)
		{
			return I;
		}
		const std::string Stem = FCString::ToLower(std::filesystem::path(E.Path).stem().string());
		if (Stem == Needle)
		{
			return I;
		}
		if (FCString::ToLower(E.Path) == Needle)
		{
			return I;
		}
	}
	return Entries.size();
}
