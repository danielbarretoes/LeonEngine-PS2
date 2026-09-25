#include "Factories/GLTFImportFactory.h"

#include "Engine/StaticMesh.h"
#include "Factories/StaticMeshImport.h"
#include "LeonEdLog.h"
#include "MeshData.h"
#include "StaticMeshBuilder.h"

UGLTFImportFactory::UGLTFImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStaticMesh::StaticClass();
	Formats.Add(TEXT("gltf;GL Transmission Format"));
	Formats.Add(TEXT("glb;GL Transmission Format (binary)"));
	bEditorImport = 1;
}

UObject* UGLTFImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled)
{
	(void)InClass;
	(void)Parms;
	bOutOperationCanceled = false;
	AdditionalImportedObjects.Reset();
	FMeshData Data;
	FString Error;
	if (!FStaticMeshBuilder::BuildFromGltf(Filename, Data, Error))
	{
		UE_LOG(LogLeonEd, Error, "GLTFImportFactory: %s", *Error);
		return nullptr;
	}
	UStaticMesh* Mesh = CreateOrOverwriteAsset<UStaticMesh>(InParent, InName, Flags);
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	StaticMeshImport::BuildStaticMesh(*Mesh, Data, bImportMaterials, AdditionalImportedObjects);
	UpdateAssetImportData(Mesh, Filename);
	return Mesh;
}

bool UGLTFImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return FactoryCanReimport(Obj, OutFilenames);
}

void UGLTFImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	FactorySetReimportPaths(Obj, NewReimportPaths);
}

EReimportResult::Type UGLTFImportFactory::Reimport(UObject* Obj)
{
	return FactoryReimport(Obj);
}

void UGLTFImportFactory::GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
{
	FactoryGetAdditionalReimportedObjects(OutObjects);
}
