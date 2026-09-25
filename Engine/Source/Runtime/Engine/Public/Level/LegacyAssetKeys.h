#pragma once

#include "CoreMinimal.h"

/**
 * The packages that legacy content keys name (Leon, P14 part 2, until P15 replaces the `.llev` levels with `.lmap`
 * maps). A key is the content-relative file path a `.llev` actor record or a `.lmat` map stored: `materials/
 * M_WorldGrid.lmat`, `Meshes/Crate.lmesh`, `Textures/T_Default_D.png`. The content became `.lasset` packages; a key
 * resolves to the package the migration (LeonEd's MigrateLegacyContent) made of its file:
 *
 * 1. The key loses its extension (and a legacy `assets/` or `./` start); '\' becomes '/'.
 * 2. The migrated name: the leaf gets the class prefix of the extension unless it has it (`.lmat` M_, `.lmesh` SM_,
 *    images T_, `.wav` S_), and under `/Engine` the legacy engine folders `Materials/` and `Textures/` are
 *    `EngineMaterials/` (`materials/M_WorldGrid.lmat` is `/Engine/EngineMaterials/M_WorldGrid`).
 * 3. The candidates are the migrated name, then the key as it is, under the content root the key is relative to,
 *    then under `/Game`, then under `/Engine`. The first whose package exists wins.
 *
 * **Content roots.** A level's keys are relative to the folder above its `Levels/` folder (`<Root>/Levels/X.llev`).
 * A content root under a mount point uses that mount point's path (`Engine/Content` is `/Engine`); any other folder
 * gets a mount point named after it for the rest of the process (MountContentDirectory: `.../RenderTest` is
 * `/RenderTest`, `/RenderTest_2` when that name maps elsewhere), so the migrated `.lasset` files next to a level
 * load from there.
 */
class ENGINE_API FLegacyAssetKeys
{
public:
	/** The prefix the migration gives the asset of a file extension (with or without the dot); empty for others. */
	[[nodiscard]] static FString GetPrefixForExtension(const FString& Extension);

	/** The key's path without its extension: '/' separators, no `assets/` or `./` start (step 1). */
	[[nodiscard]] static FString NormalizeKey(const FString& Key);

	/** The package the migration makes of Key's file under a content root's package path (step 2). */
	[[nodiscard]] static FString GetMigratedPackageName(const FString& ContentRootPath, const FString& Key);

	/** The candidate packages of Key, in order (step 3); ContentRootPath may be empty. */
	static void GetCandidatePackageNames(
		const FString& ContentRootPath, const FString& Key, TArray<FString>& OutPackageNames);

	/** The object path of Key's package (`/Engine/EngineMaterials/M_WorldGrid.M_WorldGrid`); empty for none. */
	[[nodiscard]] static FString ResolveKey(const FString& ContentRootPath, const FString& Key);

	/**
	 * The package path of a content folder: the path under the mount point that contains it (`/Engine`,
	 * `/Game/Pack`), else the root of a mount point registered for it, named after the folder.
	 */
	[[nodiscard]] static FString MountContentDirectory(const FString& ContentDir);
};
