#include "SkyBoxGeometry.h"

#include "Engine/TextureCube.h"
#include "LPS2Mesh.h"

namespace
{

	/** Positions are quantized in 1/16384 of the box's half size. */
	constexpr float PositionScale = 1.0f / 16384.0f;

	/** One vertex of a batch as the streams hold it. */
	struct FSkyVertex
	{
		int16 Position[3] = {0, 0, 0};
		int8 Normal[4] = {0, 0, 0, 0};
		int16 TexCoord[2] = {0, 0};
	};

	/** The vertex of grid point (Column, Row) of a face: its position, its inward normal and its UV. */
	FSkyVertex MakeVertex(ECubeFace Face, int32 Column, int32 Row)
	{
		FVector Forward;
		FVector Right;
		FVector Up;
		UTextureCube::GetFaceBasis(Face, Forward, Right, Up);
		constexpr int32 Side = FSkyBoxGeometry::QuadsPerSide;
		const float U = float(Column) / float(Side);
		const float V = float(Row) / float(Side);
		const FVector Position = Forward + (Right * ((U * 2.0f) - 1.0f)) + (Up * ((V * 2.0f) - 1.0f));
		FSkyVertex Vertex;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Vertex.Position[Axis] = int16(FMath::RoundToInt(Position[Axis] / PositionScale));
			Vertex.Normal[Axis] = int8(FMath::RoundToInt(-Forward[Axis] * float(FLPS2Mesh::NormalRange)));
		}
		Vertex.TexCoord[0] = int16((Column * FLPS2Mesh::TexCoordOne) / Side);
		Vertex.TexCoord[1] = int16((Row * FLPS2Mesh::TexCoordOne) / Side);
		return Vertex;
	}

	template <typename RecordType>
	void WriteRecord(TArray<uint8>& Blob, int32 Offset, const RecordType& Record)
	{
		FMemory::Memcpy(Blob.GetData() + Offset, &Record, sizeof(RecordType));
	}

} // namespace

bool FSkyBoxGeometry::BuildMesh(FLPS2Mesh& OutMesh)
{
	constexpr int32 BatchesPerRow = QuadsPerSide / QuadsPerBatch;
	constexpr int32 NumBatches = NumFaces * BatchesPerFace;
	constexpr int32 StripVertices = (QuadsPerBatch + 1) * 2;
	constexpr int32 BatchVertices = StripVertices * QuadsPerBatch;
	static_assert(QuadsPerSide % QuadsPerBatch == 0, "A face is whole batches");
	static_assert(BatchVertices <= FLPS2Mesh::MaxBatchVertices, "A batch fits VU1's buffer");

	const int32 TablesEnd = FLPS2Mesh::BatchTableOffset(NumFaces) + (NumBatches * int32(sizeof(FLPS2Batch)));
	const int32 BatchBytes = FLPS2Mesh::BatchBytes(BatchVertices);
	TArray<uint8> Blob;
	Blob.SetNumZeroed(TablesEnd + (NumBatches * BatchBytes));

	FLPS2MeshHeader Header;
	Header.Magic = FLPS2Mesh::Magic;
	Header.Version = FLPS2Mesh::Version;
	Header.NumSections = uint32(NumFaces);
	Header.NumBatches = uint32(NumBatches);
	Header.NumVertices = uint32(NumBatches * BatchVertices);
	Header.NumTriangles = uint32(NumFaces * QuadsPerSide * QuadsPerSide * 2);
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Header.PositionScale[Axis] = PositionScale;
		Header.PositionBias[Axis] = 0.0f;
	}
	WriteRecord(Blob, 0, Header);

	int32 BatchIndex = 0;
	for (int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		const ECubeFace Face = ECubeFace(FaceIndex);
		FLPS2Section Section;
		Section.FirstBatch = uint32(BatchIndex);
		Section.NumBatches = uint32(BatchesPerFace);
		Section.MaterialIndex = uint32(FaceIndex);
		Section.NumTriangles = uint32(QuadsPerSide * QuadsPerSide * 2);
		WriteRecord(Blob, int32(sizeof(FLPS2MeshHeader)) + (FaceIndex * int32(sizeof(FLPS2Section))), Section);
		for (int32 BatchRow = 0; BatchRow < BatchesPerRow; ++BatchRow)
		{
			for (int32 BatchColumn = 0; BatchColumn < BatchesPerRow; ++BatchColumn)
			{
				// Two strips, a row of quads each, from the top edge down so their triangles face the centre: the top
				// vertex, then the one under it, column after column.
				FSkyVertex Vertices[BatchVertices];
				int32 Count = 0;
				for (int32 Strip = 0; Strip < QuadsPerBatch; ++Strip)
				{
					const int32 Row = (BatchRow * QuadsPerBatch) + Strip;
					for (int32 Step = 0; Step < StripVertices; ++Step)
					{
						const int32 Column = (BatchColumn * QuadsPerBatch) + (Step / 2);
						FSkyVertex& Vertex = Vertices[Count++];
						Vertex = MakeVertex(Face, Column, (Step % 2) == 0 ? Row + 1 : Row);
						// A strip's first two vertices close no triangle; its odd triangles are wound the other way.
						Vertex.Normal[3] = int8(
							Step < 2 ? FLPS2Mesh::FlagNoKick : ((Step - 2) % 2 != 0 ? FLPS2Mesh::FlagReversed : 0));
					}
				}
				FLPS2Batch Batch;
				Batch.DataOffset = uint32(TablesEnd + (BatchIndex * BatchBytes));
				Batch.NumVertices = uint32(BatchVertices);
				FBox Box(ForceInit);
				for (const FSkyVertex& Vertex : Vertices)
				{
					Box += FVector(float(Vertex.Position[0]), float(Vertex.Position[1]), float(Vertex.Position[2])) *
						PositionScale;
				}
				const FVector Center = Box.GetCenter();
				float Radius = 0.0f;
				for (const FSkyVertex& Vertex : Vertices)
				{
					const FVector Point =
						FVector(float(Vertex.Position[0]), float(Vertex.Position[1]), float(Vertex.Position[2])) *
						PositionScale;
					Radius = FMath::Max(Radius, FVector::Dist(Point, Center));
				}
				for (int32 Axis = 0; Axis < 3; ++Axis)
				{
					Batch.BoundsCenter[Axis] = Center[Axis];
				}
				Batch.BoundsRadius = (Radius * 1.0001f) + PositionScale;
				WriteRecord(
					Blob, FLPS2Mesh::BatchTableOffset(NumFaces) + (BatchIndex * int32(sizeof(FLPS2Batch))), Batch);

				// The streams: positions, normals with the strip flags, colours (white), texture coordinates.
				uint8* Data = Blob.GetData() + Batch.DataOffset;
				uint8* Normals = Data + FLPS2Mesh::PositionBytes(BatchVertices);
				uint8* Colors = Normals + FLPS2Mesh::NormalBytes(BatchVertices);
				uint8* TexCoords = Colors + FLPS2Mesh::ColorBytes(BatchVertices);
				for (int32 Index = 0; Index < BatchVertices; ++Index)
				{
					FMemory::Memcpy(Data + (Index * 6), Vertices[Index].Position, 6);
					FMemory::Memcpy(Normals + (Index * 4), Vertices[Index].Normal, 4);
					FMemory::Memset(Colors + (Index * 4), 0xff, 4);
					FMemory::Memcpy(TexCoords + (Index * 4), Vertices[Index].TexCoord, 4);
				}
				++BatchIndex;
			}
		}
	}
	return OutMesh.SetData(MoveTemp(Blob));
}

float FSkyBoxGeometry::GetRadius(const FMatrix& Projection)
{
	// Clip z = A x depth + B: 0 at the near plane, the depth itself at the far one.
	const float A = Projection.M[2][2];
	const float B = Projection.M[3][2];
	const float Near = FMath::Abs(A) > SMALL_NUMBER ? -B / A : 0.0f;
	if (Near <= 0.0f)
	{
		return 1000.0f;
	}
	if (FMath::Abs(1.0f - A) <= KINDA_SMALL_NUMBER)
	{
		return Near * 100.0f;
	}
	const float Far = B / (1.0f - A);
	return Far > Near ? FMath::Sqrt(Near * Far) : Near * 100.0f;
}
