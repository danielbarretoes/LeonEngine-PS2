#include "Factories/StaticMeshImport.h"

#include "AssetImportUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "MeshData.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{

	/** The package of an asset named AssetName in the folder of AssetPackageName. */
	FString SiblingPackageName(const FString& AssetPackageName, const FString& AssetName)
	{
		return FPackageName::GetLongPackagePath(AssetPackageName) + TEXT("/") + AssetName;
	}

	/**
	 * The material asset `M_<Name>` in MaterialPackagePath (a folder), or next to the mesh without one: the existing
	 * one, or a new one made from the source's values.
	 */
	UMaterialInterface* FindOrCreateMaterial(const FString& SlotName, const FMeshData& Data, int32 Slot,
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
			Values.AlbedoMap =
				StaticMeshImport::FindOrImportTexture(Data.AlbedoMapPaths[Slot], PackageName, true, OutNewAssets);
		}
		if (Data.NormalMapPaths.IsValidIndex(Slot) && !Data.NormalMapPaths[Slot].IsEmpty())
		{
			Values.NormalMap =
				StaticMeshImport::FindOrImportTexture(Data.NormalMapPaths[Slot], PackageName, false, OutNewAssets);
		}
		UMaterial* Material =
			NewObject<UMaterial>(CreatePackage(*PackageName), FName(*AssetName), RF_Public | RF_Standalone);
		Material->SetFromRenderProxy(Values);
		OutNewAssets.Add(Material);
		return Material;
	}

} // namespace

void StaticMeshImport::BuildStaticMesh(UStaticMesh& Mesh, const FMeshData& Data, bool bImportMaterials,
	TArray<UObject*>& OutNewAssets, const FString& MaterialPackagePath)
{
	(void)Mesh.BuildFromMeshData(Data);

	int32 NumSlots = FMath::Max(Data.Materials.Num(), 1);
	for (const FMeshSection& Section : Mesh.GetLODResources().Sections)
	{
		NumSlots = FMath::Max(NumSlots, Section.MaterialIndex + 1);
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
			Material = FindOrCreateMaterial(SourceName, Data, Slot, MeshPackageName, MaterialPackagePath, OutNewAssets);
		}
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
}

UTexture2D* StaticMeshImport::FindOrImportTexture(
	const FString& ImageFile, const FString& AssetPackageName, bool bSRGB, TArray<UObject*>& OutNewAssets)
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
	Settings.Add(TEXT("ColorSpaceMode"), bSRGB ? TEXT("SRGB") : TEXT("Linear"));
	(void)Factory->ApplyImportSettings(Settings);
	UTexture2D* Texture = Cast<UTexture2D>(UFactory::StaticImportObject(UTexture2D::StaticClass(),
		CreatePackage(*PackageName), FName(*AssetName), RF_Public | RF_Standalone, ImageFile, Factory));
	if (Texture != nullptr)
	{
		OutNewAssets.Add(Texture);
	}
	return Texture;
}
