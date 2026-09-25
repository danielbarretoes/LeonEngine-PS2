#pragma once

#include "CoreMinimal.h"

class UClass;
class UObject;
class UPackage;

/**
 * Asset naming, package files and content scans shared by the factories and the commandlets (Leon; UE spreads them over
 * IAssetTools, FPackageName and the asset registry).
 *
 * **Prefixes.** New assets are named with UE's prefixes: SM_ (UStaticMesh), SK_ (USkeletalMesh), SKEL_ (USkeleton),
 * A_ (UAnimSequence), BS_ (UBlendSpace1D), T_ (UTexture), M_ (UMaterialInterface), S_ (USoundWave). A source file
 * whose name already has the prefix keeps it (T_Default_D.png is T_Default_D, Cube.obj is SM_Cube).
 */
class LEONED_API FAssetImportUtils
{
public:
	/** The prefix of Class's assets ("SM_"), empty for a class without one. */
	[[nodiscard]] static FString GetAssetPrefix(const UClass* Class);

	/** Letters, digits and '_' kept; every other character becomes '_' (a valid object and package name). */
	[[nodiscard]] static FString SanitizeName(const FString& Name);

	/** The asset name for a source base name: sanitized, with Prefix unless it starts with it already. */
	[[nodiscard]] static FString MakeAssetName(const FString& Prefix, const FString& BaseName);

	/** MakeAssetName with Class's prefix. */
	[[nodiscard]] static FString MakeAssetName(const UClass* Class, const FString& BaseName);

	/**
	 * The asset AssetName of Class in the package PackageName: in memory, else loaded from its file; null when neither
	 * has it (no warning).
	 */
	[[nodiscard]] static UObject* FindOrLoadAsset(UClass* Class, const FString& PackageName, const FString& AssetName);

	/**
	 * Saves the package of Asset to its file under its mount point (`.lasset`, or `.lmap` for a map), with Asset and
	 * every public object of the package. False (logged) when it has no file name or the save fails.
	 */
	static bool SavePackage(UPackage* Package, UObject* Asset = nullptr);

	/** The file of a long package name: the existing `.lasset` / `.lmap`, else the `.lasset` it would have. */
	[[nodiscard]] static FString GetPackageFilename(const FString& PackageName);

	/**
	 * The long package names of every `.lasset` / `.lmap` file under a mount point's content ("/Engine/", "/Game/"),
	 * or under one of its folders ("/Game/Maps"), sorted.
	 */
	static void FindPackages(const FString& PackagePath, TArray<FString>& OutPackageNames);

	/**
	 * The mount points a content-wide commandlet walks: "/Engine/", and "/Game/" when the project's content exists
	 * (Leon; UE walks every mount point the asset registry knows).
	 */
	static void GetContentMountPoints(TArray<FString>& OutRoots);
};
