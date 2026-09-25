#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "GLTFImportFactory.generated.h"

/**
 * Imports `.gltf` / `.glb` files as a UStaticMesh (UE: UGLTFImportFactory, the glTF Importer plugin), through cgltf
 * in MeshUtilities: every triangle primitive merged into one mesh, one section and material slot per primitive, glTF's
 * right-handed Y-up metres converted to the engine world. With bImportMaterials each named glTF material becomes an
 * `M_<Name>` UMaterial next to the mesh (base colour, metallic, roughness, opacity) with its external base colour
 * and normal images imported as `T_` textures; embedded images are not read. A glTF scene imported as a map is
 * UGLTFMapFactory's (`-type=Map`); skins are not read.
 */
UCLASS()
class LEONED_API UGLTFImportFactory
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UGLTFImportFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Makes the materials (Leon; UE's glTF importer always does). */
	UPROPERTY()
	bool bImportMaterials = true;

	UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled) override;

	// FReimportHandler
	bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	EReimportResult::Type Reimport(UObject* Obj) override;
	void GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const override;
};
