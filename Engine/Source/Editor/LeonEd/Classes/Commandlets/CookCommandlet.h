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
	/** Level 0 and the mips (FPalettedTextureBuilder::GetNumMips). */
	int32 NumMips = 1;
	/**
	 * GS local memory: the levels' and the CLUT's blocks in the GS's layout (FGSTextureLayout::GetFootprint), 256 bytes
	 * each.
	 */
	uint32 Blocks = 0;
};

/** A sound the cook made SPU2 ADPCM, and what it costs there (the SPU2 RAM report). */
struct FCookedSound
{
	/** The sound's object path. */
	FString Name;
	int32 SourceSampleRate = 0;
	int32 SampleRate = 0;
	/** The ADPCM's frames (28 a block) and bytes: its SPU2 RAM. */
	int32 NumFrames = 0;
	int32 Bytes = 0;
	bool bLooping = false;
};

/** A mesh the cook saved, and what the budgets check on it. */
struct FCookedMesh
{
	/** The mesh's object path. */
	FString Name;
	int32 Triangles = 0;
	/** The skeleton's bones (0 for a static mesh). */
	int32 Bones = 0;
};

/** What the cook made of one package: the assets the reports and the budgets read. */
struct FCookedPackageInfo
{
	TArray<FCookedTexture> Textures;
	TArray<FCookedSound> Sounds;
	TArray<FCookedMesh> Meshes;
	/** The Static meshes of a map whose baked lighting needs rebuilding (N22's warning, repeated from the cache). */
	int32 UnbakedLighting = 0;
};

/** What GetCookKey reads of each package, kept across the packages of a cook. */
struct FCookKeyCache
{
	/** SHA-1 of each package's source file (empty: unreadable). */
	TMap<FString, FString> SourceHashes;
	/** Each package's hard imports (the packages its import table names). */
	TMap<FString, TArray<FString>> Imports;
};

/**
 * The cook's hard budgets (Docs/PLANS/ps2-shipping.md N23): `[/Script/LeonEd.CookSettings]` of the target platform's
 * Game config (Engine/Config/BaseGame.ini has the defaults; a project or a platform layer overrides them), each also
 * taken from the command line (`-MaxTextureSize=128`). An asset or a map over one fails the cook with an error that
 * names it, the figure and the setting. The sizes are KB; 0 takes the platform's own limit.
 */
struct LEONED_API FCookBudgets
{
	/** Per map, with the common assets: the GS texture VRAM its textures take (0: the GS texture arena). */
	int32 MapVramKB = 0;
	/**
	 * Per map, with the common assets: the EE RAM, estimated as RuntimeBaseKB plus its cooked bytes times
	 * RuntimeExpansionPercent / 100 (0: `[Core.MemoryBudgets] Total` of the platform's Engine config; none without it).
	 */
	int32 MapRamKB = 0;
	/**
	 * The heap the engine and the game take besides what grows with the content: the frame's GS lists and DMA chains
	 * among them (GMalloc on the EE, calibrated from MeasurePS2's peaks in N29, Budgets.md).
	 */
	int32 RuntimeBaseKB = 3072;
	/** Loaded objects over their cooked bytes (the objects, their arrays and the bulk data). */
	int32 RuntimeExpansionPercent = 200;
	/** Per map, with the common sounds: the SPU2 RAM (0: what audsrv leaves, FSpuAdpcm::SoundRamBytes). */
	int32 MapSoundRamKB = 0;
	/** Per mesh (static or skeletal): its triangles. */
	int32 MaxMeshTriangles = 4096;
	/** Per skeletal mesh: its skeleton's bones. */
	int32 MaxMeshBones = 64;
	/** Per texture: the longer side once cooked (at most 256, the paletted cook's). */
	int32 MaxTextureSize = 256;
	/** Per texture: 4 (PSMT4) or 8 (PSMT8). */
	int32 MaxTextureBitsPerPixel = 8;

	/** The section in the Game config. */
	static const TCHAR* const Section;

	/**
	 * The budgets of TargetPlatform: the Game config of its layers, then ParamsMap's values of the same names, with 0
	 * resolved to the platform's limits.
	 */
	static FCookBudgets Load(const ITargetPlatform& TargetPlatform, const TMap<FString, FString>& ParamsMap);

	/** The errors of the assets of Info over a budget (empty when all fit). */
	void CheckAssets(const FCookedPackageInfo& Info, TArray<FString>& OutErrors) const;

	/** A map's RAM estimate (bytes) from its cooked bytes. */
	[[nodiscard]] int64 EstimateRamBytes(int64 CookedBytes) const
	{
		return (int64(RuntimeBaseKB) * 1024) + ((CookedBytes * RuntimeExpansionPercent) / 100);
	}
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
 *    the Editor ones), the .lproj, and the engine's shaders for a platform with shader formats (Win64, not the PS2).
 *
 * A platform whose texture formats include "Paletted" (the PS2) gets its textures as PF_P8 / PF_P4
 * (FPalettedTextureBuilder: powers of two up to 256, a CLUT of 256 or 16 colours, stored load-in-place: the GS's CLUT
 * image, then each level's indices, Docs/PLANS/ps2-shipping.md N23), and a VRAM report of each map's textures against
 * its budget in <Project>/Saved/Cooked/<Platform>-VramReport.txt (Docs/PLANS/ps2-engine.md E3). A platform whose wave
 * formats include the SPU2's ADPCM (both) gets each sound's ADPCM (USoundWave::CacheCompressedData) instead of its PCM,
 * and an SPU2 RAM report of each map's sounds in <Project>/Saved/Cooked/<Platform>-SoundReport.txt; a map whose sounds
 * do not fit the SPU2 RAM (FSpuAdpcm::SoundRamBytes) fails the cook (Docs/PLANS/ps2-shipping.md N19).
 *
 * Hard budgets (FCookBudgets, N23): a texture, a mesh or a map over its budget (the maps' VRAM, RAM and SPU2 RAM) is an
 * error and fails the cook.
 *
 * Incremental (N23, `-iterate`, the default): each package's cooked file and what the reports need of it are kept in
 * <Project>/Intermediate/CookCache/<Platform>/, keyed by GetCookKey (the source package and its hard imports' bytes,
 * the cooker version and the platform's settings); a package whose key has not changed is copied from there instead of
 * cooked. `-full` cooks every package (and refreshes the cache). Either way the output folder is emptied first and the
 * output depends only on the content: two cooks, full or incremental, give the same bytes. Returns 0 when everything
 * cooked within the budgets.
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
	 * the platform's format (added to OutTextures when given) and its sounds saved as their ADPCM (added to OutSounds);
	 * false (logged) when it cannot be loaded or saved, or a sound has no format the platform plays. The loaded package
	 * is left for the caller's next garbage collection, its textures as they were loaded.
	 */
	static bool CookPackage(const FString& PackageName, const ITargetPlatform& TargetPlatform, const FString& CookedDir,
		TArray<FCookedTexture>* OutTextures = nullptr, TArray<FCookedSound>* OutSounds = nullptr,
		TArray<FCookedMesh>* OutMeshes = nullptr, int32* OutUnbakedLighting = nullptr);

	/**
	 * The version of what the cook writes: bump it when a change to the cook or to a type's cooked serialization
	 * changes the bytes of a cooked package, so the cook cache (GetCookKey) forgets what older cooks made.
	 */
	static constexpr int32 CookerVersion = 1;

	/**
	 * The cook cache's key of a package (N23): SHA-1 over the cooker version, the package format version, the
	 * platform's settings (its names and formats) and the name and the source bytes of the package and of every package
	 * its hard imports reach (a change to an import can change the importer's cooked bytes). Empty when a source file
	 * cannot be read. Cache keeps each package's source hash and hard imports across calls.
	 */
	static FString GetCookKey(const FString& PackageName, const ITargetPlatform& TargetPlatform, FCookKeyCache& Cache);

	/**
	 * CookPackage through the cook cache in CacheDir (N23): with bUseCache and a cached package of the same Key, the
	 * cached file is copied into CookedDir and OutInfo read from the cache (bOutFromCache); otherwise the package is
	 * cooked and stored in the cache under Key. False as CookPackage.
	 */
	static bool CookPackageCached(const FString& PackageName, const ITargetPlatform& TargetPlatform,
		const FString& CookedDir, const FString& CacheDir, const FString& Key, bool bUseCache,
		FCookedPackageInfo& OutInfo, bool& bOutFromCache);

	/** <Project>/Intermediate/CookCache/<Platform>/. */
	static FString GetCookCacheDir(const FString& PlatformName);

	/**
	 * A config file as a game on the target reads it (N23): without the comments, the blank lines and the sections
	 * only the editor and the cook read (/Script/UnrealEd.*, /Script/LeonEd.*).
	 */
	static FString StripConfigForTarget(const FString& Text);

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
	 * (BytesByPackage: the cooked file sizes by package name). The serialized size, which the loader reads, and the
	 * runtime estimate against Budgets.MapRamKB (FCookBudgets::EstimateRamBytes; the heap the objects really take is
	 * GMalloc's, measured on the EE, Budgets.md): OutOverBudget gets the maps over it. Sorted, so the same cook gives
	 * the same text.
	 */
	static FString MakeRamReport(const TArray<FString>& Maps, const TArray<FString>& Common,
		const TMap<FString, int64>& BytesByPackage, const FCookBudgets& Budgets, TArray<FString>& OutOverBudget);

	/**
	 * The SPU2 RAM report (Docs/PLANS/ps2-shipping.md N19): the sounds of the Common packages' closure, then per map
	 * the other sounds of its closure, and each map's total with the common ones against BudgetBytes (SoundsByPackage:
	 * the cooked sounds by package name). Sorted, so the same cook gives the same text. OutOverBudget gets the maps
	 * whose sounds do not fit: the audio device could not keep them all resident.
	 */
	static FString MakeSoundReport(const TArray<FString>& Maps, const TArray<FString>& Common,
		const TMap<FString, TArray<FCookedSound>>& SoundsByPackage, int32 BudgetBytes, TArray<FString>& OutOverBudget);

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
