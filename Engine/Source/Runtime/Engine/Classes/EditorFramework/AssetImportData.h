#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AssetImportData.generated.h"

/** One source file of an imported asset (UE: FAssetImportInfo::FSourceFile, a nested struct there). */
USTRUCT()
struct ENGINE_API FAssetImportSourceFile
{
	GENERATED_BODY()

	FAssetImportSourceFile() = default;
	FAssetImportSourceFile(const FString& InRelativeFilename, const FString& InFileHash)
		: RelativeFilename(InRelativeFilename)
		, FileHash(InFileHash)
	{
	}

	/**
	 * The file, relative to the source root of the asset's package (UAssetImportData::GetSourceRootDir), with '/'
	 * separators; absolute only when it cannot be made relative (another drive). UE stores it relative to the package
	 * file.
	 */
	UPROPERTY()
	FString RelativeFilename;

	/** The MD5 of the file's bytes as 32 lower-case hex digits (UE: FileHash, an FMD5Hash). */
	UPROPERTY()
	FString FileHash;
};

/** The source files of an imported asset (UE: FAssetImportInfo). No timestamps: a reimport saves the same bytes. */
USTRUCT()
struct ENGINE_API FAssetImportInfo
{
	GENERATED_BODY()

	/** UE: SourceFiles. */
	UPROPERTY()
	TArray<FAssetImportSourceFile> SourceFiles;
};

/**
 * Where an asset was imported from and how (UE: UAssetImportData, EditorFramework/AssetImportData.h): the source files
 * with their MD5, and the import settings the factory applied. Every imported asset (UTexture, UStaticMesh,
 * USkeletalMesh, UAnimSequence, USoundWave) keeps one as its instanced `AssetImportData` subobject, made by the
 * factory that imported it; a reimport reads the source and the settings back from it.
 *
 * The data is editor-only (WITH_EDITORONLY_DATA, plan decision D14): the cook drops it and the PS2 and Shipping builds
 * never see it. It records nothing that changes between runs or machines (no timestamps, no absolute paths inside the
 * source root), so importing the same file again saves the same bytes (gate G5).
 *
 * **Source root.** Relative paths are relative to the directory above the content of the asset's mount point: the
 * engine directory for `/Engine` (`SourceArt/EngineMaterials/T_Default_D.png`), the project directory for `/Game`,
 * and the parent of the content directory for any other mount point.
 */
UCLASS()
class ENGINE_API UAssetImportData : public UObject
{
	GENERATED_BODY()

public:
	UAssetImportData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Only the editor needs it: a cooked package leaves it out, with its asset's reference to it (UE). */
	virtual bool IsEditorOnly() const override
	{
		return true;
	}

#if WITH_EDITORONLY_DATA
	/** The source files (UE: SourceData). */
	UPROPERTY()
	FAssetImportInfo SourceData;

	/**
	 * The import options the factory applied, by name (an ImportList.ini section's keys other than Source, Dest, Name
	 * and Type): a reimport applies them again. Leon keeps them here; UE keeps typed settings in subclasses
	 * (UFbxAssetImportData, ...) and on the asset.
	 */
	UPROPERTY()
	TMap<FString, FString> ImportSettings;

	/**
	 * Records AbsoluteFilename as the only source file, with FileHash (MD5 hex; empty: hashed now), relative to the
	 * source root when it can be (UE: Update).
	 */
	void Update(const FString& AbsoluteFilename, const FString& FileHash = FString());

	/** The first source file as an absolute path, empty when there is none (UE: GetFirstFilename). */
	[[nodiscard]] FString GetFirstFilename() const;

	/** Every source file as an absolute path (UE: ExtractFilenames). */
	void ExtractFilenames(TArray<FString>& AbsoluteFilenames) const;

	/** The recorded MD5 of the first source file, empty when there is none (Leon). */
	[[nodiscard]] FString GetFirstFileHash() const;

	/** AbsolutePath as stored: relative to the source root with '/' separators, when it can be (UE). */
	[[nodiscard]] FString SanitizeImportFilename(const FString& AbsolutePath) const;

	/** A stored path back to an absolute one (UE: ResolveImportFilename). */
	[[nodiscard]] FString ResolveImportFilename(const FString& RelativePath) const;

	/** The source root of this object's package (above). */
	[[nodiscard]] FString GetSourceRootDir() const;

	/** Replaces the import settings; they are kept sorted by name, so the saved order never changes (Leon). */
	void SetImportSettings(const TMap<FString, FString>& InSettings);

	/** The source root of a long package name (above; Leon). */
	[[nodiscard]] static FString GetSourceRootDir(const FString& LongPackageName);

	/** The MD5 of a file's bytes as lower-case hex, empty when it cannot be read (UE: FMD5Hash::HashFile). */
	[[nodiscard]] static FString HashFile(const FString& Filename);
#endif // WITH_EDITORONLY_DATA
};
