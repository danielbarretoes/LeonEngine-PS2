#include "Misc/FileHelper.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <system_error>

namespace
{

	[[nodiscard]] std::filesystem::path MakeTempSibling(const std::filesystem::path& Path)
	{
		const auto Parent = Path.parent_path();
		const auto Stem = Path.filename().string();
		const auto Now = std::chrono::steady_clock::now().time_since_epoch().count();
		std::mt19937 Rng{static_cast<std::mt19937::result_type>(Now)};
		const std::uint32_t Roll = Rng();
		return Parent / (Stem + ".tmp." + std::to_string(Now) + "." + std::to_string(Roll));
	}

} // namespace

bool FFileHelper::WriteFileAtomic(const std::filesystem::path& Path, const void* Data, std::size_t Size)
{
	if (Path.empty())
	{
		return false;
	}
	std::error_code Ec;
	if (!Path.parent_path().empty())
	{
		std::filesystem::create_directories(Path.parent_path(), Ec);
		if (Ec)
		{
			std::cerr << "FileIO: cannot create directory for " << Path.string() << ": " << Ec.message() << '\n';
			return false;
		}
	}

	const std::filesystem::path Temp = MakeTempSibling(Path);
	{
		std::ofstream Out(Temp, std::ios::binary | std::ios::trunc);
		if (!Out)
		{
			std::cerr << "FileIO: cannot open temp for write: " << Temp.string() << '\n';
			return false;
		}
		if (Size > 0 && Data != nullptr)
		{
			Out.write(static_cast<const char*>(Data), static_cast<std::streamsize>(Size));
		}
		Out.flush();
		if (!Out)
		{
			std::cerr << "FileIO: write failed: " << Temp.string() << '\n';
			std::filesystem::remove(Temp, Ec);
			return false;
		}
	}

	std::filesystem::rename(Temp, Path, Ec);
	if (Ec)
	{
		// Windows: replace existing target when rename-over fails.
		std::filesystem::remove(Path, Ec);
		Ec.clear();
		std::filesystem::rename(Temp, Path, Ec);
		if (Ec)
		{
			std::cerr << "FileIO: rename failed " << Temp.string() << " -> " << Path.string() << ": " << Ec.message()
					  << '\n';
			std::filesystem::remove(Temp, Ec);
			return false;
		}
	}
	return true;
}

bool FFileHelper::WriteFileAtomic(const std::filesystem::path& Path, const std::vector<std::uint8_t>& Bytes)
{
	return FFileHelper::WriteFileAtomic(Path, Bytes.data(), Bytes.size());
}

bool FFileHelper::WriteTextFileAtomic(const std::filesystem::path& Path, std::string_view Text)
{
	return FFileHelper::WriteFileAtomic(Path, Text.data(), Text.size());
}
