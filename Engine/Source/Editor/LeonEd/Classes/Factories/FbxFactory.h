#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "FbxFactory.generated.h"

class USkeleton;

/** What an FBX file is imported as (UE: EFBXImportType, Factories/FbxImportUI.h). */
UENUM()
enum EFBXImportType
{
	FBXIT_StaticMesh,
	FBXIT_SkeletalMesh,
	FBXIT_Animation,
	FBXIT_MAX,
};

/**
 * Imports FBX files (through ufbx) and OBJ files (tinyobjloader) (UE: UFbxFactory, which reads OBJ through the FBX SDK
 * too), converted to the engine world by MeshUtilities:
 * - FBXIT_StaticMesh (FBX, OBJ): a UStaticMesh; its named material slots get `M_<Name>` material assets next to it
 *   (bImportMaterials), with the maps the source names imported as `T_` textures.
 * - FBXIT_SkeletalMesh (FBX): a USkeletalMesh on Skeleton, or on a new `SKEL_<Name>` skeleton next to it.
 * - FBXIT_Animation (FBX): a UAnimSequence of the file's first animation stack, baked against Skeleton (required).
 * A reimport keeps what it finds in the asset: its type, its skeleton (whose bones must still match) and its slots'
 * materials. Leon has no import UI (UE: UFbxImportUI): the options are this factory's properties.
 */
UCLASS()
class LEONED_API UFbxFactory
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UFbxFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** What to make (UE: ImportUI->MeshTypeToImport). ImportList.ini: `MeshTypeToImport=FBXIT_SkeletalMesh`. */
	UPROPERTY()
	TEnumAsByte<EFBXImportType> MeshTypeToImport = FBXIT_StaticMesh;

	/** The skeleton of a skeletal mesh or an animation (UE: ImportUI->Skeleton). ImportList.ini: an object path. */
	UPROPERTY()
	USkeleton* Skeleton = nullptr;

	/** Makes the static mesh's materials (UE: ImportUI->bImportMaterials). */
	UPROPERTY()
	bool bImportMaterials = true;

	UClass* ResolveSupportedClass() override;
	bool DoesSupportClass(UClass* Class) override;
	UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled) override;

	// FReimportHandler
	bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	EReimportResult::Type Reimport(UObject* Obj) override;
	void GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const override;

private:
	UObject* ImportStaticMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename);
	UObject* ImportSkeletalMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename);
	UObject* ImportAnimation(UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename);
};
