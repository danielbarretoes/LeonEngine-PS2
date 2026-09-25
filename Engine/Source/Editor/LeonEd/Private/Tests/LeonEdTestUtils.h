#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

#if WITH_DEV_AUTOMATION_TESTS

// Shared by the LeonEd tests: a mount point over a fresh folder of the program's Intermediate directory (never the
// repository's content), and small source files written by the tests themselves.

namespace LeonEdTest
{

	/** The tests' mount point and its folder: <Intermediate>/Tests/LeonEd/Content, the source files beside it. */
	inline const TCHAR* const Root = TEXT("/LeonEdTest/");

	inline FString GetTestDir()
	{
		return FPaths::ProjectIntermediateDir() + TEXT("Tests/LeonEd/");
	}

	inline FString GetContentDir()
	{
		return GetTestDir() + TEXT("Content/");
	}

	inline FString GetSourceDir()
	{
		return GetTestDir() + TEXT("SourceArt/");
	}

	/** Every object of the packages under a root, pending kill, then collected: a fresh process for the next test. */
	inline void DestroyPackagesUnder(const FString& PackageRoot)
	{
		TArray<UPackage*> Packages;
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			if (It->GetName().StartsWith(PackageRoot))
			{
				Packages.Add(*It);
			}
		}
		for (UPackage* Package : Packages)
		{
			TArray<UObject*> Objects;
			GetObjectsWithOuter(Package, Objects, true);
			for (UObject* Object : Objects)
			{
				Object->RemoveFromRoot();
				Object->MarkPendingKill();
			}
			Package->MarkPendingKill();
		}
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
	}

	/**
	 * The test mount point over an empty folder for a test's lifetime; at the end its packages are destroyed, the
	 * folder deleted and the mount point removed.
	 */
	class FScopedTestContent
	{
	public:
		FScopedTestContent()
		{
			IFileManager::Get().DeleteDirectory(*GetTestDir(), false, true);
			IFileManager::Get().MakeDirectory(*GetContentDir(), true);
			IFileManager::Get().MakeDirectory(*GetSourceDir(), true);
			FPackageName::RegisterMountPoint(Root, GetContentDir());
		}

		~FScopedTestContent()
		{
			DestroyPackagesUnder(Root);
			FPackageName::UnRegisterMountPoint(Root, GetContentDir());
			IFileManager::Get().DeleteDirectory(*GetTestDir(), false, true);
		}

		FScopedTestContent(const FScopedTestContent&) = delete;
		FScopedTestContent& operator=(const FScopedTestContent&) = delete;
	};

	inline void AppendU16(TArray<uint8>& Bytes, uint32 Value)
	{
		Bytes.Add(static_cast<uint8>(Value & 0xFF));
		Bytes.Add(static_cast<uint8>((Value >> 8) & 0xFF));
	}

	inline void AppendU32(TArray<uint8>& Bytes, uint32 Value)
	{
		AppendU16(Bytes, Value & 0xFFFF);
		AppendU16(Bytes, Value >> 16);
	}

	inline void AppendTag(TArray<uint8>& Bytes, const char* Tag)
	{
		Bytes.Append(reinterpret_cast<const uint8*>(Tag), 4);
	}

	/**
	 * A 24-bit BMP of Width x Height RGB texels given bottom row first (as the file stores them and the texture keeps
	 * them).
	 */
	inline TArray<uint8> MakeBmp(int32 Width, int32 Height, const TArray<uint8>& RGB)
	{
		const int32 Stride = ((Width * 3) + 3) & ~3;
		const uint32 DataSize = static_cast<uint32>(Stride * Height);
		TArray<uint8> Bytes;
		Bytes.Add('B');
		Bytes.Add('M');
		AppendU32(Bytes, 54 + DataSize);
		AppendU32(Bytes, 0);
		AppendU32(Bytes, 54);
		AppendU32(Bytes, 40);
		AppendU32(Bytes, static_cast<uint32>(Width));
		AppendU32(Bytes, static_cast<uint32>(Height));
		AppendU16(Bytes, 1);
		AppendU16(Bytes, 24);
		AppendU32(Bytes, 0);
		AppendU32(Bytes, DataSize);
		AppendU32(Bytes, 2835);
		AppendU32(Bytes, 2835);
		AppendU32(Bytes, 0);
		AppendU32(Bytes, 0);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 I = ((Y * Width) + X) * 3;
				Bytes.Add(RGB[I + 2]);
				Bytes.Add(RGB[I + 1]);
				Bytes.Add(RGB[I + 0]);
			}
			for (int32 Pad = Width * 3; Pad < Stride; ++Pad)
			{
				Bytes.Add(0);
			}
		}
		return Bytes;
	}

	/** A RIFF / WAVE file of Samples (interleaved) with BitsPerSample bits (16: PCM16; 8: an 8-bit file). */
	inline TArray<uint8> MakeWave(const TArray<int16>& Samples, int32 Channels, int32 Rate, int32 BitsPerSample = 16)
	{
		const int32 BytesPerSample = BitsPerSample / 8;
		const int32 DataSize = Samples.Num() * BytesPerSample;
		TArray<uint8> Bytes;
		AppendTag(Bytes, "RIFF");
		AppendU32(Bytes, static_cast<uint32>(36 + DataSize));
		AppendTag(Bytes, "WAVE");
		AppendTag(Bytes, "fmt ");
		AppendU32(Bytes, 16);
		AppendU16(Bytes, 1);
		AppendU16(Bytes, static_cast<uint32>(Channels));
		AppendU32(Bytes, static_cast<uint32>(Rate));
		AppendU32(Bytes, static_cast<uint32>(Rate * Channels * BytesPerSample));
		AppendU16(Bytes, static_cast<uint32>(Channels * BytesPerSample));
		AppendU16(Bytes, static_cast<uint32>(BitsPerSample));
		AppendTag(Bytes, "data");
		AppendU32(Bytes, static_cast<uint32>(DataSize));
		for (const int16 Sample : Samples)
		{
			if (BytesPerSample == 2)
			{
				AppendU16(Bytes, static_cast<uint16>(Sample));
			}
			else
			{
				Bytes.Add(static_cast<uint8>(Sample));
			}
		}
		return Bytes;
	}

	/** Writes Text to a file under the source folder; returns its full path. */
	inline FString WriteSource(const FString& RelativePath, const FString& Text)
	{
		const FString Path = FPaths::ConvertRelativePathToFull(GetSourceDir() + RelativePath);
		(void)FFileHelper::SaveStringToFile(Text, *Path);
		return Path;
	}

	/** Writes Bytes to a file under the source folder; returns its full path. */
	inline FString WriteSource(const FString& RelativePath, const TArray<uint8>& Bytes)
	{
		const FString Path = FPaths::ConvertRelativePathToFull(GetSourceDir() + RelativePath);
		(void)FFileHelper::SaveArrayToFile(Bytes, *Path);
		return Path;
	}

	/** The bytes of a file (empty when missing). */
	inline TArray<uint8> ReadBytes(const FString& Path)
	{
		TArray<uint8> Bytes;
		(void)FFileHelper::LoadFileToArray(Bytes, *Path);
		return Bytes;
	}

} // namespace LeonEdTest

#endif // WITH_DEV_AUTOMATION_TESTS
