#pragma once

#include <string>
#include <string_view>
#include <vector>

struct ENGINE_API FLevelEntry
{
	std::string Name; // from the level document name, else filename stem
	std::string Path; // resolved .llev path
	std::string Pack; // project folder name under `Projects/`
	std::string GameMode; // Unreal-like GameMode Override
};

/// Discovers binary Leon Level files (`.llev`) under project packs or a flat directory.
class ENGINE_API FLevelCatalog
{
public:
	/// Scan a flat directory of `*.llev` (unit tests / tools).
	bool Scan(const std::string& InDirectory);

	/// Scan every pack under `projectsRoot/` for `Content/Levels/*.llev`.
	bool ScanProjectPacks(const std::string& ProjectsRoot);

	/// Scan one pack (`Projects/<pack>/`) for `Content/Levels/*.llev`
	/// (also `<pack>/Levels` via FPaths::ProjectContentDir pre-Content layout).
	bool ScanPack(const std::string& PackDirectory);

	/// First catalog index whose `gameMode` or `pack` equals `id`, or `NumEntries()` if none.
	[[nodiscard]] std::size_t FindIndexByGameModeOrPack(const std::string& Id) const;

	/// Match `FLevelEntry.name`, path stem, or full path (case-insensitive). Returns `NumEntries()` if none.
	[[nodiscard]] std::size_t FindIndexByLevelKey(std::string_view Key) const;

	[[nodiscard]] const std::vector<FLevelEntry>& GetEntries() const
	{
		return Entries;
	}
	[[nodiscard]] bool IsEmpty() const
	{
		return Entries.empty();
	}
	[[nodiscard]] std::size_t NumEntries() const
	{
		return Entries.size();
	}
	[[nodiscard]] const std::string& GetDirectory() const
	{
		return Directory;
	}

private:
	std::string Directory;
	std::vector<FLevelEntry> Entries;
};
