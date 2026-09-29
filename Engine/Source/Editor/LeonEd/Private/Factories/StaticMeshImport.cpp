#include "Factories/StaticMeshImport.h"

#include "AssetImportUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "LeonEdLog.h"
#include "Materials/Material.h"
#include "MeshData.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/Package.h"

namespace
{

	/** The package of an asset named AssetName in the folder of AssetPackageName. */
	FString SiblingPackageName(const FString& AssetPackageName, const FString& AssetName)
	{
		return FPackageName::GetLongPackagePath(AssetPackageName) + TEXT("/") + AssetName;
	}

} // namespace

UMaterialInterface* StaticMeshImport::FindOrCreateMaterial(const FString& SlotName, const FMeshData& Data, int32 Slot,
	const FString& MeshPackageName, const FString& MaterialPackagePath, TArray<UObject*>& OutNewAssets)
{
	const FString AssetName = FAssetImportUtils::MakeAssetName(UMaterial::StaticClass(), SlotName);
	const FString PackageName = MaterialPackagePath.IsEmpty() ? SiblingPackageName(MeshPackageName, AssetName)
															  : MaterialPackagePath + TEXT("/") + AssetName;
	if (UMaterial* Existing =
			Cast<UMaterial>(FAssetImportUtils::FindOrLoadAsset(UMaterial::StaticClass(), PackageName, AssetName)))
	{
		return Existing;
	}
	FMaterial Values = Data.Materials.IsValidIndex(Slot) ? Data.Materials[Slot] : FMaterial();
	if (Data.AlbedoMapPaths.IsValidIndex(Slot) && !Data.AlbedoMapPaths[Slot].IsEmpty())
	{
		Values.AlbedoMap = FindOrImportTexture(Data.AlbedoMapPaths[Slot], PackageName, OutNewAssets);
	}
	else if (Data.AlbedoMapImages.IsValidIndex(Slot) && Data.AlbedoMapImages[Slot].EncodedData.Num() > 0)
	{
		Values.AlbedoMap = FindOrCreateEmbeddedTexture(Data.AlbedoMapImages[Slot], PackageName, OutNewAssets);
	}
	UMaterial* Material =
		NewObject<UMaterial>(CreatePackage(*PackageName), FName(*AssetName), RF_Public | RF_Standalone);
	Material->SetFromRenderProxy(Values);
	OutNewAssets.Add(Material);
	return Material;
}

void StaticMeshImport::ApplyPhysicalMaterial(UMaterialInterface* Material, const FString& SlotName,
	const FMeshData& Data, int32 Slot, TArray<UObject*>& OutChanged)
{
	UMaterial* Imported = Cast<UMaterial>(Material);
	const FString Path = Data.PhysicalMaterialNames.IsValidIndex(Slot) ? Data.PhysicalMaterialNames[Slot] : FString();
	if (Imported == nullptr || Path.IsEmpty() ||
		Imported->GetName() != FAssetImportUtils::MakeAssetName(UMaterial::StaticClass(), SlotName))
	{
		return;
	}
	// A long package name (/Game/PhysicalMaterials/PM_Wood) or an object path (...PM_Wood.PM_Wood).
	const FString ObjectPath = Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
	UPhysicalMaterial* PhysMaterial = FindObject<UPhysicalMaterial>(nullptr, *ObjectPath);
	if (PhysMaterial == nullptr && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(ObjectPath)))
	{
		PhysMaterial = LoadObject<UPhysicalMaterial>(nullptr, *ObjectPath);
	}
	if (PhysMaterial == nullptr)
	{
		UE_LOG(LogLeonEd, Error, "The material %s names the physical material '%s', which does not exist",
			*Imported->GetName(), *Path);
		return;
	}
	if (Imported->PhysMaterial != PhysMaterial)
	{
		Imported->PhysMaterial = PhysMaterial;
		OutChanged.AddUnique(Imported);
	}
}

bool StaticMeshImport::BuildStaticMesh(UStaticMesh& Mesh, const FMeshData& Data, bool bImportMaterials,
	TArray<UObject*>& OutNewAssets, const FString& MaterialPackagePath)
{
	// UStaticMesh logs why a build fails, with its name; nothing further is made of a mesh without render data.
	if (!Mesh.BuildFromMeshData(Data))
	{
		return false;
	}

	int32 NumSlots = FMath::Max(Data.Materials.Num(), 1);
	const FStaticMeshLODResources& Resources = Mesh.GetLODResources();
	for (int32 Section = 0; Section < Resources.GetNumSections(); ++Section)
	{
		NumSlots = FMath::Max(NumSlots, Resources.GetSectionMaterialIndex(Section) + 1);
	}
	const TArray<FStaticMaterial> OldSlots = Mesh.StaticMaterials;
	Mesh.StaticMaterials.Reset();
	const FString MeshPackageName = Mesh.GetOutermost()->GetName();
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		const FString SourceName = Data.MaterialSlotNames.IsValidIndex(Slot) ? Data.MaterialSlotNames[Slot] : FString();
		const FName SlotName =
			SourceName.IsEmpty() ? FName(NAME_None) : FName(*FAssetImportUtils::SanitizeName(SourceName));
		UMaterialInterface* Material = nullptr;
		// A reimport keeps the material of a slot whose name did not change (an unnamed slot: by its index).
		for (int32 OldIndex = 0; OldIndex < OldSlots.Num(); ++OldIndex)
		{
			const FStaticMaterial& Old = OldSlots[OldIndex];
			if (Old.MaterialSlotName == SlotName && (SlotName != NAME_None || OldIndex == Slot))
			{
				Material = Old.MaterialInterface;
				break;
			}
		}
		if (Material == nullptr && bImportMaterials && SlotName != NAME_None)
		{
			Material = StaticMeshImport::FindOrCreateMaterial(
				SourceName, Data, Slot, MeshPackageName, MaterialPackagePath, OutNewAssets);
		}
		StaticMeshImport::ApplyPhysicalMaterial(Material, SourceName, Data, Slot, OutNewAssets);
		Mesh.StaticMaterials.Add(FStaticMaterial(Material, SlotName));
	}

	// The source's sockets, each an inner object named after it (a reimport reuses the objects by name and drops the
	// sockets the source no longer has; UE keeps sockets added in the editor, which Leon has not).
	const TArray<UStaticMeshSocket*> OldSockets = Mesh.Sockets;
	Mesh.Sockets.Reset();
	for (const FMeshSocketData& Source : Data.Sockets)
	{
		const FName SocketName(*FAssetImportUtils::SanitizeName(Source.Name));
		const FName ObjectName(*(FString(TEXT("StaticMeshSocket_")) + SocketName.ToString()));
		UStaticMeshSocket* Socket = nullptr;
		for (UStaticMeshSocket* Old : OldSockets)
		{
			if (Old != nullptr && Old->GetFName() == ObjectName)
			{
				Socket = Old;
			}
		}
		if (Socket == nullptr)
		{
			Socket = NewObject<UStaticMeshSocket>(&Mesh, ObjectName);
		}
		Socket->SocketName = SocketName;
		Socket->RelativeLocation = Source.Transform.GetLocation();
		Socket->RelativeRotation = Source.Transform.GetRotation().Rotator();
		Socket->RelativeScale = Source.Transform.GetScale3D();
		Mesh.Sockets.Add(Socket);
	}
	for (UStaticMeshSocket* Old : OldSockets)
	{
		if (Old != nullptr && !Mesh.Sockets.Contains(Old))
		{
			Old->MarkPendingKill();
		}
	}
	return true;
}

UTexture2D* StaticMeshImport::FindOrImportTexture(
	const FString& ImageFile, const FString& AssetPackageName, TArray<UObject*>& OutNewAssets)
{
	const FString AssetName =
		FAssetImportUtils::MakeAssetName(UTexture2D::StaticClass(), FPaths::GetBaseFilename(ImageFile));
	const FString PackageName = SiblingPackageName(AssetPackageName, AssetName);
	if (UTexture2D* Existing =
			Cast<UTexture2D>(FAssetImportUtils::FindOrLoadAsset(UTexture2D::StaticClass(), PackageName, AssetName)))
	{
		return Existing;
	}
	UTextureFactory* Factory = NewObject<UTextureFactory>(GetTransientPackage());
	TMap<FString, FString> Settings;
	Settings.Add(TEXT("ColorSpaceMode"), TEXT("SRGB"));
	(void)Factory->ApplyImportSettings(Settings);
	UTexture2D* Texture = Cast<UTexture2D>(UFactory::StaticImportObject(UTexture2D::StaticClass(),
		CreatePackage(*PackageName), FName(*AssetName), RF_Public | RF_Standalone, ImageFile, Factory));
	if (Texture != nullptr)
	{
		OutNewAssets.Add(Texture);
	}
	return Texture;
}

UTexture2D* StaticMeshImport::FindOrCreateEmbeddedTexture(
	const FMeshEmbeddedImage& Image, const FString& AssetPackageName, TArray<UObject*>& OutNewAssets)
{
	const FString AssetName = FAssetImportUtils::MakeAssetName(UTexture2D::StaticClass(), Image.Name);
	const FString PackageName = SiblingPackageName(AssetPackageName, AssetName);
	if (UTexture2D* Existing =
			Cast<UTexture2D>(FAssetImportUtils::FindOrLoadAsset(UTexture2D::StaticClass(), PackageName, AssetName)))
	{
		return Existing;
	}
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> Texels;
	FString Error;
	if (!UTextureFactory::DecodeImage(
			Image.EncodedData.GetData(), Image.EncodedData.Num(), Width, Height, Texels, Error))
	{
		UE_LOG(LogLeonEd, Error, "The embedded image '%s' cannot be decoded (%s)", *Image.Name, *Error);
		return nullptr;
	}
	UTexture2D* Texture =
		NewObject<UTexture2D>(CreatePackage(*PackageName), FName(*AssetName), RF_Public | RF_Standalone);
	Texture->SRGB = 1;
	(void)Texture->SetPlatformData(Width, Height, PF_R8G8B8A8, Texels.GetData());
	OutNewAssets.Add(Texture);
	return Texture;
}
