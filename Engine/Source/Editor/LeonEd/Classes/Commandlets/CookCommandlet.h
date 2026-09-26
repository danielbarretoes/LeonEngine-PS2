#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "CookCommandlet.generated.h"

class ITargetPlatform;

/** A texture the cook converted for its platform, and what it costs there (the VRAM report). */
struct FCookedTexture
{
	/** The texture's object path. */
	FString Name;
	int32 SourceSizeX = 0;
	int32 SourceSizeY = 0;
	int32 SizeX = 0;
	int32 SizeY = 0;
	EPixelFormat Format = PF_Unknown;
	/** The distinct colours before the palette (more than 256: approximated). */
	int32 SourceColors = 0;
	/** GS local memory: the texels' whole pages and the CLUT, in 256-byte blocks. */
	uint32 Blocks = 0;
};

/**
 * Cooks the game's content for a target platform (UE: UCookCommandlet, `-run=Cook`, cook by the book):
 *
 *   LeonCook <Project>.lproj -run=Cook -TargetPlatform=Win64|PS2 [-map=<Map>+<Map>] [-package=<Package>,...]
 *
 * 1. Seeds (GatherCookSeeds): the maps (-map=, else `+MapsToCook` of [/Script/UnrealEd.ProjectPackagingSettings],
 *    else every map under /Game/Maps, as UE cooks every map when none is listed), GameDefaultMap and
 *    ServerDefaultMap, the packages under each `+DirectoriesToAlwaysCook=(Path="/Game/...")`, and the engine's default
 *    assets its config names ([/Script/Engine.Engine] DefaultMaterialName, ...), all read from the target platform's
 *    config layers; -package= / -packagefolder= cook only those packages instead.
 * 2. The dependency closure (CollectDependencies): every package reached through the hard imports and the soft
 *    package references of the packages' tables (FLinkerLoad::CreateLinker, nothing is loaded).
 * 3. Each package, sorted, is loaded and saved into <Project>/Saved/Cooked/<Platform>/ (UE's layout:
 *    Engine/Content/..., <Project>/Content/...) with PKG_FilterEditorOnly | PKG_Cooked and the platform's name, so
 *    no editor-only data remains: the editor-only properties, and the import data of the assets and of the maps'
 *    worlds (UAssetImportData::IsEditorOnly).
 * 4. The rest of the build's files (StageNonPackageFiles): the config (the Base, Default and platform ini files, not
 *    the Editor ones), the engine's shaders and the .lproj.
 *
 * A platform whose texture formats include "Paletted" (the PS2) gets its textures as PF_P8 / PF_P4
 * (FPalettedTextureBuilder: powers of two up to 256, a CLUT of 256 or 16 colours), and a VRAM report of each map's
 * textures against the GS's texture arena in <Project>/Saved/Cooked/<Platform>-VramReport.txt
 * (Docs/PLANS/ps2-engine.md E3).
 *
 * The output folder is emptied first, and the output depends only on the content: two cooks give the same bytes.
 * Returns 0 when everything cooked.
 */
UCLASS()
class LEONED_API UCookCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCookCommandlet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 Main(const FString& Params) override;

	/** The packages the cook starts from, sorted and unique (step 1 above). */
	static void GatherCookSeeds(
		const ITargetPlatform& TargetPlatform, const TMap<FString, FString>& ParamsMap, TArray<FString>& OutPackages);

	/**
	 * Seeds and every package they reach through hard imports and soft package references, sorted. False (logged)
	 * when a seed or a hard import has no file; a soft reference to a missing package is a warning.
	 */
	static bool CollectDependencies(const TArray<FString>& Seeds, TArray<FString>& OutPackages);

	/**
	 * Loads a package and saves it cooked for TargetPlatform under CookedDir (step 3 above), its textures converted to
	 * the platform's format (added to OutTextures when given); false (logged) when it cannot be loaded or saved. The
	 * loaded package is left for the caller's next garbage collection, its textures as they were loaded.
	 */
	static bool CookPackage(const FString& PackageName, const ITargetPlatform& TargetPlatform, const FString& CookedDir,
		TArray<FCookedTexture>* OutTextures = nullptr);

	/**
	 * The VRAM report of a paletted cook (Docs/PLANS/ps2-engine.md E3): the textures of the Common packages' closure
	 * (the config's default assets and the directories always cooked, which any map may draw), then per map the other
	 * textures of its closure, and each map's total with the common ones against ArenaBlocks (TexturesByPackage: the
	 * cooked textures by package name). Sorted, so the same cook gives the same text. OutOverBudget gets the maps whose
	 * textures do not fit; the texture cache then uploads them again as it fills.
	 */
	static FString MakeVramReport(const TArray<FString>& Maps, const TArray<FString>& Common,
		const TMap<FString, TArray<FCookedTexture>>& TexturesByPackage, uint32 ArenaBlocks,
		TArray<FString>& OutOverBudget);

	/**
	 * The RAM report of a cook (Docs/PLANS/ps2-preview.md V2): the cooked bytes of the Common packages' closure, then
	 * per map the other packages of its closure, largest first, and each map's total with the common ones
	 * (BytesByPackage: the cooked file sizes by package name). The serialized size, which the loader reads: the heap
	 * the objects take once loaded is GMalloc's, measured on the EE (Budgets.md). Sorted, so the same cook gives the
	 * same text.
	 */
	static FString MakeRamReport(
		const TArray<FString>& Maps, const TArray<FString>& Common, const TMap<FString, int64>& BytesByPackage);

	/** Copies the config, the shaders and the .lproj into CookedDir (step 4 above); the file count, -1 on failure. */
	static int32 StageNonPackageFiles(const ITargetPlatform& TargetPlatform, const FString& CookedDir);

	/** <Project>/Saved/Cooked/<Platform>/. */
	static FString GetCookedDir(const FString& PlatformName);

	/**
	 * The cooked file of a package: "/Engine/X" is <CookedDir>Engine/Content/X, "/Game/X"
	 * <CookedDir><Project>/Content/X and any other mount point "/<Root>/X" <CookedDir><Root>/Content/X (UE's layout),
	 * with the extension of the package's file (.lasset unless it is a map); empty for a package under no mount point.
	 */
	static FString GetCookedFilename(
		const FString& PackageName, const FString& CookedDir, const FString& Extension = FString(TEXT(".lasset")));

	/** The folder of the project in the cooked and staged layout: its name, or "Game" without a project. */
	static FString GetProjectFolderName();
};
