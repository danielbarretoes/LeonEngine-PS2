#include "Misc/Paths.h"

#include <array>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#endif

namespace
{

	std::filesystem::path& ActiveContentRootStorage()
	{
		static std::filesystem::path Root;
		return Root;
	}

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

	void AppendActivePackCandidates(std::vector<std::filesystem::path>& Out, const std::filesystem::path& Rel,
		const std::filesystem::path& UnderAssets)
	{
		const std::filesystem::path Active = ActiveContentRootStorage();
		if (Active.empty())
		{
			return;
		}
		const std::filesystem::path Content = FPaths::ProjectContentDir(Active);
		if (!Content.empty())
		{
			Out.push_back(Content / Rel);
			if (!UnderAssets.empty())
			{
				Out.push_back(Content / "assets" / UnderAssets);
				Out.push_back(Content / UnderAssets);
			}
		}
		Out.push_back(Active / Rel);
		if (!UnderAssets.empty())
		{
			Out.push_back(Active / "assets" / UnderAssets);
		}
	}

} // namespace

void FPaths::SetActiveContentRoot(const std::filesystem::path& ProjectOrPackRoot)
{
	std::error_code Ec;
	ActiveContentRootStorage() =
		ProjectOrPackRoot.empty() ? std::filesystem::path{} : ProjectOrPackRoot.lexically_normal();
	(void)Ec;
}

std::filesystem::path FPaths::GetActiveContentRoot()
{
	return ActiveContentRootStorage();
}

std::filesystem::path FPaths::ExecutableDir()
{
#if defined(_WIN32)
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

std::filesystem::path FPaths::ProjectContentDir(const std::filesystem::path& ProjectOrPackRoot)
{
	std::error_code Ec;
	const std::filesystem::path Root = ProjectOrPackRoot.lexically_normal();
	if (Root.empty())
	{
		return {};
	}
	const std::filesystem::path Content = Root / "Content";
	const bool bContentLevels = std::filesystem::is_directory(Content / "Levels", Ec) && !Ec;
	const bool bLegacyLevels = std::filesystem::is_directory(Root / "Levels", Ec) && !Ec;
	if (bContentLevels)
	{
		return Content;
	}
	if (bLegacyLevels)
	{
		return Root;
	}
	// Default Unreal-like layout (folder may be created by the editor).
	return Content;
}

std::string FPaths::ResolveContentAssetPath(
	const std::filesystem::path& ProjectOrPackRoot, const std::string& RelativeOrKey)
{
	std::error_code Ec;
	if (RelativeOrKey.empty())
	{
		return {};
	}
	const std::filesystem::path Key(RelativeOrKey);
	if (Key.is_absolute() && std::filesystem::exists(Key, Ec) && !Ec)
	{
		return Key.lexically_normal().string();
	}

	const std::filesystem::path Content = FPaths::ProjectContentDir(ProjectOrPackRoot);
	if (!Content.empty())
	{
		const std::filesystem::path InContent = (Content / Key).lexically_normal();
		if (std::filesystem::exists(InContent, Ec) && !Ec)
		{
			return InContent.string();
		}
	}

	if (!ProjectOrPackRoot.empty())
	{
		const std::filesystem::path InRoot = (ProjectOrPackRoot / Key).lexically_normal();
		if (std::filesystem::exists(InRoot, Ec) && !Ec)
		{
			return InRoot.string();
		}
	}

	// Engine / exe staging only — never another project's Content/.
	return FPaths::ResolveAssetPath(RelativeOrKey);
}

std::string FPaths::ResolveAssetPath(const std::string& RelativePath)
{
	const std::filesystem::path ExeDir = FPaths::ExecutableDir();
	const std::filesystem::path Rel(RelativePath);
	const std::filesystem::path UnderAssets = StripAssetsPrefix(Rel);

	// Active pack Content wins outright (never lose to a newer Engine/staging copy).
	{
		std::vector<std::filesystem::path> PackCandidates;
		AppendActivePackCandidates(PackCandidates, Rel, UnderAssets);
		const std::string PackHit = NewestExisting(PackCandidates);
		if (!PackHit.empty())
		{
			return PackHit;
		}
	}

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

std::string FPaths::ResolveProjectsDir()
{
	// Prefer a Projects/ whose parent is a real Leon root (Build/Dependencies.cmake +
	// Engine/). Avoid the thin POST_BUILD staging folder beside the editor exe
	// (e.g. Editor/build/Release/Projects) — that breaks game CMakeLists ../.. paths.
	const std::filesystem::path ExeDir = FPaths::ExecutableDir();
	std::error_code Ec;
	const auto IsSdkProjects = [&](const std::filesystem::path& ProjectsDir)
	{
		if (!std::filesystem::is_directory(ProjectsDir, Ec) || Ec)
		{
			return false;
		}
		const std::filesystem::path Root = ProjectsDir.parent_path();
		return std::filesystem::is_regular_file(Root / "Build" / "Dependencies.cmake", Ec) && !Ec &&
			std::filesystem::is_directory(Root / "Engine", Ec) && !Ec;
	};

	const std::filesystem::path Candidates[] = {
		ExeDir / "Projects", // Dist/LeonEditor/Projects
		ExeDir / ".." / ".." / "Projects", // Editor/build-fast → repo/Projects
		ExeDir / ".." / ".." / ".." / "Projects", // Editor/build/Release → repo/Projects
		std::filesystem::path("Projects"),
		std::filesystem::path("..") / "Projects",
		std::filesystem::path("..") / ".." / "Projects",
		std::filesystem::path("..") / ".." / ".." / "Projects",
	};
	for (const auto& C : Candidates)
	{
		const std::filesystem::path P = C.lexically_normal();
		if (IsSdkProjects(P))
		{
			return P.string();
		}
	}

	// Fallback without calling FPaths::ResolveAssetPath("Projects") — that would recurse via Projects scan.
	if (std::filesystem::is_directory(ExeDir / "Projects", Ec) && !Ec)
	{
		return (ExeDir / "Projects").lexically_normal().string();
	}
	if (std::filesystem::is_directory("Projects", Ec) && !Ec)
	{
		return std::filesystem::path("Projects").lexically_normal().string();
	}
	return (ExeDir / "Projects").lexically_normal().string();
}
