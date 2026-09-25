#include "Commandlets/MigrateLegacyContentCommandlet.h"

#include "AssetImportUtils.h"
#include "Camera/CameraActor.h"
#include "Commandlets/ImportAssetsCommandlet.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "LeonEdLog.h"
#include "Level/LegacyAssetKeys.h"
#include "Level/LevelLoader.h"
#include "MeshData.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Primitives.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

namespace
{

	/** The legacy files a migration converts, in the order it converts them (maps before the materials using them). */
	enum class ELegacyFileKind : uint8
	{
		Image,
		Sound,
		Mesh,
		Material,
		Level,
		Other,
	};

	ELegacyFileKind GetLegacyFileKind(const FString& Filename)
	{
		const FString Extension = FPaths::GetExtension(Filename);
		if (Extension == TEXT("png") || Extension == TEXT("jpg") || Extension == TEXT("jpeg") ||
			Extension == TEXT("tga") || Extension == TEXT("bmp"))
		{
			return ELegacyFileKind::Image;
		}
		if (Extension == TEXT("wav"))
		{
			return ELegacyFileKind::Sound;
		}
		if (Extension == TEXT("lmesh"))
		{
			return ELegacyFileKind::Mesh;
		}
		if (Extension == TEXT("lmat"))
		{
			return ELegacyFileKind::Material;
		}
		if (Extension == TEXT("llev"))
		{
			return ELegacyFileKind::Level;
		}
		return ELegacyFileKind::Other;
	}

	// The procedural engine assets, as the runtime generated them before they were packaged (saved once by -engine).

	/** The sizes of the procedural textures: the `.lmat` `checker` and `bump` maps. */
	constexpr int32 DefaultTextureSize = 64;
	constexpr int32 BumpNormalTextureSize = 256;

	/** The default UV sphere of the basic shapes. */
	constexpr int32 DefaultSphereSegments = 24;
	constexpr int32 DefaultSphereRings = 16;

	int32 PixelIndex(int32 X, int32 Y, int32 Size)
	{
		return (Y * Size) + X;
	}

	/** A grey checker with 8 cells per side (UE: DefaultTexture). */
	void MakeCheckerPixels(int32 Size, TArray<uint8>& OutPixels)
	{
		OutPixels.SetNumUninitialized(Size * Size * 4);
		const int32 Cell = FMath::Max(1, Size / 8);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const bool bDark = ((X / Cell) + (Y / Cell)) % 2 == 0;
				const uint8 C = bDark ? static_cast<uint8>(60) : static_cast<uint8>(220);
				const int32 I = PixelIndex(X, Y, Size) * 4;
				OutPixels[I + 0] = C;
				OutPixels[I + 1] = C;
				OutPixels[I + 2] = C;
				OutPixels[I + 3] = 255;
			}
		}
	}

	/** Strong procedural bumps for demo normal mapping (tileable). */
	void MakeBumpNormalPixels(int32 Size, TArray<uint8>& OutPixels)
	{
		// Height field to a finite-difference normal map (tileable, intentionally strong).
		TArray<float> Height;
		Height.SetNumUninitialized(Size * Size);
		constexpr float TwoPi = 6.28318530718f;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const float U = static_cast<float>(X) / static_cast<float>(Size);
				const float V = static_cast<float>(Y) / static_cast<float>(Size);
				// Dense ripples + circular dimples so lighting / reflections clearly warp.
				const float Ripples = (0.55f * FMath::Sin(U * TwoPi * 8.0f) * FMath::Cos(V * TwoPi * 6.0f)) +
					(0.30f * FMath::Sin((U + V) * TwoPi * 10.0f));
				const float Cx = FMath::Fmod(U * 4.0f, 1.0f) - 0.5f;
				const float Cy = FMath::Fmod(V * 4.0f, 1.0f) - 0.5f;
				const float Dimple = 0.45f * FMath::Exp(-18.0f * ((Cx * Cx) + (Cy * Cy)));
				Height[PixelIndex(X, Y, Size)] = Ripples + Dimple;
			}
		}

		OutPixels.SetNumUninitialized(Size * Size * 4);
		const float Strength = 6.0f;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const int32 X0 = (X + Size - 1) % Size;
				const int32 X1 = (X + 1) % Size;
				const int32 Y0 = (Y + Size - 1) % Size;
				const int32 Y1 = (Y + 1) % Size;
				const float HL = Height[PixelIndex(X0, Y, Size)];
				const float HR = Height[PixelIndex(X1, Y, Size)];
				const float HD = Height[PixelIndex(X, Y0, Size)];
				const float HU = Height[PixelIndex(X, Y1, Size)];
				float Nx = (HL - HR) * Strength;
				float Ny = (HD - HU) * Strength;
				float Nz = 1.0f;
				const float InvLen = 1.0f / FMath::Sqrt(((Nx * Nx) + (Ny * Ny)) + (Nz * Nz));
				Nx *= InvLen;
				Ny *= InvLen;
				Nz *= InvLen;

				const int32 I = PixelIndex(X, Y, Size) * 4;
				OutPixels[I + 0] = static_cast<uint8>(((Nx * 0.5f) + 0.5f) * 255.0f);
				OutPixels[I + 1] = static_cast<uint8>(((Ny * 0.5f) + 0.5f) * 255.0f);
				OutPixels[I + 2] = static_cast<uint8>(((Nz * 0.5f) + 0.5f) * 255.0f);
				OutPixels[I + 3] = 255;
			}
		}
	}

	/** The asset PackageName (named after it) of Class: the one in memory or on disk, else a new one. */
	template <typename T>
	T* FindOrCreateEngineAsset(const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetShortName(PackageName);
		if (T* Existing = Cast<T>(FAssetImportUtils::FindOrLoadAsset(T::StaticClass(), PackageName, AssetName)))
		{
			return Existing;
		}
		return NewObject<T>(CreatePackage(*PackageName), FName(*AssetName), RF_Public | RF_Standalone);
	}

	bool SaveProceduralTexture(
		const FString& PackageName, int32 Size, bool bSRGB, void (*MakePixels)(int32, TArray<uint8>&))
	{
		TArray<uint8> Pixels;
		MakePixels(Size, Pixels);
		UTexture2D* Texture = FindOrCreateEngineAsset<UTexture2D>(PackageName);
		Texture->SRGB = bSRGB ? 1 : 0;
		(void)Texture->SetPlatformData(Size, Size, PF_R8G8B8A8, Pixels.GetData());
		return FAssetImportUtils::SavePackage(Texture->GetOutermost(), Texture);
	}

	bool SaveProceduralMesh(const FString& PackageName, const FMeshData& Data)
	{
		// The basic shapes have no material slots: a component's material 0 draws them.
		UStaticMesh* Mesh = FindOrCreateEngineAsset<UStaticMesh>(PackageName);
		Mesh->StaticMaterials.Reset();
		(void)Mesh->BuildFromMeshData(Data);
		return FAssetImportUtils::SavePackage(Mesh->GetOutermost(), Mesh);
	}

	/** A transient mesh's geometry as mesh data (its sections, no material slots). */
	FMeshData GetMeshData(const UStaticMesh& Mesh)
	{
		FMeshData Data;
		Data.Vertices = Mesh.GetLODResources().Vertices;
		Data.Indices = Mesh.GetLODResources().Indices;
		Data.Submeshes = Mesh.GetLODResources().Sections;
		return Data;
	}

	/**
	 * Saves every mesh the level's components show that the reader built at run time (transient) as `SM_<Mesh>` in
	 * `<Map>/Meshes`, and gives it to the components instead; false when one does not save.
	 */
	bool SaveTransientMeshes(UWorld& World, const FString& MapPackageName)
	{
		TMap<UStaticMesh*, UStaticMesh*> Saved;
		for (AActor* Actor : World.PersistentLevel->Actors)
		{
			AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
			UStaticMeshComponent* Component = MeshActor != nullptr ? MeshActor->GetStaticMeshComponent() : nullptr;
			UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
			if (Mesh == nullptr || !Mesh->GetOutermost()->HasAnyFlags(RF_Transient))
			{
				continue;
			}
			UStaticMesh** Copy = Saved.Find(Mesh);
			if (Copy == nullptr)
			{
				const FString AssetName = FAssetImportUtils::MakeAssetName(UStaticMesh::StaticClass(), Mesh->GetName());
				const FString PackageName = MapPackageName + TEXT("/Meshes/") + AssetName;
				UStaticMesh* NewMesh = FindOrCreateEngineAsset<UStaticMesh>(PackageName);
				NewMesh->StaticMaterials.Reset();
				(void)NewMesh->BuildFromMeshData(GetMeshData(*Mesh));
				if (!FAssetImportUtils::SavePackage(NewMesh->GetOutermost(), NewMesh))
				{
					return false;
				}
				Copy = &Saved.Add(Mesh, NewMesh);
			}
			(void)Component->SetStaticMesh(*Copy);
		}
		return true;
	}

} // namespace

UMigrateLegacyContentCommandlet::UMigrateLegacyContentCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HelpDescription =
		TEXT("Converts legacy .lmat / .lmesh / image / .wav content into .lasset packages and .llev levels into .lmap "
			 "maps (temporary)");
	HelpUsage =
		TEXT("-run=MigrateLegacyContent [-source=<ContentDir>] [-level=<File.llev> -dest=<MapPackage>] [-engine]");
	LogToConsole = 1;
}

UWorld* UMigrateLegacyContentCommandlet::MigrateLevel(const FString& LevelFile, const FString& MapPackageName)
{
	if (!FPackageName::IsValidLongPackageName(MapPackageName))
	{
		UE_LOG(LogLeonEd, Error, "MigrateLegacyContent: '%s' is not a package under a mount point", *MapPackageName);
		return nullptr;
	}
	// Built and saved, never drawn: no renderer's scene.
	const UWorld::InitializationValues IVS = UWorld::InitializationValues().InitializeScenes(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(*FPackageName::GetShortName(MapPackageName)),
		CreatePackage(*MapPackageName), /*bAddToRoot =*/true, &IVS);
	const auto Discard = [World]()
	{
		World->DestroyWorld(false);
		World->GetOutermost()->MarkPendingKill();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	};
	if (!LoadLevelFile(*World, FPaths::ConvertRelativePathToFull(LevelFile)))
	{
		Discard();
		return nullptr;
	}
	// The view the level opened with becomes its first player start (the camera actor keeps the framing itself).
	if (const ACameraActor* Framing = World->FindFirst<ACameraActor>())
	{
		FVector Location;
		FRotator Rotation;
		GetLegacyPlayFromHereView(*Framing->GetCameraComponent(), Location, Rotation);
		APlayerStart* Start = World->SpawnActor<APlayerStart>(Location, Rotation);
		TArray<AActor*>& Actors = World->PersistentLevel->Actors;
		const int32 FirstStart = Actors.IndexOfByPredicate(
			[Start](const AActor* Actor) { return Actor != Start && Actor != nullptr && Actor->IsA<APlayerStart>(); });
		if (FirstStart != INDEX_NONE)
		{
			Actors.Remove(Start);
			Actors.Insert(Start, FirstStart);
		}
	}
	if (!SaveTransientMeshes(*World, MapPackageName) || !FAssetImportUtils::SavePackage(World->GetOutermost(), World))
	{
		Discard();
		return nullptr;
	}
	UE_LOG(LogLeonEd, Display, "MigrateLegacyContent: '%s' is the map %s (%d actors)", *LevelFile, *MapPackageName,
		World->PersistentLevel->Actors.Num());
	World->RemoveFromRoot();
	return World;
}

int32 UMigrateLegacyContentCommandlet::SaveEngineProceduralAssets()
{
	int32 Failures = 0;
	Failures += SaveProceduralTexture(
					TEXT("/Engine/EngineResources/DefaultTexture"), DefaultTextureSize, true, &MakeCheckerPixels)
		? 0
		: 1;
	Failures += SaveProceduralTexture(TEXT("/Engine/EngineMaterials/T_Default_Bump_N"), BumpNormalTextureSize, false,
					&MakeBumpNormalPixels)
		? 0
		: 1;
	Failures += SaveProceduralMesh(TEXT("/Engine/BasicShapes/Cube"), MakeCube()) ? 0 : 1;
	// 100 cm with 0-1 UVs; the tiling is the material's UVScale.
	Failures += SaveProceduralMesh(TEXT("/Engine/BasicShapes/Plane"), MakePlane(PrimitiveEdgeLength, 1.0f)) ? 0 : 1;
	Failures +=
		SaveProceduralMesh(TEXT("/Engine/BasicShapes/Sphere"), MakeSphere(DefaultSphereSegments, DefaultSphereRings))
		? 0
		: 1;
	return Failures;
}

int32 UMigrateLegacyContentCommandlet::MigrateDirectory(const FString& ContentDir)
{
	FString Directory = FPaths::ConvertRelativePathToFull(ContentDir);
	FPaths::NormalizeFilename(Directory);
	Directory.RemoveFromEnd(TEXT("/"));
	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		UE_LOG(LogLeonEd, Error, "MigrateLegacyContent: '%s' is not a folder", *Directory);
		return 1;
	}
	const FString Root = FLegacyAssetKeys::MountContentDirectory(Directory);
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*"), true, false);
	Files.Sort();

	int32 Failures = 0;
	int32 Converted = 0;
	for (const ELegacyFileKind Kind : {ELegacyFileKind::Image, ELegacyFileKind::Sound, ELegacyFileKind::Mesh,
			 ELegacyFileKind::Material, ELegacyFileKind::Level})
	{
		for (const FString& File : Files)
		{
			if (GetLegacyFileKind(File) != Kind)
			{
				continue;
			}
			FString Key = File;
			FPaths::NormalizeFilename(Key);
			Key = Key.RightChop(Directory.Len() + 1);
			if (Kind == ELegacyFileKind::Level)
			{
				// <Root>/Levels/X.llev is the map <Root>/Maps/X (UE's content layout).
				if (MigrateLevel(File, Root + TEXT("/Maps/") + FPaths::GetBaseFilename(File)) == nullptr)
				{
					++Failures;
					continue;
				}
				++Converted;
				continue;
			}
			const FString PackageName = FLegacyAssetKeys::GetMigratedPackageName(Root, Key);
			if (PackageName.IsEmpty())
			{
				UE_LOG(LogLeonEd, Error, "MigrateLegacyContent: no package name for '%s'", *File);
				++Failures;
				continue;
			}
			TMap<FString, FString> Settings;
			FString Type;
			if (Kind == ELegacyFileKind::Material)
			{
				// The `.lmat` map keys are relative to the migrated folder.
				Settings.Add(TEXT("ContentRootPath"), Root);
			}
			else if (Kind == ELegacyFileKind::Image)
			{
				Type = TEXT("Texture");
			}
			else if (Kind == ELegacyFileKind::Sound)
			{
				Type = TEXT("Sound");
			}
			UObject* Asset = UImportAssetsCommandlet::ImportAsset(File, FPackageName::GetLongPackagePath(PackageName),
				FPackageName::GetShortName(PackageName), Type, Settings);
			if (Asset == nullptr)
			{
				++Failures;
				continue;
			}
			++Converted;
		}
	}
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	UE_LOG(LogLeonEd, Display, "MigrateLegacyContent: %d files of '%s' converted under %s, %d failed", Converted,
		*Directory, *Root, Failures);
	return Failures;
}

int32 UMigrateLegacyContentCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamsMap;
	ParseCommandLine(*Params, Tokens, Switches, ParamsMap);
	const bool bEngine = Switches.Contains(TEXT("engine"));
	const FString* Source = ParamsMap.Find(TEXT("source"));
	const FString* Level = ParamsMap.Find(TEXT("level"));
	const FString* Dest = ParamsMap.Find(TEXT("dest"));
	if ((!bEngine && Source == nullptr && Level == nullptr) || ((Level == nullptr) != (Dest == nullptr)))
	{
		UE_LOG(LogLeonEd, Error, "MigrateLegacyContent: usage: %s", *HelpUsage);
		return 1;
	}
	int32 Failures = 0;
	if (bEngine)
	{
		Failures += SaveEngineProceduralAssets();
	}
	if (Source != nullptr)
	{
		Failures += MigrateDirectory(*Source);
	}
	if (Level != nullptr && MigrateLevel(*Level, *Dest) == nullptr)
	{
		++Failures;
	}
	return Failures == 0 ? 0 : 1;
}
