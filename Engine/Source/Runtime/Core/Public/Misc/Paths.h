#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"

/**
 * Engine / project directories and path string helpers (UE: FPaths). Directories end with '/' and use '/'.
 *
 * Desktop: the engine and project folders are found relative to the executable (LeonBuildTool records the
 * relative paths of each target), so the directories are absolute. PS2: a staged layout under the executable's
 * device folder: <Base>/Engine/..., <Base>/<Project>/....
 */
class CORE_API FPaths
{
public:
	/** The working directory when the process started (UE: LaunchDir). */
	static FString LaunchDir();

	/** Folder holding Engine/ (and the Game/ projects on desktop) (UE: RootDir). */
	static FString RootDir();

	static FString EngineDir();
	static FString EngineContentDir();
	static FString EngineConfigDir();
	static FString EngineIntermediateDir();
	static FString EngineSavedDir();
	static FString EnginePluginsDir();
	static FString EngineSourceDir();

	/** Engine/Platforms/ (UE: EnginePlatformExtensionsDir). */
	static FString EnginePlatformExtensionsDir();

	/**
	 * The project's folder: the .lproj's folder once one is set, the target's project otherwise; programs without
	 * a project use Engine/Programs/<Name>/ like UE.
	 */
	static FString ProjectDir();
	static FString ProjectContentDir();
	static FString ProjectConfigDir();
	static FString ProjectSavedDir();
	static FString ProjectIntermediateDir();
	static FString ProjectPluginsDir();
	static FString ProjectLogDir();
	static FString ProjectPlatformExtensionsDir();

	/** Saved/Config/: the user's config layer (UE: GeneratedConfigDir). */
	static FString GeneratedConfigDir();

	/** Path of the .lproj, empty when none is set (UE: GetProjectFilePath). */
	static const FString& GetProjectFilePath();
	static bool SetProjectFilePath(const FString& NewGameProjectFilePath);
	static bool IsProjectFilePathSet();

	/** "png" (or ".png" with bIncludeDot) (UE: GetExtension). */
	static FString GetExtension(const FString& InPath, bool bIncludeDot = false);

	/** File name with extension, without the path (UE: GetCleanFilename). */
	static FString GetCleanFilename(const FString& InPath);

	/** File name without extension; with bRemovePath false the path stays (UE: GetBaseFilename). */
	static FString GetBaseFilename(const FString& InPath, bool bRemovePath = true);

	/** Everything before the last separator, without it (UE: GetPath). */
	static FString GetPath(const FString& InPath);

	/** Last element, file or directory (UE: GetPathLeaf). */
	static FString GetPathLeaf(const FString& InPath);

	/** Replaces an existing extension; a path without one is returned as is (UE: ChangeExtension). */
	static FString ChangeExtension(const FString& InPath, const FString& InNewExtension);

	/** Replaces or adds the extension (UE: SetExtension). */
	static FString SetExtension(const FString& InPath, const FString& InNewExtension);

	static bool FileExists(const FString& InPath);
	static bool DirectoryExists(const FString& InPath);

	/** "C:", "C:/", "host:" and the like (UE: IsDrive, extended to PS2 device names). */
	static bool IsDrive(const FString& InPath);

	/** Not rooted: no leading '/', no drive or device (UE: IsRelative). */
	static bool IsRelative(const FString& InPath);

	/** Backslashes to '/' (UE: NormalizeFilename). */
	static void NormalizeFilename(FString& InPath);

	/** Backslashes to '/' and no trailing '/' (except "C:/" and "//") (UE: NormalizeDirectoryName). */
	static void NormalizeDirectoryName(FString& InPath);

	/** Removes "/./" and "dir/.." pairs; false when a ".." would go above the start (UE: CollapseRelativeDirectories).
	 */
	static bool CollapseRelativeDirectories(FString& InPath);

	/** Turns "//" into "/" (UE: RemoveDuplicateSlashes). */
	static void RemoveDuplicateSlashes(FString& InPath);

	/** Full, collapsed path; relative paths are taken from LaunchDir (UE: ConvertRelativePathToFull). */
	static FString ConvertRelativePathToFull(const FString& InPath);
	static FString ConvertRelativePathToFull(const FString& BasePath, const FString& InPath);

	/** Makes InPath relative to InRelativeTo's folder; a directory needs its trailing '/' (UE: MakePathRelativeTo). */
	static bool MakePathRelativeTo(FString& InPath, const TCHAR* InRelativeTo);

	/** Same file or directory once both are full paths, ignoring case (UE: IsSamePath). */
	static bool IsSamePath(const FString& PathA, const FString& PathB);

	/** InPath is InDirectory or inside it (UE: IsUnderDirectory). */
	static bool IsUnderDirectory(const FString& InPath, const FString& InDirectory);

	/** Path, base name and extension (UE: Split). */
	static void Split(const FString& InPath, FString& PathPart, FString& FilenamePart, FString& ExtensionPart);

	/** A file name in Path that does not exist yet: Prefix + GUID + Extension (UE: CreateTempFilename). */
	static FString CreateTempFilename(const TCHAR* Path, const TCHAR* Prefix = "", const TCHAR* Extension = ".tmp");

	/** Joins paths with '/' (UE: Combine). */
	template <typename... PathTypes>
	static FString Combine(PathTypes&&... InPaths)
	{
		const TCHAR* Paths[] = {GetTCharPtr(InPaths)...};
		FString Out;
		CombineInternal(Out, Paths, int32(sizeof...(InPaths)));
		return Out;
	}

	/**
	 * Legacy (until P15): finds a content file by the old keys ("LevelTemplates/X.llev", "Shaders/x.vert", an
	 * optional "assets/" prefix): as given, then the project's and the engine's Content (Shaders/ under Engine).
	 * Returns the engine content path when nothing exists.
	 */
	static FString ResolveLegacyContentPath(const FString& RelativePath);

private:
	static const TCHAR* GetTCharPtr(const TCHAR* Ptr)
	{
		return Ptr;
	}
	static const TCHAR* GetTCharPtr(const FString& Str)
	{
		return *Str;
	}

	static void CombineInternal(FString& OutPath, const TCHAR** Pathes, int32 NumPathes);
};
