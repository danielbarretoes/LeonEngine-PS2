#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "Factory.generated.h"

// LeonEd reads and writes the editor-only data of the assets (their UAssetImportData): it builds where that data
// exists, the desktop builds outside Shipping (plan decision D14).
#if !WITH_EDITORONLY_DATA
	#error "LeonEd needs WITH_EDITORONLY_DATA: it does not build for Shipping or the PS2"
#endif

class UAssetImportData;

/**
 * Makes assets (UE: UFactory, Factories/Factory.h): from a source file (bEditorImport: FactoryCreateFile, which reads
 * the file and calls FactoryCreateBinary) or from nothing (bCreateNew: FactoryCreateNew). The import commandlet picks
 * the factory by the file's extension (Formats) and the asset class (SupportedClass).
 *
 * **Settings.** A factory's options are its UPROPERTYs. The ImportList.ini keys and `-Key=Value` switches that name one
 * are applied with ApplyImportSettings (UE applies automated import settings from JSON the same way), and the asset's
 * UAssetImportData keeps them for the reimport.
 *
 * **Existing assets.** Leon cannot construct a new object over an existing one (UE: NewObject with the same name
 * replaces it), so CreateOrOverwriteAsset returns the object that already has the name and class, and the factory
 * fills it again: importing over an asset reimports it in place and keeps every reference to it.
 *
 * Leon's factories also implement FReimportHandler themselves (UE has separate UReimport*Factory classes). There is no
 * FFeedbackContext parameter: errors go to the log (LogLeonEd).
 */
UCLASS(Abstract)
class LEONED_API UFactory : public UObject
{
	GENERATED_BODY()

public:
	UFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The class of the assets the factory makes (UE: SupportedClass). */
	UPROPERTY()
	TSubclassOf<UObject> SupportedClass;

	/** One "extension;Description" entry per file type the factory imports (UE: Formats). */
	UPROPERTY()
	TArray<FString> Formats;

	/** The factory makes new assets without a file (UE: bCreateNew). */
	UPROPERTY()
	uint8 bCreateNew : 1;

	/** The factory imports files (UE: bEditorImport). */
	UPROPERTY()
	uint8 bEditorImport : 1;

	/** Among the factories that can import a file, the highest priority wins (UE: ImportPriority). */
	UPROPERTY()
	int32 ImportPriority = 100;

	/**
	 * Assets the last import made or reused besides the one it returned: a skeleton, materials, textures (UE:
	 * AdditionalImportedObjects). The import commandlet saves their packages too.
	 */
	UPROPERTY(Transient)
	TArray<UObject*> AdditionalImportedObjects;

	/** The file the current import reads (UE: CurrentFilename); FactoryCreateBinary records it as the source. */
	static FString CurrentFilename;

	/** True when Filename's extension is one of Formats (UE: FactoryCanImport, which may also look inside). */
	virtual bool FactoryCanImport(const FString& Filename);

	/** True when the factory makes Class (or a subclass of it) (UE: DoesSupportClass). */
	virtual bool DoesSupportClass(UClass* Class);

	/** The class the factory makes (UE: ResolveSupportedClass). */
	virtual UClass* ResolveSupportedClass();

	/** A new asset without a file (UE: FactoryCreateNew). The base makes nothing. */
	virtual UObject* FactoryCreateNew(
		UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context);

	/**
	 * An asset from the bytes of a file of type Type (its extension) (UE: FactoryCreateBinary). CurrentFilename is
	 * the file. The base makes nothing.
	 */
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled);

	/**
	 * An asset from a file (UE: FactoryCreateFile): the base reads it and calls FactoryCreateBinary; factories whose
	 * libraries read the file themselves override it. Parms is unused (UE: the command line of the import).
	 */
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled);

	/**
	 * Imports Filename as InName in InParent with InFactory, or the best factory for the file and Class (UE:
	 * StaticImportObject, trimmed). Null, with an error, when no factory can import it or the import fails.
	 */
	static UObject* StaticImportObject(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
		const FString& Filename, UFactory* InFactory = nullptr);

	/**
	 * The factory class that imports Filename, preferring those that make PreferredClass (a subclass of it), highest
	 * ImportPriority first; null when none (Leon; UE: the editor's AssetTools).
	 */
	static UClass* FindFactoryClassForFile(const FString& Filename, UClass* PreferredClass = nullptr);

	/**
	 * Sets the factory's UPROPERTYs named by Settings from their text (ImportText). Keys that name no property are
	 * left in OutUnknown (when given) and reported as warnings; the applied pairs are kept in AppliedImportSettings.
	 * False when a value cannot be parsed (Leon).
	 */
	bool ApplyImportSettings(const TMap<FString, FString>& Settings, TArray<FString>* OutUnknown = nullptr);

	/** The settings ApplyImportSettings applied, recorded in the imported asset's UAssetImportData (Leon). */
	TMap<FString, FString> AppliedImportSettings;

protected:
	/**
	 * The asset InName of InClass in InParent: the existing object of that name and class (a reimport over it), else a
	 * new one (UE: CreateOrOverwriteAsset). Null, with an error, when another class already has the name.
	 */
	UObject* CreateOrOverwriteAsset(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags) const;

	template <typename T>
	T* CreateOrOverwriteAsset(UObject* InParent, FName InName, EObjectFlags InFlags) const
	{
		return static_cast<T*>(CreateOrOverwriteAsset(T::StaticClass(), InParent, InName, InFlags));
	}

	/**
	 * The asset's import data, made as its `AssetImportData` subobject when it has none, updated with SourceFile and
	 * the applied settings (Leon: UE's factories call AssetImportData->Update).
	 */
	UAssetImportData* UpdateAssetImportData(UObject* Asset, const FString& SourceFile) const;

	/**
	 * FReimportHandler for the subclasses (each forwards its overrides here): an asset of SupportedClass whose import
	 * data names a file this factory imports can be reimported, by a fresh factory of this class with the recorded
	 * settings.
	 */
	bool FactoryCanReimport(UObject* Obj, TArray<FString>& OutFilenames);
	void FactorySetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths);
	EReimportResult::Type FactoryReimport(UObject* Obj);
	void FactoryGetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const;

public:
	/**
	 * The asset's `AssetImportData` subobject (the UPROPERTY every imported asset class has), made when bCreate and it
	 * has none; null for a class without the property (Leon: UE reads each class's member).
	 */
	static UAssetImportData* GetAssetImportData(UObject* Asset, bool bCreate = false);

private:
	/** The additional objects of the last FactoryReimport. */
	TArray<UObject*> LastReimportAdditionalObjects;
};
