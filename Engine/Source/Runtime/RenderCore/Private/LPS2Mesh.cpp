#include "LPS2Mesh.h"

#include "Misc/Crc.h"
#include "RHIDeferredRelease.h"

namespace
{

	const FLPS2MeshHeader GEmptyHeader;

	/** A record of type RecordType at Offset bytes into Blob (the caller checked that it is inside). */
	template <typename RecordType>
	[[nodiscard]] const RecordType& RecordAt(const TArray<uint8>& Blob, int32 Offset)
	{
		return *reinterpret_cast<const RecordType*>(Blob.GetData() + Offset);
	}

} // namespace

bool FLPS2Mesh::IsValidBlob(const TArray<uint8>& Blob)
{
	const int32 Size = Blob.Num();
	if (Size < int32(sizeof(FLPS2MeshHeader)) || (Size % 16) != 0)
	{
		return false;
	}
	const FLPS2MeshHeader& Header = RecordAt<FLPS2MeshHeader>(Blob, 0);
	if (Header.Magic != Magic || Header.Version != Version || (Header.Flags & ~FlagSkinned) != 0 ||
		Header.NumSections == 0)
	{
		return false;
	}
	const bool bSkinned = (Header.Flags & FlagSkinned) != 0;
	const uint32 MaxVertices = uint32(bSkinned ? MaxSkinnedBatchVertices : MaxBatchVertices);
	// The tables, counted in 64 bits so that no count can wrap.
	const int64 TablesEnd = int64(sizeof(FLPS2MeshHeader)) + (int64(Header.NumSections) * int64(sizeof(FLPS2Section))) +
		(int64(Header.NumBatches) * int64(sizeof(FLPS2Batch)));
	if (TablesEnd > int64(Size))
	{
		return false;
	}
	for (uint32 Index = 0; Index < Header.NumSections; ++Index)
	{
		const FLPS2Section& Section =
			RecordAt<FLPS2Section>(Blob, int32(sizeof(FLPS2MeshHeader)) + (int32(Index) * int32(sizeof(FLPS2Section))));
		if (int64(Section.FirstBatch) + int64(Section.NumBatches) > int64(Header.NumBatches))
		{
			return false;
		}
	}
	const int32 BatchTable = BatchTableOffset(int32(Header.NumSections));
	uint64 NumVertices = 0;
	for (uint32 Index = 0; Index < Header.NumBatches; ++Index)
	{
		const FLPS2Batch& Batch = RecordAt<FLPS2Batch>(Blob, BatchTable + (int32(Index) * int32(sizeof(FLPS2Batch))));
		if (Batch.NumVertices < 3 || Batch.NumVertices > MaxVertices || (Batch.DataOffset % 16) != 0 ||
			int64(Batch.DataOffset) < TablesEnd ||
			int64(Batch.DataOffset) + BatchBytes(int32(Batch.NumVertices), bSkinned) > int64(Size))
		{
			return false;
		}
		if (bSkinned)
		{
			// The palette, and every vertex's palette indices and weights.
			const FLPS2SkinPalette& Palette = RecordAt<FLPS2SkinPalette>(Blob, int32(Batch.DataOffset));
			if (Palette.NumBones == 0 || Palette.NumBones > uint32(MaxPaletteBones))
			{
				return false;
			}
			const int32 SkinOffset = int32(Batch.DataOffset) + BatchBytes(int32(Batch.NumVertices), true) -
				SkinBytes(int32(Batch.NumVertices));
			for (uint32 Vertex = 0; Vertex < Batch.NumVertices; ++Vertex)
			{
				const uint8* Skin = Blob.GetData() + SkinOffset + (Vertex * 4);
				if (Skin[0] >= Palette.NumBones || Skin[1] >= Palette.NumBones ||
					int32(Skin[2]) + int32(Skin[3]) != 255)
				{
					return false;
				}
			}
		}
		NumVertices += Batch.NumVertices;
	}
	return NumVertices == Header.NumVertices;
}

FLPS2Mesh::~FLPS2Mesh()
{
	ReleaseData();
}

FLPS2Mesh& FLPS2Mesh::operator=(const FLPS2Mesh& Other)
{
	if (this != &Other)
	{
		ReleaseData();
		Data = Other.Data;
		SectionUvDensity = Other.SectionUvDensity;
		ColorStreamOffsets = Other.ColorStreamOffsets;
		DataCrc = Other.DataCrc;
	}
	return *this;
}

FLPS2Mesh& FLPS2Mesh::operator=(FLPS2Mesh&& Other)
{
	if (this != &Other)
	{
		ReleaseData();
		Data = MoveTemp(Other.Data);
		SectionUvDensity = MoveTemp(Other.SectionUvDensity);
		ColorStreamOffsets = MoveTemp(Other.ColorStreamOffsets);
		DataCrc = Other.DataCrc;
	}
	return *this;
}

void FLPS2Mesh::ReleaseData()
{
	FRHIDeferredRelease::Release(MoveTemp(Data));
	Data.Reset();
}

bool FLPS2Mesh::SetData(TArray<uint8>&& InData)
{
	ReleaseData();
	if (!IsValidBlob(InData))
	{
		return false;
	}
	Data = MoveTemp(InData);
	ComputeDerivedData();
	return true;
}

void FLPS2Mesh::ComputeDerivedData()
{
	ComputeUvDensity();
	ColorStreamOffsets.Reset();
	int32 Offset = 0;
	for (int32 BatchIndex = 0; BatchIndex < GetNumBatches(); ++BatchIndex)
	{
		ColorStreamOffsets.Add(Offset);
		Offset += ColorBytes(int32(GetBatch(BatchIndex).NumVertices));
	}
	ColorStreamOffsets.Add(Offset);
	DataCrc = FCrc::MemCrc32(Data.GetData(), Data.Num());
}

void FLPS2Mesh::ComputeUvDensity()
{
	SectionUvDensity.Reset();
	for (int32 SectionIndex = 0; SectionIndex < GetNumSections(); ++SectionIndex)
	{
		const FLPS2Section& Section = GetSection(SectionIndex);
		float Area = 0.0f;
		float UvArea = 0.0f;
		for (uint32 BatchIndex = Section.FirstBatch; BatchIndex < Section.FirstBatch + Section.NumBatches; ++BatchIndex)
		{
			const FLPS2Batch& Batch = GetBatch(int32(BatchIndex));
			const int16* Positions = GetPositions(Batch);
			const int8* Normals = GetNormals(Batch);
			const int16* TexCoords = GetTexCoords(Batch);
			for (int32 Index = 2; Index < int32(Batch.NumVertices); ++Index)
			{
				if ((uint8(Normals[(Index * 4) + 3]) & FlagNoKick) != 0)
				{
					continue;
				}
				const FVector A = DequantizePosition(Positions + ((Index - 2) * 3));
				const FVector B = DequantizePosition(Positions + ((Index - 1) * 3));
				const FVector C = DequantizePosition(Positions + (Index * 3));
				Area += ((B - A) ^ (C - A)).Size() * 0.5f;
				const FVector2D UvA = DequantizeTexCoord(TexCoords + ((Index - 2) * 2), Batch);
				const FVector2D UvB = DequantizeTexCoord(TexCoords + ((Index - 1) * 2), Batch);
				const FVector2D UvC = DequantizeTexCoord(TexCoords + (Index * 2), Batch);
				UvArea += FMath::Abs(FVector2D::CrossProduct(UvB - UvA, UvC - UvA)) * 0.5f;
			}
		}
		SectionUvDensity.Add(Area > 0.0f ? FMath::Sqrt(UvArea / Area) : 0.0f);
	}
}

const FLPS2MeshHeader& FLPS2Mesh::GetHeader() const
{
	return Data.Num() > 0 ? RecordAt<FLPS2MeshHeader>(Data, 0) : GEmptyHeader;
}

const FLPS2Section& FLPS2Mesh::GetSection(int32 Index) const
{
	check(Index >= 0 && Index < GetNumSections());
	return RecordAt<FLPS2Section>(Data, int32(sizeof(FLPS2MeshHeader)) + (Index * int32(sizeof(FLPS2Section))));
}

const FLPS2Batch& FLPS2Mesh::GetBatch(int32 Index) const
{
	check(Index >= 0 && Index < GetNumBatches());
	return RecordAt<FLPS2Batch>(Data, BatchTableOffset(GetNumSections()) + (Index * int32(sizeof(FLPS2Batch))));
}

const int16* FLPS2Mesh::GetPositions(const FLPS2Batch& Batch) const
{
	return reinterpret_cast<const int16*>(Data.GetData() + GetStreamsOffset(Batch));
}

const int8* FLPS2Mesh::GetNormals(const FLPS2Batch& Batch) const
{
	return reinterpret_cast<const int8*>(
		Data.GetData() + GetStreamsOffset(Batch) + PositionBytes(int32(Batch.NumVertices)));
}

const uint8* FLPS2Mesh::GetColors(const FLPS2Batch& Batch) const
{
	const int32 NumVertices = int32(Batch.NumVertices);
	return Data.GetData() + GetStreamsOffset(Batch) + PositionBytes(NumVertices) + NormalBytes(NumVertices);
}

const int16* FLPS2Mesh::GetTexCoords(const FLPS2Batch& Batch) const
{
	const int32 NumVertices = int32(Batch.NumVertices);
	return reinterpret_cast<const int16*>(Data.GetData() + GetStreamsOffset(Batch) + PositionBytes(NumVertices) +
		NormalBytes(NumVertices) + ColorBytes(NumVertices));
}

const FLPS2SkinPalette& FLPS2Mesh::GetPalette(const FLPS2Batch& Batch) const
{
	check(IsSkinned());
	return RecordAt<FLPS2SkinPalette>(Data, int32(Batch.DataOffset));
}

const uint8* FLPS2Mesh::GetSkin(const FLPS2Batch& Batch) const
{
	check(IsSkinned());
	const int32 NumVertices = int32(Batch.NumVertices);
	return Data.GetData() + GetStreamsOffset(Batch) + PositionBytes(NumVertices) + NormalBytes(NumVertices) +
		ColorBytes(NumVertices) + TexCoordBytes(NumVertices);
}

void FLPS2Mesh::Serialize(FArchive& Ar)
{
	if (!Ar.IsLoading())
	{
		Ar << Data;
		return;
	}
	TArray<uint8> Loaded;
	Ar << Loaded;
	if (Ar.IsError() || (Loaded.Num() > 0 && !IsValidBlob(Loaded)))
	{
		Ar.SetError();
		ReleaseData();
		SectionUvDensity.Empty();
		ColorStreamOffsets.Empty();
		DataCrc = 0;
		return;
	}
	ReleaseData();
	Data = MoveTemp(Loaded);
	ComputeDerivedData();
}

FLPS2ColorStreams::~FLPS2ColorStreams()
{
	Reset();
}

FLPS2ColorStreams& FLPS2ColorStreams::operator=(const FLPS2ColorStreams& Other)
{
	if (this != &Other)
	{
		Reset();
		MeshCrc = Other.MeshCrc;
		Data = Other.Data;
	}
	return *this;
}

FLPS2ColorStreams& FLPS2ColorStreams::operator=(FLPS2ColorStreams&& Other)
{
	if (this != &Other)
	{
		Reset();
		MeshCrc = Other.MeshCrc;
		Data = MoveTemp(Other.Data);
	}
	return *this;
}

void FLPS2ColorStreams::Reset()
{
	MeshCrc = 0;
	FRHIDeferredRelease::Release(MoveTemp(Data));
	Data.Reset();
}

void FLPS2ColorStreams::Init(const FLPS2Mesh& Mesh)
{
	Reset();
	MeshCrc = Mesh.GetDataCrc();
	Data.SetNumUninitialized(Mesh.GetColorStreamsSize());
	if (Data.Num() > 0)
	{
		FMemory::Memset(Data.GetData(), 0xff, static_cast<SIZE_T>(Data.Num()));
	}
}

bool FLPS2ColorStreams::Matches(const FLPS2Mesh& Mesh) const
{
	return !Mesh.IsEmpty() && Data.Num() > 0 && MeshCrc == Mesh.GetDataCrc() &&
		Data.Num() == Mesh.GetColorStreamsSize();
}

void FLPS2ColorStreams::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		Reset();
	}
	Ar << MeshCrc;
	Ar << Data;
	if (Ar.IsLoading() && (Data.Num() % 16) != 0)
	{
		Ar.SetError();
		Reset();
	}
}
