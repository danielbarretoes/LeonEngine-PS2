#pragma once

// Long package names and their files (UE: Misc/PackageName.h).

#include "CoreMinimal.h"

class UPackage;
struct FGuid;

/**
 * Converts between long package names ("/Game/Maps/Arena") and package files, through mount points (UE: FPackageName,
 * the subset Leon uses). A mount point maps a root to a content folder: "/Engine/" to FPaths::EngineContentDir(),
 * "/Game/" to FPaths::ProjectContentDir(), and whatever RegisterMountPoint adds (plugins, the tests).
 * "/Script/<Module>" names the compiled-in package of a module: a valid read-only root with no file. Asset packages end
 * in ".lasset", map packages in ".lmap".
 */
class COREUOBJECT_API FPackageName
{
public:
	/** ".lasset" (UE: GetAssetPackageExtension, ".uasset"). */
	static const FString& GetAssetPackageExtension();

	/** ".lmap" (UE: GetMapPackageExtension, ".umap"). */
	static const FString& GetMapPackageExtension();

	/** True for ".lasset" / ".lmap", with or without the dot (UE: IsPackageExtension). */
	static bool IsPackageExtension(const TCHAR* Ext);

	/**
	 * Maps RootPath ("/MyPlugin/", slashes added when missing) to the folder ContentPath; a later registration of the
	 * same root replaces the folder (UE: RegisterMountPoint).
	 */
	static void RegisterMountPoint(const FString& RootPath, const FString& ContentPath);

	/** Removes a mount point registered with RegisterMountPoint (UE: UnRegisterMountPoint). */
	static void UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath);

	/** True when RootPath is a mount point ("/Game/", "/Engine/" or a registered one) (UE: MountPointExists). */
	static bool MountPointExists(const FString& RootPath);

	/** True for "/Script/..." (UE: IsScriptPackage). */
	static bool IsScriptPackage(const FString& InPackageName);

	/**
	 * True for "/<Root>/Path/Name" under a mount point, without invalid characters (\ : * ? " < > | ' , . & ! ~ @ #,
	 * spaces, control characters), "//" or a trailing '/'. "/Script/<Module>" is valid only with bIncludeReadOnlyRoots
	 * (UE: IsValidLongPackageName).
	 */
	static bool IsValidLongPackageName(
		const FString& InLongPackageName, bool bIncludeReadOnlyRoots = false, FText* OutReason = nullptr);

	/**
	 * "/Game/Maps/Arena" to "<ProjectContentDir>Maps/Arena" + InExtension; false for an invalid name or one without a
	 * file (/Script) (UE: TryConvertLongPackageNameToFilename).
	 */
	static bool TryConvertLongPackageNameToFilename(
		const FString& InLongPackageName, FString& OutFilename, const FString& InExtension = FString());

	/** TryConvertLongPackageNameToFilename that must succeed; logs and returns InLongPackageName otherwise (UE). */
	static FString LongPackageNameToFilename(const FString& InLongPackageName, const FString& InExtension = FString());

	/**
	 * A file under a mount point's content folder (relative paths are taken from the launch folder; the extension is
	 * dropped) to its long package name; a long package name is returned as is. False, with OutFailureReason, when no
	 * mount point contains the file (UE: TryConvertFilenameToLongPackageName).
	 */
	static bool TryConvertFilenameToLongPackageName(
		const FString& InFilename, FString& OutPackageName, FString* OutFailureReason = nullptr);

	/** TryConvertFilenameToLongPackageName that must succeed; logs and returns an empty string otherwise (UE). */
	static FString FilenameToLongPackageName(const FString& InFilename);

	/**
	 * True when the package has a file, ".lasset" or ".lmap", or bytes registered with
	 * FLinkerLoad::RegisterInMemoryPackage; /Script packages never do. OutFilename receives the file (UE:
	 * DoesPackageExist; Leon does not check Guid).
	 */
	static bool DoesPackageExist(
		const FString& LongPackageName, const FGuid* Guid = nullptr, FString* OutFilename = nullptr);

	/** Finds InPackageFilename (no extension) + ".lasset" or ".lmap" (UE: FindPackageFileWithoutExtension). */
	static bool FindPackageFileWithoutExtension(const FString& InPackageFilename, FString& OutFilename);

	/** "/Game/Maps/Arena.Arena:PersistentLevel" to "/Game/Maps/Arena" (UE: ObjectPathToPackageName). */
	static FString ObjectPathToPackageName(const FString& InObjectPath);

	/** "/Game/Maps/Arena.Arena:PersistentLevel" to "PersistentLevel" (UE: ObjectPathToObjectName). */
	static FString ObjectPathToObjectName(const FString& InObjectPath);

	/** "/Game/Maps/Arena" to "Arena" (UE: GetShortName). */
	static FString GetShortName(const FString& LongName);
	static FString GetShortName(const UPackage* Package);
	static FString GetShortName(const FName& LongName);
	static FString GetShortName(const TCHAR* LongName);

	/** "/Game/Maps/Arena" to "/Game/Maps" (UE: GetLongPackagePath). */
	static FString GetLongPackagePath(const FString& InLongPackageName);

	/**
	 * "/Game/Maps/Arena" to "/Game/", "Maps/" and "Arena" ("Game/" with bStripRootLeadingSlash); false when no mount
	 * point matches (UE: SplitLongPackageName).
	 */
	static bool SplitLongPackageName(const FString& InLongPackageName, FString& OutPackageRoot, FString& OutPackagePath,
		FString& OutPackageName, const bool bStripRootLeadingSlash = false);

	/** The mount point of a package path: "Game" for "/Game/Maps/Arena" ("/Game/" without bWithoutSlashes) (UE). */
	static FName GetPackageMountPoint(const FString& InPackagePath, bool InWithoutSlashes = true);
};
