#include "Misc/Paths.h"

#include <array>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <system_error>
#include <vector>

#if PLATFORM_WINDOWS
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#endif

namespace
{

	/// Prefer the newest existing candidate so repo edits win over a stale POST_BUILD copy.
	std::string NewestExisting(const std::vector<std::filesystem::path>& Candidates)
	{
		std::filesystem::path Best;
		std::filesystem::file_time_type BestTime{};
		bool bFound = false;
		for (const auto& Path : Candidates)
		{
			std::error_code Ec;
			if (!std::filesystem::exists(Path, Ec) || Ec)
			{
				continue;
			}
			const auto Time = std::filesystem::last_write_time(Path, Ec);
			if (Ec)
			{
				continue;
			}
			if (!bFound || Time > BestTime)
			{
				Best = Path;
				BestTime = Time;
				bFound = true;
			}
		}
		if (!bFound)
		{
			return {};
		}
		return Best.lexically_normal().string();
	}

	std::string NewestExisting(std::initializer_list<std::filesystem::path> Candidates)
	{
		return NewestExisting(std::vector<std::filesystem::path>(Candidates));
	}

	std::filesystem::path StripAssetsPrefix(const std::filesystem::path& Rel)
	{
		const std::string S = Rel.generic_string();
		if (S.rfind("assets/", 0) == 0)
		{
			return S.substr(7);
		}
		if (S == "assets")
		{
			return {};
		}
		return Rel;
	}

} // namespace

std::filesystem::path FPaths::ExecutableDir()
{
#if PLATFORM_WINDOWS
	std::vector<wchar_t> Buffer(MAX_PATH);
	for (;;)
	{
		const DWORD Length = GetModuleFileNameW(nullptr, Buffer.data(), static_cast<DWORD>(Buffer.size()));
		if (Length == 0)
		{
			return std::filesystem::current_path();
		}
		if (Length < Buffer.size())
		{
			return std::filesystem::path(Buffer.data()).parent_path();
		}
		// Truncated — grow and retry (long paths / Unicode).
		Buffer.resize(Buffer.size() * 2);
		if (Buffer.size() > 32768)
		{
			return std::filesystem::current_path();
		}
	}
#else
	std::error_code ec;
	auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
	if (!ec)
	{
		return path.parent_path();
	}
	return std::filesystem::current_path();
#endif
}

std::string FPaths::ResolveAssetPath(const std::string& RelativePath)
{
	const std::filesystem::path ExeDir = FPaths::ExecutableDir();
	const std::filesystem::path Rel(RelativePath);
	const std::filesystem::path UnderAssets = StripAssetsPrefix(Rel);

	std::vector<std::filesystem::path> Candidates = {
		ExeDir / Rel,
		ExeDir / "assets" / UnderAssets,
		std::filesystem::path("assets") / UnderAssets,
		Rel,
		std::filesystem::path("../") / Rel,
		std::filesystem::path("../../") / Rel,
		std::filesystem::path("../../../") / Rel,
		ExeDir / ".." / Rel,
		ExeDir / "../.." / Rel,
		ExeDir / "../../.." / Rel,
	};
	// Engine content (Unreal layout): Engine/Content/<X>, shaders in Engine/Shaders/<X>.
	// Executables live in Engine/Binaries/<Platform>, so ../.. from the exe is Engine/.
	const std::string UnderStr = UnderAssets.generic_string();
	const bool bIsShader = UnderStr == "Shaders" || UnderStr.rfind("Shaders/", 0) == 0;
	const std::filesystem::path EngineRel =
		bIsShader ? std::filesystem::path(UnderStr.size() > 8 ? UnderStr.substr(8) : std::string()) : UnderAssets;
	const std::vector<std::filesystem::path> EngineDirs = {
		std::filesystem::path("Engine"),
		std::filesystem::path("../Engine"),
		std::filesystem::path("../../Engine"),
		ExeDir / "../..",
		ExeDir / "../../../Engine",
#ifdef LEON_ENGINE_DIR
		std::filesystem::path(LEON_ENGINE_DIR),
#endif
	};
	for (const std::filesystem::path& EngineDir : EngineDirs)
	{
		Candidates.push_back(EngineDir / (bIsShader ? "Shaders" : "Content") / EngineRel);
	}

	std::string Found = NewestExisting(Candidates);
	if (!Found.empty())
	{
		return Found;
	}
	return (ExeDir / Rel).lexically_normal().string();
}
