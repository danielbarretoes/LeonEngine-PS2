#include "LeonMeshFormat.h"

#include "HAL/FileManager.h"
#include "LegacyCoordinateConversion.h"
#include "MeshData.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogLeonMesh, Log, All);

namespace
{

	constexpr ANSICHAR Magic[4] = {'L', 'M', 'S', 'H'};
	/** Legacy data: metres, Y up, right-handed. Read only, converted at load. */
	constexpr uint32 LegacyVersion = 1;
	/** Engine world data (UE: Z up, left-handed, centimetres), as the importers produce it. Written. */
	constexpr uint32 Version = 2;

#pragma pack(push, 1)
	struct FLeonMeshHeader
	{
		ANSICHAR Magic[4];
		uint32 Version;
		uint32 Flags;
		uint32 VertexCount;
		uint32 IndexCount;
		uint32 SubmeshCount;
		uint32 MaterialSlotCount;
		float AabbMin[3];
		float AabbMax[3];
	};
#pragma pack(pop)

	struct FLeonMeshSection
	{
		uint32 IndexOffset = 0;
		uint32 IndexCount = 0;
		uint32 MaterialIndex = 0;
	};

	void ComputeAabb(const FMeshData& Data, float OutMin[3], float OutMax[3])
	{
		OutMin[0] = OutMin[1] = OutMin[2] = 0.0f;
		OutMax[0] = OutMax[1] = OutMax[2] = 0.0f;
		if (Data.Vertices.Num() == 0)
		{
			return;
		}
		OutMin[0] = OutMax[0] = Data.Vertices[0].Position.X;
		OutMin[1] = OutMax[1] = Data.Vertices[0].Position.Y;
		OutMin[2] = OutMax[2] = Data.Vertices[0].Position.Z;
		for (const FVertex& V : Data.Vertices)
		{
			OutMin[0] = FMath::Min(OutMin[0], V.Position.X);
			OutMin[1] = FMath::Min(OutMin[1], V.Position.Y);
			OutMin[2] = FMath::Min(OutMin[2], V.Position.Z);
			OutMax[0] = FMath::Max(OutMax[0], V.Position.X);
			OutMax[1] = FMath::Max(OutMax[1], V.Position.Y);
			OutMax[2] = FMath::Max(OutMax[2], V.Position.Z);
		}
	}

	/** Reads Length bytes; false at end of file (the archive reports a short read as an error). */
	[[nodiscard]] bool ReadBytes(FArchive& Ar, void* Data, int64 Length)
	{
		if (Length <= 0)
		{
			return true;
		}
		if (Ar.Tell() + Length > Ar.TotalSize())
		{
			return false;
		}
		Ar.Serialize(Data, Length);
		return !Ar.IsError();
	}

} // namespace

bool IsLeonMeshPath(const FString& Path)
{
	return FPaths::GetExtension(Path, true).Equals(".lmesh", ESearchCase::IgnoreCase);
}

bool LoadLeonMeshFile(const FString& Path, FMeshData& Out)
{
	TUniquePtr<FArchive> In(IFileManager::Get().CreateFileReader(*Path));
	if (!In)
	{
		UE_LOG(LogLeonMesh, Error, "Cannot open %s", *Path);
		return false;
	}
	FLeonMeshHeader Header{};
	if (!ReadBytes(*In, &Header, sizeof(Header)) || FMemory::Memcmp(Header.Magic, Magic, 4) != 0 ||
		(Header.Version != Version && Header.Version != LegacyVersion))
	{
		UE_LOG(LogLeonMesh, Error, "Bad header in %s", *Path);
		return false;
	}
	if (Header.VertexCount == 0 || Header.IndexCount == 0)
	{
		UE_LOG(LogLeonMesh, Error, "Empty mesh in %s", *Path);
		return false;
	}

	FMeshData Data;
	Data.Vertices.SetNum(static_cast<int32>(Header.VertexCount));
	Data.Indices.SetNum(static_cast<int32>(Header.IndexCount));
	if (!ReadBytes(*In, Data.Vertices.GetData(), static_cast<int64>(sizeof(FVertex)) * Header.VertexCount) ||
		!ReadBytes(*In, Data.Indices.GetData(), static_cast<int64>(sizeof(uint32)) * Header.IndexCount))
	{
		UE_LOG(LogLeonMesh, Error, "Truncated vertex / index data in %s", *Path);
		return false;
	}
	if (Header.Version == LegacyVersion)
	{
		FLegacyCoordinateConversion::ConvertMeshData(Data);
	}

	if (Header.SubmeshCount == 0)
	{
		Data.Submeshes.Add(FMeshSection{0, static_cast<int32>(Header.IndexCount), 0});
	}
	else
	{
		TArray<FLeonMeshSection> Subs;
		Subs.SetNum(static_cast<int32>(Header.SubmeshCount));
		if (!ReadBytes(*In, Subs.GetData(), static_cast<int64>(sizeof(FLeonMeshSection)) * Header.SubmeshCount))
		{
			UE_LOG(LogLeonMesh, Error, "Truncated submeshes in %s", *Path);
			return false;
		}
		Data.Submeshes.Reserve(Subs.Num());
		for (const FLeonMeshSection& S : Subs)
		{
			Data.Submeshes.Add(FMeshSection{static_cast<int32>(S.IndexOffset), static_cast<int32>(S.IndexCount),
				static_cast<int32>(S.MaterialIndex)});
		}
	}

	// Optional string table: material slot paths (null-terminated), one per slot.
	Data.Materials.SetNum(static_cast<int32>(FMath::Max<uint32>(1, Header.MaterialSlotCount)));
	Data.AlbedoMapPaths.SetNum(Data.Materials.Num());
	for (uint32 I = 0; I < Header.MaterialSlotCount; ++I)
	{
		FString Slot;
		ANSICHAR Ch = 0;
		while (ReadBytes(*In, &Ch, 1))
		{
			if (Ch == '\0')
			{
				break;
			}
			Slot.AppendChar(Ch);
		}
		// The slot string may become a .lmat path; for now only texture paths are kept (as the albedo map).
		if (!Slot.IsEmpty() &&
			(Slot.Contains(".png", ESearchCase::CaseSensitive) || Slot.Contains(".jpg", ESearchCase::CaseSensitive)))
		{
			Data.AlbedoMapPaths[static_cast<int32>(I)] = Slot;
		}
	}

	Out = MoveTemp(Data);
	return !Out.IsEmpty();
}

bool SaveLeonMeshFile(const FString& Path, const FMeshData& Data)
{
	if (Data.IsEmpty())
	{
		UE_LOG(LogLeonMesh, Error, "Refusing to save an empty mesh");
		return false;
	}
	TUniquePtr<FArchive> Out(IFileManager::Get().CreateFileWriter(*Path));
	if (!Out)
	{
		UE_LOG(LogLeonMesh, Error, "Cannot write %s", *Path);
		return false;
	}

	FLeonMeshHeader Header{};
	FMemory::Memcpy(Header.Magic, Magic, 4);
	Header.Version = Version;
	Header.Flags = 0;
	Header.VertexCount = static_cast<uint32>(Data.Vertices.Num());
	Header.IndexCount = static_cast<uint32>(Data.Indices.Num());
	Header.SubmeshCount = static_cast<uint32>(Data.Submeshes.Num() == 0 ? 1 : Data.Submeshes.Num());
	Header.MaterialSlotCount = static_cast<uint32>(FMath::Max(1, Data.Materials.Num()));
	ComputeAabb(Data, Header.AabbMin, Header.AabbMax);

	Out->Serialize(&Header, sizeof(Header));
	Out->Serialize(
		const_cast<FVertex*>(Data.Vertices.GetData()), static_cast<int64>(sizeof(FVertex)) * Data.Vertices.Num());
	Out->Serialize(
		const_cast<uint32*>(Data.Indices.GetData()), static_cast<int64>(sizeof(uint32)) * Data.Indices.Num());

	if (Data.Submeshes.Num() == 0)
	{
		FLeonMeshSection S{0, Header.IndexCount, 0};
		Out->Serialize(&S, sizeof(S));
	}
	else
	{
		for (const FMeshSection& Sm : Data.Submeshes)
		{
			FLeonMeshSection S{static_cast<uint32>(Sm.IndexOffset), static_cast<uint32>(Sm.IndexCount),
				static_cast<uint32>(Sm.MaterialIndex)};
			Out->Serialize(&S, sizeof(S));
		}
	}

	for (uint32 I = 0; I < Header.MaterialSlotCount; ++I)
	{
		FString Slot;
		if (static_cast<int32>(I) < Data.AlbedoMapPaths.Num())
		{
			Slot = Data.AlbedoMapPaths[static_cast<int32>(I)];
		}
		// TCHAR is UTF-8: the bytes plus the terminator.
		Out->Serialize(const_cast<TCHAR*>(*Slot), Slot.Len() + 1);
	}
	return Out->Close();
}
