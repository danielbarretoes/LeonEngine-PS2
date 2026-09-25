#include "LegacyAssetLoader.h"

#include "Containers/StringConv.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EngineLogs.h"
#include "LeonMaterialFormat.h"
#include "LeonMeshFormat.h"
#include "Materials/Material.h"
#include "MeshData.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Primitives.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

const TCHAR* const FLegacyAssetLoader::LegacyPackageRoot = TEXT("/Temp/LegacyAssets");

namespace
{

	/** The flags of a legacy file's asset: an asset that is never saved. */
	constexpr EObjectFlags LegacyAssetFlags = RF_Public | RF_Standalone | RF_Transient;

	/** The flags of an engine asset made in its final package. */
	constexpr EObjectFlags EngineAssetFlags = RF_Public | RF_Standalone;

	/** The sizes of the procedural textures: the `.lmat` `checker` and `bump` maps. */
	constexpr int32 DefaultTextureSize = 64;
	constexpr int32 BumpNormalTextureSize = 256;

	/** The default UV sphere of the basic shapes and the `.llev` spheres. */
	constexpr int32 DefaultSphereSegments = 24;
	constexpr int32 DefaultSphereRings = 16;

	FString NormalizeFullPath(const FString& Path)
	{
		FString Full = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Full);
		FPaths::CollapseRelativeDirectories(Full);
		return Full;
	}

	/** Every character that cannot be in a package or object name becomes '_' ('/' kept when bKeepSlashes). */
	FString SanitizeName(const FString& In, bool bKeepSlashes)
	{
		FString Out;
		Out.Reserve(In.Len());
		for (int32 Index = 0; Index < In.Len(); ++Index)
		{
			const TCHAR Char = In[Index];
			const bool bKeep = FChar::IsAlnum(Char) || Char == '_' || (bKeepSlashes && Char == '/');
			Out.AppendChar(bKeep ? Char : '_');
		}
		return Out;
	}

	/** The object name of a legacy file's asset: its base name. */
	FString GetLegacyObjectName(const FString& Filename)
	{
		return SanitizeName(FPaths::GetBaseFilename(Filename), false);
	}

	/** An object of the legacy file's package that is still alive, or null. */
	template <typename T>
	T* FindLegacyAsset(const FString& Filename)
	{
		const FString ObjectPath =
			FLegacyAssetLoader::GetLegacyPackageName(Filename) + TEXT(".") + GetLegacyObjectName(Filename);
		T* Found = FindObject<T>(nullptr, *ObjectPath);
		return Found != nullptr && !Found->IsPendingKill() ? Found : nullptr;
	}

	/**
	 * A new object named ObjectName in the transient package PackageName (a pending-kill object of that name still
	 * waiting for the garbage collector makes it take a unique name instead).
	 */
	template <typename T>
	T* NewTransientAsset(const FString& PackageName, const FString& ObjectName)
	{
		UPackage* Package = CreatePackage(*PackageName);
		Package->SetFlags(RF_Transient);
		FName Name(*ObjectName);
		if (FindObjectFast<UObject>(Package, Name) != nullptr)
		{
			Name = MakeUniqueObjectName(Package, T::StaticClass(), Name);
		}
		return NewObject<T>(Package, Name, LegacyAssetFlags);
	}

	/** A new object in a legacy file's transient package. */
	template <typename T>
	T* NewLegacyAsset(const FString& Filename)
	{
		return NewTransientAsset<T>(FLegacyAssetLoader::GetLegacyPackageName(Filename), GetLegacyObjectName(Filename));
	}

	/** An engine object in its final package, added to the root set (made once). */
	template <typename T>
	T* NewEngineAsset(const FString& PackageName, const FString& AssetName)
	{
		T* Asset = NewObject<T>(CreatePackage(*PackageName), FName(*AssetName), EngineAssetFlags);
		Asset->AddToRoot();
		return Asset;
	}

	/** The engine object (or its class default object before GEngine exists) whose config names the defaults. */
	const UEngine& GetEngineConfig()
	{
		return GEngine != nullptr ? *GEngine : *GetDefault<UEngine>();
	}

	int32 PixelIndex(int32 X, int32 Y, int32 Size)
	{
		return (Y * Size) + X;
	}

	/** A grey checker with 8 cells per side (the default material's map). */
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

	/** Decodes an image file (PNG, JPEG, TGA, ... through stb_image), flipped so the bottom row comes first. */
	bool LoadImagePixels(const FString& Filename, int32& OutWidth, int32& OutHeight, TArray<uint8>& OutPixels)
	{
		stbi_set_flip_vertically_on_load(1);
		int32 Channels = 0;
		uint8* Data = stbi_load(TCHAR_TO_UTF8(*Filename), &OutWidth, &OutHeight, &Channels, 4);
		if (Data == nullptr)
		{
			UE_LOG(
				LogEngine, Error, "LegacyAssetLoader: cannot load texture '%s' (%s)", *Filename, stbi_failure_reason());
			return false;
		}
		OutPixels.Reset();
		OutPixels.Append(Data, OutWidth * OutHeight * 4);
		stbi_image_free(Data);
		return OutWidth > 0 && OutHeight > 0;
	}

	/** The engine's `.lmat` `checker` map (DefaultTextureName). */
	UTexture2D* GetCheckerTexture()
	{
		return FLegacyAssetLoader::LoadEngineObject<UTexture2D>(GetEngineConfig().DefaultTextureName);
	}

	/** The engine's `.lmat` `bump` map (DefaultBumpNormalTextureName). */
	UTexture2D* GetBumpNormalTexture()
	{
		return FLegacyAssetLoader::LoadEngineObject<UTexture2D>(GetEngineConfig().DefaultBumpNormalTextureName);
	}

	/** Reads a `.lmat` into Material and loads its maps; false (Material unchanged) when the file cannot be read. */
	bool ReadLegacyMaterial(const FString& Filename, UMaterial& Material)
	{
		if (!IsLeonMaterialPath(Filename))
		{
			UE_LOG(LogEngine, Error, "LegacyAssetLoader: expected a .lmat, got '%s'", *Filename);
			return false;
		}
		FLeonMaterialDocument Doc;
		if (!LoadLeonMaterialDocument(Filename, Doc))
		{
			return false;
		}
		// "checker" and "bump" are the engine's procedural maps; any other value is a content-relative path.
		if (!Doc.BaseColorMapPath.IsEmpty())
		{
			Doc.Material.AlbedoMap = Doc.BaseColorMapPath.Equals("checker", ESearchCase::CaseSensitive)
				? GetCheckerTexture()
				: FLegacyAssetLoader::LoadTexture(FPaths::ResolveLegacyContentPath(Doc.BaseColorMapPath));
		}
		if (!Doc.NormalMapPath.IsEmpty())
		{
			Doc.Material.NormalMap = Doc.NormalMapPath.Equals("bump", ESearchCase::CaseSensitive)
				? GetBumpNormalTexture()
				: FLegacyAssetLoader::LoadTexture(FPaths::ResolveLegacyContentPath(Doc.NormalMapPath));
		}
		Material.SetFromRenderProxy(Doc.Material);
		return true;
	}

	/** The default material without its `.lmat`: white Blinn-Phong over the grey checker (the legacy fallback). */
	void MakeCheckerMaterial(UMaterial& Material)
	{
		FMaterial Values;
		Values.Shading = EMaterialLightingModel::BlinnPhong;
		Values.Albedo = FVector(1.0f, 1.0f, 1.0f);
		Values.Specular = FVector(0.04f, 0.04f, 0.04f);
		Values.Metallic = 0.0f;
		Values.Shininess = 8.0f;
		Values.SyncRoughnessFromShininess();
		Values.bCastsShadows = true;
		Values.bPlanarMirror = false;
		Values.AlbedoMap = GetCheckerTexture();
		Material.SetFromRenderProxy(Values);
	}

	/** Builds a procedural mesh and gives it a body setup; the basic shapes have no material slots of their own. */
	UStaticMesh* BuildProceduralMesh(UStaticMesh* Mesh, const FMeshData& Data)
	{
		(void)Mesh->BuildFromMeshData(Data);
		return Mesh;
	}

	/** Reads a PCM16 `.wav` (RIFF / WAVE, PCM or extensible PCM); false with an error for anything else. */
	bool ReadWave(const FString& Filename, TArray<int16>& OutSamples, int32& OutChannels, int32& OutSampleRate)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
		{
			UE_LOG(LogEngine, Error, "LegacyAssetLoader: cannot open '%s'", *Filename);
			return false;
		}
		auto ReadU32 = [&Bytes](int32 Offset)
		{
			return static_cast<uint32>(Bytes[Offset]) | (static_cast<uint32>(Bytes[Offset + 1]) << 8) |
				(static_cast<uint32>(Bytes[Offset + 2]) << 16) | (static_cast<uint32>(Bytes[Offset + 3]) << 24);
		};
		auto ReadU16 = [&Bytes](int32 Offset)
		{ return static_cast<uint16>(Bytes[Offset] | (static_cast<uint16>(Bytes[Offset + 1]) << 8)); };
		auto HasTag = [&Bytes](int32 Offset, const char* Tag)
		{ return FMemory::Memcmp(Bytes.GetData() + Offset, Tag, 4) == 0; };

		if (Bytes.Num() < 12 || !HasTag(0, "RIFF") || !HasTag(8, "WAVE"))
		{
			UE_LOG(LogEngine, Error, "LegacyAssetLoader: '%s' is not a RIFF / WAVE file", *Filename);
			return false;
		}
		int32 Format = 0;
		int32 BitsPerSample = 0;
		int32 DataOffset = INDEX_NONE;
		int32 DataSize = 0;
		for (int32 Offset = 12; Offset + 8 <= Bytes.Num();)
		{
			const int32 ChunkSize = static_cast<int32>(ReadU32(Offset + 4));
			const int32 ChunkData = Offset + 8;
			if (ChunkSize < 0 || ChunkData + ChunkSize > Bytes.Num())
			{
				break;
			}
			if (HasTag(Offset, "fmt ") && ChunkSize >= 16)
			{
				Format = ReadU16(ChunkData);
				OutChannels = ReadU16(ChunkData + 2);
				OutSampleRate = static_cast<int32>(ReadU32(ChunkData + 4));
				BitsPerSample = ReadU16(ChunkData + 14);
				// WAVE_FORMAT_EXTENSIBLE: the real format is the sub-format GUID's first two bytes.
				if (Format == 0xFFFE && ChunkSize >= 26)
				{
					Format = ReadU16(ChunkData + 24);
				}
			}
			else if (HasTag(Offset, "data"))
			{
				DataOffset = ChunkData;
				DataSize = ChunkSize;
			}
			// Chunks are padded to an even size.
			Offset = ChunkData + ChunkSize + (ChunkSize & 1);
		}
		if (Format != 1 || BitsPerSample != 16 || OutChannels <= 0 || OutSampleRate <= 0 || DataOffset == INDEX_NONE)
		{
			UE_LOG(LogEngine, Error, "LegacyAssetLoader: '%s' is not 16-bit PCM (format %d, %d bits)", *Filename,
				Format, BitsPerSample);
			return false;
		}
		const int32 NumSamples = DataSize / 2;
		OutSamples.SetNumUninitialized(NumSamples);
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			OutSamples[Index] = static_cast<int16>(ReadU16(DataOffset + (Index * 2)));
		}
		return true;
	}

	/** Makes the engine object PackageName names, from the table in LegacyAssetLoader.h; null when none. */
	UObject* MakeEngineObject(const FString& PackageName, const FString& AssetName)
	{
		const FString ShortName = FPackageName::GetShortName(PackageName);
		if (AssetName != ShortName)
		{
			return nullptr;
		}
		if (PackageName == TEXT("/Engine/EngineResources/DefaultTexture"))
		{
			TArray<uint8> Pixels;
			MakeCheckerPixels(DefaultTextureSize, Pixels);
			UTexture2D* Texture = NewEngineAsset<UTexture2D>(PackageName, AssetName);
			(void)Texture->SetPlatformData(DefaultTextureSize, DefaultTextureSize, PF_R8G8B8A8, Pixels.GetData());
			return Texture;
		}
		if (PackageName == TEXT("/Engine/EngineMaterials/T_Default_Bump_N"))
		{
			TArray<uint8> Pixels;
			MakeBumpNormalPixels(BumpNormalTextureSize, Pixels);
			UTexture2D* Texture = NewEngineAsset<UTexture2D>(PackageName, AssetName);
			Texture->SRGB = 0;
			(void)Texture->SetPlatformData(BumpNormalTextureSize, BumpNormalTextureSize, PF_R8G8B8A8, Pixels.GetData());
			return Texture;
		}
		if (PackageName == TEXT("/Engine/BasicShapes/Cube"))
		{
			return BuildProceduralMesh(NewEngineAsset<UStaticMesh>(PackageName, AssetName), MakeCube());
		}
		if (PackageName == TEXT("/Engine/BasicShapes/Plane"))
		{
			// 100 cm with 0-1 UVs; the tiling is the material's UVScale.
			return BuildProceduralMesh(
				NewEngineAsset<UStaticMesh>(PackageName, AssetName), MakePlane(PrimitiveEdgeLength, 1.0f));
		}
		if (PackageName == TEXT("/Engine/BasicShapes/Sphere"))
		{
			return BuildProceduralMesh(NewEngineAsset<UStaticMesh>(PackageName, AssetName),
				MakeSphere(DefaultSphereSegments, DefaultSphereRings));
		}
		if (FPackageName::GetLongPackagePath(PackageName) != TEXT("/Engine/EngineMaterials"))
		{
			return nullptr;
		}
		// The project's content first, then the engine's, as the legacy default material was found.
		const FString MaterialFile = FPaths::ResolveLegacyContentPath(TEXT("Materials/") + ShortName + TEXT(".lmat"));
		if (FPaths::FileExists(MaterialFile))
		{
			UMaterial* Material = NewEngineAsset<UMaterial>(PackageName, AssetName);
			if (!ReadLegacyMaterial(MaterialFile, *Material) && ShortName == TEXT("M_Default"))
			{
				MakeCheckerMaterial(*Material);
			}
			return Material;
		}
		if (ShortName == TEXT("M_Default"))
		{
			UMaterial* Material = NewEngineAsset<UMaterial>(PackageName, AssetName);
			MakeCheckerMaterial(*Material);
			return Material;
		}
		const FString TextureFile = FPaths::ResolveLegacyContentPath(TEXT("Textures/") + ShortName + TEXT(".png"));
		int32 Width = 0;
		int32 Height = 0;
		TArray<uint8> Pixels;
		if (FPaths::FileExists(TextureFile) && LoadImagePixels(TextureFile, Width, Height, Pixels))
		{
			UTexture2D* Texture = NewEngineAsset<UTexture2D>(PackageName, AssetName);
			(void)Texture->SetPlatformData(Width, Height, PF_R8G8B8A8, Pixels.GetData());
			return Texture;
		}
		return nullptr;
	}

} // namespace

FString FLegacyAssetLoader::GetLegacyPackageName(const FString& Filename)
{
	const FString Full = NormalizeFullPath(Filename);
	const FString EngineContent = NormalizeFullPath(FPaths::EngineContentDir());
	const FString ProjectContent = NormalizeFullPath(FPaths::ProjectContentDir());
	FString Relative;
	if (Full.StartsWith(EngineContent))
	{
		Relative = TEXT("Engine/") + Full.Mid(EngineContent.Len());
	}
	else if (Full.StartsWith(ProjectContent))
	{
		Relative = TEXT("Game/") + Full.Mid(ProjectContent.Len());
	}
	else
	{
		Relative = TEXT("External/") + Full;
	}
	// The extension stays in the name ("M_Red.lmat" -> "M_Red_lmat"), so a mesh and a texture of the same name differ.
	FString Sanitized = SanitizeName(Relative, true);
	while (Sanitized.Contains(TEXT("//")))
	{
		Sanitized = Sanitized.Replace(TEXT("//"), TEXT("/"));
	}
	Sanitized.RemoveFromEnd(TEXT("/"));
	return FString(LegacyPackageRoot) + TEXT("/") + Sanitized;
}

UStaticMesh* FLegacyAssetLoader::LoadStaticMesh(const FString& Filename)
{
	if (UStaticMesh* Found = FindLegacyAsset<UStaticMesh>(Filename))
	{
		return Found;
	}
	// Cooked .lmesh only (OBJ / FBX / glTF go through LeonCook).
	if (!IsLeonMeshPath(Filename))
	{
		UE_LOG(LogEngine, Error, "LegacyAssetLoader: expected a .lmesh, got '%s'", *Filename);
		return nullptr;
	}
	FMeshData Data;
	if (!LoadLeonMeshFile(Filename, Data) || Data.IsEmpty())
	{
		UE_LOG(LogEngine, Error, "LegacyAssetLoader: cannot load the .lmesh '%s'", *Filename);
		return nullptr;
	}

	UStaticMesh* Mesh = NewLegacyAsset<UStaticMesh>(Filename);
	(void)Mesh->BuildFromMeshData(Data);
	// One material per slot: the slot's default parameters and the diffuse map its MTL named.
	for (int32 Slot = 0; Slot < Data.Materials.Num(); ++Slot)
	{
		FMaterial Values = Data.Materials[Slot];
		if (Data.AlbedoMapPaths.IsValidIndex(Slot) && !Data.AlbedoMapPaths[Slot].IsEmpty())
		{
			Values.AlbedoMap = LoadTexture(Data.AlbedoMapPaths[Slot]);
			if (Values.AlbedoMap == nullptr)
			{
				UE_LOG(LogEngine, Error, "LegacyAssetLoader: missing albedo map '%s'", *Data.AlbedoMapPaths[Slot]);
			}
		}
		UMaterial* SlotMaterial = NewTransientAsset<UMaterial>(
			Mesh->GetOutermost()->GetName(), FString::Printf(TEXT("%s_Slot%d"), *Mesh->GetName(), Slot));
		SlotMaterial->SetFromRenderProxy(Values);
		Mesh->StaticMaterials.Add(FStaticMaterial(SlotMaterial));
	}
	return Mesh;
}

UTexture2D* FLegacyAssetLoader::LoadTexture(const FString& Filename)
{
	if (UTexture2D* Found = FindLegacyAsset<UTexture2D>(Filename))
	{
		return Found;
	}
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> Pixels;
	if (!LoadImagePixels(Filename, Width, Height, Pixels))
	{
		return nullptr;
	}
	UTexture2D* Texture = NewLegacyAsset<UTexture2D>(Filename);
	(void)Texture->SetPlatformData(Width, Height, PF_R8G8B8A8, Pixels.GetData());
	return Texture;
}

UMaterial* FLegacyAssetLoader::LoadMaterial(const FString& Filename)
{
	if (UMaterial* Found = FindLegacyAsset<UMaterial>(Filename))
	{
		return Found;
	}
	UMaterial* Material = NewLegacyAsset<UMaterial>(Filename);
	if (!ReadLegacyMaterial(Filename, *Material))
	{
		// Nothing references it: the next collection frees it.
		Material->MarkPendingKill();
		return nullptr;
	}
	return Material;
}

USoundWave* FLegacyAssetLoader::LoadSoundWave(const FString& Filename)
{
	if (USoundWave* Found = FindLegacyAsset<USoundWave>(Filename))
	{
		return Found;
	}
	TArray<int16> Samples;
	int32 NumChannels = 0;
	int32 SampleRate = 0;
	if (!ReadWave(Filename, Samples, NumChannels, SampleRate))
	{
		return nullptr;
	}
	USoundWave* Sound = NewLegacyAsset<USoundWave>(Filename);
	(void)Sound->SetPCMData(Samples.GetData(), Samples.Num() / NumChannels, NumChannels, SampleRate);
	return Sound;
}

UObject* FLegacyAssetLoader::LoadEngineObject(UClass* Class, const FSoftObjectPath& ObjectPath)
{
	if (ObjectPath.IsNull())
	{
		return nullptr;
	}
	UObject* Object = ObjectPath.ResolveObject();
	if (Object != nullptr && Object->IsPendingKill())
	{
		// Its name is taken until the garbage collector frees it: nothing can be made there before.
		UE_LOG(LogEngine, Warning, "LegacyAssetLoader: '%s' is pending kill", *ObjectPath.ToString());
		return nullptr;
	}
	if (Object == nullptr)
	{
		const FString PackageName = ObjectPath.GetLongPackageName();
		if (FPackageName::DoesPackageExist(PackageName))
		{
			Object = StaticLoadObject(Class, nullptr, *ObjectPath.ToString());
		}
		else
		{
			Object = MakeEngineObject(PackageName, ObjectPath.GetAssetName());
		}
	}
	if (Object == nullptr)
	{
		UE_LOG(
			LogEngine, Warning, "LegacyAssetLoader: no package and no legacy source for '%s'", *ObjectPath.ToString());
		return nullptr;
	}
	if (Class != nullptr && !Object->IsA(Class))
	{
		UE_LOG(LogEngine, Warning, "LegacyAssetLoader: '%s' is a %s, not a %s", *ObjectPath.ToString(),
			*Object->GetClass()->GetName(), *Class->GetName());
		return nullptr;
	}
	return Object;
}

UStaticMesh* FLegacyAssetLoader::GetSphereMesh(int32 Segments, int32 Rings)
{
	if (Segments == DefaultSphereSegments && Rings == DefaultSphereRings)
	{
		return LoadEngineObject<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	}
	const FString Name = FString::Printf("Sphere_%dx%d", Segments, Rings);
	const FString PackageName = FString(LegacyPackageRoot) + TEXT("/BasicShapes/") + Name;
	if (UStaticMesh* Found = FindObject<UStaticMesh>(nullptr, *(PackageName + TEXT(".") + Name)))
	{
		if (!Found->IsPendingKill())
		{
			return Found;
		}
	}
	return BuildProceduralMesh(NewTransientAsset<UStaticMesh>(PackageName, Name), MakeSphere(Segments, Rings));
}
