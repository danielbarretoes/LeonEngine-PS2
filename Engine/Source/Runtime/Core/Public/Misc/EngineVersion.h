#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "HAL/PreprocessorHelpers.h"
#include "Serialization/Archive.h"

// Engine version from Engine/Build/Build.version (LeonBuildTool defines ENGINE_{MAJOR,MINOR,PATCH}_VERSION).
#ifndef ENGINE_MAJOR_VERSION
	#define ENGINE_MAJOR_VERSION 0
#endif
#ifndef ENGINE_MINOR_VERSION
	#define ENGINE_MINOR_VERSION 0
#endif
#ifndef ENGINE_PATCH_VERSION
	#define ENGINE_PATCH_VERSION 0
#endif

/** The branch of Build.version ("BranchName") (UE: BRANCH_NAME). */
#ifndef BRANCH_NAME
	#define BRANCH_NAME "LeonEngine"
#endif

/** "Major.Minor.Patch" string literal. */
#define LEON_ENGINE_VERSION_STRING                                                                                     \
	PREPROCESSOR_TO_STRING(ENGINE_MAJOR_VERSION)                                                                       \
	"." PREPROCESSOR_TO_STRING(ENGINE_MINOR_VERSION) "." PREPROCESSOR_TO_STRING(ENGINE_PATCH_VERSION)

/** Engine version for logs and window titles (e.g. "0.12.0"). */
[[nodiscard]] inline constexpr const char* EngineVersionString()
{
	return LEON_ENGINE_VERSION_STRING;
}

/**
 * An engine version as packages record it: major, minor, patch, changelist and branch (UE: FEngineVersion). Saved
 * packages store the version that wrote them (FPackageFileSummary::SavedByEngineVersion).
 */
class CORE_API FEngineVersion
{
public:
	FEngineVersion() = default;

	FEngineVersion(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist, const FString& InBranch)
		: Major(InMajor)
		, Minor(InMinor)
		, Patch(InPatch)
		, Changelist(InChangelist)
		, Branch(InBranch)
	{
	}

	/** The version of the running engine (UE: FEngineVersion::Current). Leon builds have changelist 0. */
	static FEngineVersion Current()
	{
		return FEngineVersion(uint16(ENGINE_MAJOR_VERSION), uint16(ENGINE_MINOR_VERSION), uint16(ENGINE_PATCH_VERSION),
			0u, FString(TEXT(BRANCH_NAME)));
	}

	FORCEINLINE uint16 GetMajor() const
	{
		return Major;
	}

	FORCEINLINE uint16 GetMinor() const
	{
		return Minor;
	}

	FORCEINLINE uint16 GetPatch() const
	{
		return Patch;
	}

	FORCEINLINE uint32 GetChangelist() const
	{
		return Changelist;
	}

	FORCEINLINE const FString& GetBranch() const
	{
		return Branch;
	}

	/** True for the default (all zero) version (UE: IsEmpty). */
	FORCEINLINE bool IsEmpty() const
	{
		return Major == 0 && Minor == 0 && Patch == 0;
	}

	/** "Major.Minor.Patch-Changelist+Branch" (UE's full form). */
	FString ToString() const
	{
		return FString::Printf(
			TEXT("%u.%u.%u-%u+%s"), uint32(Major), uint32(Minor), uint32(Patch), Changelist, *Branch);
	}

	friend bool operator==(const FEngineVersion& A, const FEngineVersion& B)
	{
		return A.Major == B.Major && A.Minor == B.Minor && A.Patch == B.Patch && A.Changelist == B.Changelist &&
			A.Branch == B.Branch;
	}

	friend bool operator!=(const FEngineVersion& A, const FEngineVersion& B)
	{
		return !(A == B);
	}

	/** Major, minor and patch as uint16, the changelist as uint32, then the branch (UE). */
	friend FArchive& operator<<(FArchive& Ar, FEngineVersion& Version)
	{
		Ar << Version.Major << Version.Minor << Version.Patch << Version.Changelist << Version.Branch;
		return Ar;
	}

private:
	uint16 Major = 0;
	uint16 Minor = 0;
	uint16 Patch = 0;
	uint32 Changelist = 0;
	FString Branch;
};
