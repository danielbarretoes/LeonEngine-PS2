#include "ResourceCache.h"

#include "Containers/StringConv.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EngineLogs.h"
#include "LeonMeshFormat.h"
#include "MaterialAsset.h"
#include "Materials/Material.h"
#include "MeshData.h"
#include "Misc/Paths.h"
#include "Primitives.h"
#include "UObject/Package.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{

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
	bool LoadImagePixels(const FString& Path, int32& OutWidth, int32& OutHeight, TArray<uint8>& OutPixels)
	{
		stbi_set_flip_vertically_on_load(1);
		int32 Channels = 0;
		uint8* Data = stbi_load(TCHAR_TO_UTF8(*Path), &OutWidth, &OutHeight, &Channels, 4);
		if (Data == nullptr)
		{
			UE_LOG(LogEngine, Error, "Failed to load texture: %s (%s)", *Path, stbi_failure_reason());
			return false;
		}
		OutPixels.Reset();
		OutPixels.Append(Data, OutWidth * OutHeight * 4);
		stbi_image_free(Data);
		return OutWidth > 0 && OutHeight > 0;
	}

} // namespace

FString FResourceCache::NormalizeKey(const FString& Path)
{
	FString Key = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Key);
	FPaths::CollapseRelativeDirectories(Key);
	return Key;
}

UStaticMesh* FResourceCache::CacheMesh(const FString& Key, const FMeshData& Data)
{
	if (UStaticMesh* const* Found = Meshes.Find(Key))
	{
		return *Found;
	}
	if (Data.IsEmpty())
	{
		return nullptr;
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!Mesh->BuildFromMeshData(Data))
	{
		return nullptr;
	}
	// One material per slot of the data (none for the procedural shapes).
	for (int32 Slot = 0; Slot < Data.Materials.Num(); ++Slot)
	{
		UMaterial* SlotMaterial = NewObject<UMaterial>(Mesh, NAME_None, RF_Transient);
		SlotMaterial->SetFromRenderProxy(Data.Materials[Slot]);
		Mesh->StaticMaterials.Add(FStaticMaterial(SlotMaterial));
	}
	Meshes.Add(Key, Mesh);
	return Mesh;
}

UTexture2D* FResourceCache::CacheTexture(const FString& Key, int32 SizeX, int32 SizeY, const uint8* Rgba)
{
	UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!Texture->SetPlatformData(SizeX, SizeY, PF_R8G8B8A8, Rgba))
	{
		return nullptr;
	}
	Textures.Add(Key, Texture);
	return Texture;
}

UStaticMesh* FResourceCache::LoadStaticMesh(const FString& Path)
{
	const FString CacheKey = NormalizeKey(Path);
	if (UStaticMesh* const* Found = Meshes.Find(CacheKey))
	{
		return *Found;
	}

	// Shipping / runtime: cooked .lmesh only (OBJ/FBX/glTF go through LeonCook).
	if (!IsLeonMeshPath(Path))
	{
		UE_LOG(LogEngine, Error, "ResourceCache: expected .lmesh path, got '%s'", *Path);
		return nullptr;
	}
	FMeshData Data;
	if (!LoadLeonMeshFile(Path, Data))
	{
		UE_LOG(LogEngine, Error, "ResourceCache: failed to load .lmesh '%s'", *Path);
		return nullptr;
	}

	// The diffuse textures the MTL references.
	for (int32 I = 0; I < Data.Materials.Num() && I < Data.AlbedoMapPaths.Num(); ++I)
	{
		if (Data.AlbedoMapPaths[I].IsEmpty())
		{
			continue;
		}
		Data.Materials[I].AlbedoMap = LoadTexture(Data.AlbedoMapPaths[I]);
		if (Data.Materials[I].AlbedoMap == nullptr)
		{
			UE_LOG(LogEngine, Error, "ResourceCache: missing albedo map '%s'", *Data.AlbedoMapPaths[I]);
		}
	}

	return CacheMesh(CacheKey, Data);
}

UTexture2D* FResourceCache::LoadTexture(const FString& Path)
{
	if (!bTextureLoadingEnabled)
	{
		return nullptr;
	}
	const FString CacheKey = NormalizeKey(Path);
	if (UTexture2D* const* Found = Textures.Find(CacheKey))
	{
		return *Found;
	}
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> Pixels;
	if (!LoadImagePixels(Path, Width, Height, Pixels))
	{
		return nullptr;
	}
	return CacheTexture(CacheKey, Width, Height, Pixels.GetData());
}

UTexture2D* FResourceCache::CheckerTexture(int32 Size)
{
	if (!bTextureLoadingEnabled)
	{
		return nullptr;
	}
	Size = FMath::Max(Size, 2);
	const FString CacheKey = FString::Printf("proc:checker:%d", Size);
	if (UTexture2D* const* Found = Textures.Find(CacheKey))
	{
		return *Found;
	}
	TArray<uint8> Pixels;
	MakeCheckerPixels(Size, Pixels);
	return CacheTexture(CacheKey, Size, Size, Pixels.GetData());
}

UTexture2D* FResourceCache::BumpNormalTexture(int32 Size)
{
	if (!bTextureLoadingEnabled)
	{
		return nullptr;
	}
	Size = FMath::Max(Size, 8);
	const FString CacheKey = FString::Printf("proc:normal:bump:%d", Size);
	if (UTexture2D* const* Found = Textures.Find(CacheKey))
	{
		return *Found;
	}
	TArray<uint8> Pixels;
	MakeBumpNormalPixels(Size, Pixels);
	UTexture2D* Texture = CacheTexture(CacheKey, Size, Size, Pixels.GetData());
	if (Texture != nullptr)
	{
		Texture->SRGB = 0;
	}
	return Texture;
}

FMaterial FResourceCache::LoadMaterial(const FString& Path)
{
	const FString CacheKey = NormalizeKey(Path);
	if (const FMaterial* Found = Materials.Find(CacheKey))
	{
		return *Found;
	}

	FMaterial Material;
	if (!LoadMaterialFile(*this, Path, Material))
	{
		UE_LOG(LogEngine, Warning, "ResourceCache: using default material (failed '%s')", *Path);
		return DefaultMaterial();
	}
	Materials.Add(CacheKey, Material);
	return Material;
}

FMaterial FResourceCache::DefaultMaterial()
{
	const FString Key("engine:default");
	if (const FMaterial* Found = Materials.Find(Key))
	{
		return *Found;
	}

	FMaterial Material;
	const FString LmatPath = FPaths::ResolveLegacyContentPath("assets/Materials/M_Default.lmat");
	if (FPaths::FileExists(LmatPath) && LoadMaterialFile(*this, LmatPath, Material))
	{
		Materials.Add(Key, Material);
		return Material;
	}

	if (bTextureLoadingEnabled)
	{
		Material = MakeDefaultCheckerMaterial(*this);
	}
	else
	{
		Material.Shading = EMaterialLightingModel::BlinnPhong;
		Material.Albedo = FVector(0.55f, 0.55f, 0.58f);
		Material.Shininess = 16.0f;
		Material.SyncRoughnessFromShininess();
	}
	Materials.Add(Key, Material);
	return Material;
}

UStaticMesh* FResourceCache::GetCubeMesh()
{
	return CacheMesh("proc:cube", MakeCube());
}

UStaticMesh* FResourceCache::GetPlaneMesh(float Size, float UvScale)
{
	const FString Key = FString::Printf("proc:plane:%f:%f", static_cast<double>(Size), static_cast<double>(UvScale));
	return CacheMesh(Key, MakePlane(Size, UvScale));
}

UStaticMesh* FResourceCache::GetSphereMesh(int32 Segments, int32 Rings)
{
	const FString Key = FString::Printf("proc:sphere:%d:%d", Segments, Rings);
	return CacheMesh(Key, MakeSphere(Segments, Rings));
}

void FResourceCache::Clear()
{
	Meshes.Empty();
	Textures.Empty();
	Materials.Empty();
}

void FResourceCache::InvalidateMaterial(const FString& Path)
{
	Materials.Remove(NormalizeKey(Path));
}

void FResourceCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<FString, UStaticMesh*>& Pair : Meshes)
	{
		Collector.AddReferencedObject(Pair.Value);
	}
	for (TPair<FString, UTexture2D*>& Pair : Textures)
	{
		Collector.AddReferencedObject(Pair.Value);
	}
}
