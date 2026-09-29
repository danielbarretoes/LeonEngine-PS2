#include "LPS2MeshBuilder.h"

#include "MeshUtilitiesLog.h"

#include <meshoptimizer.h>

namespace
{

	/** The strip restart of meshopt_stripify's output (never a vertex index). */
	constexpr uint32 RestartIndex = ~0u;

	/**
	 * The widest span of a batch's texture coordinates, in 4.12 units: centred on a whole offset (which may be half a
	 * repeat off the centre), the values stay within int16.
	 */
	constexpr int32 MaxTexCoordSpan = 14 * FLPS2Mesh::TexCoordOne;

	/**
	 * A corner as it is saved (texture coordinates before the batch's offset, the skeleton's bones before the batch's
	 * palette; zeroes for a static mesh); no padding the welding would read.
	 */
	struct FQuantizedVertex
	{
		int32 TexCoord[2] = {0, 0};
		int16 Position[3] = {0, 0, 0};
		int8 Normal[3] = {0, 0, 0};
		uint8 Color[4] = {255, 255, 255, 255};
		uint8 Bones[MaxBoneInfluences] = {0, 0};
		uint8 Weights[MaxBoneInfluences] = {0, 0};
		uint8 Padding[3] = {0, 0, 0};
	};
	static_assert(sizeof(FQuantizedVertex) == 28, "FQuantizedVertex has no implicit padding");

	/** What a build makes: a static mesh's batches, or a skinned mesh's (smaller, each with its palette). */
	struct FBatchLimits
	{
		int32 MaxVertices = FLPS2Mesh::MaxBatchVertices;
		/** 0: no palette (a static mesh). */
		int32 MaxPaletteBones = 0;
	};

	/** A vertex of a strip: the section's welded vertex and its strip flags. */
	struct FStripVertex
	{
		uint32 Vertex = 0;
		uint8 Flags = 0;
	};

	/** The mesh's position quantization: Quantized * Scale + Bias per axis. */
	struct FPositionQuantization
	{
		float Scale[3] = {1.0f, 1.0f, 1.0f};
		float Bias[3] = {0.0f, 0.0f, 0.0f};

		explicit FPositionQuantization(const TArray<FVertex>& Vertices)
		{
			FVector Min(TNumericLimits<float>::Max());
			FVector Max(TNumericLimits<float>::Lowest());
			for (const FVertex& Vertex : Vertices)
			{
				Min = Min.ComponentMin(Vertex.Position);
				Max = Max.ComponentMax(Vertex.Position);
			}
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				// The half extent over the int16 range: the error is at most half a step.
				const float HalfExtent = (Max[Axis] - Min[Axis]) * 0.5f;
				Bias[Axis] = (Min[Axis] + Max[Axis]) * 0.5f;
				Scale[Axis] = HalfExtent > 0.0f ? HalfExtent / float(FLPS2Mesh::PositionRange) : 1.0f;
			}
		}

		[[nodiscard]] int16 Quantize(float Value, int32 Axis) const
		{
			return int16(FMath::Clamp(FMath::RoundToInt((Value - Bias[Axis]) / Scale[Axis]), -FLPS2Mesh::PositionRange,
				FLPS2Mesh::PositionRange));
		}
	};

	[[nodiscard]] FQuantizedVertex QuantizeVertex(
		const FVertex& Source, const FSkinWeightInfo* Skin, const FPositionQuantization& Positions)
	{
		FQuantizedVertex Vertex;
		if (Skin != nullptr)
		{
			Vertex.Bones[0] = Skin->InfluenceBones[0];
			Vertex.Weights[0] = Skin->InfluenceWeights[0];
			Vertex.Weights[1] = Skin->InfluenceWeights[1];
			// An unused second influence names the first bone, so it takes no palette entry.
			Vertex.Bones[1] = Vertex.Weights[1] != 0 ? Skin->InfluenceBones[1] : Skin->InfluenceBones[0];
		}
		const FVector Normal = Source.Normal.GetSafeNormal();
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Vertex.Position[Axis] = Positions.Quantize(Source.Position[Axis], Axis);
			Vertex.Normal[Axis] = int8(FMath::Clamp(FMath::RoundToInt(Normal[Axis] * float(FLPS2Mesh::NormalRange)),
				-FLPS2Mesh::NormalRange, FLPS2Mesh::NormalRange));
		}
		// Far beyond any batch's range, but inside int32: such a coordinate is clamped with a warning.
		constexpr float TexCoordLimit = 1.0e5f;
		Vertex.TexCoord[0] = FMath::RoundToInt(
			FMath::Clamp(Source.TexCoord.X, -TexCoordLimit, TexCoordLimit) * float(FLPS2Mesh::TexCoordOne));
		Vertex.TexCoord[1] = FMath::RoundToInt(
			FMath::Clamp(Source.TexCoord.Y, -TexCoordLimit, TexCoordLimit) * float(FLPS2Mesh::TexCoordOne));
		return Vertex;
	}

	/** The nearest whole number of repeats to Units (4.12), halves away from zero. */
	[[nodiscard]] int32 NearestRepeat(int64 Units)
	{
		const int64 Half = FLPS2Mesh::TexCoordOne / 2;
		return int32(
			Units >= 0 ? (Units + Half) / FLPS2Mesh::TexCoordOne : -((-Units + Half) / FLPS2Mesh::TexCoordOne));
	}

	/** The batches of a mesh as they fill, and the finished ones. */
	class FBatcher
	{
	public:
		/**
		 * A finished batch: its vertices (indices into the section's welded vertices), texture offset and, for a
		 * skinned mesh, its palette (the skeleton's bones in the order the batch first uses them).
		 */
		struct FBatch
		{
			TArray<FStripVertex> Vertices;
			int32 TexCoordOffset[2] = {0, 0};
			TArray<uint8> Palette;
		};

		FBatcher(const TArray<FQuantizedVertex>& InVertices, const FBatchLimits& InLimits)
			: Vertices(InVertices)
			, Limits(InLimits)
		{
		}

		/** Adds a strip: whole to this batch or the next one, else split with the last two vertices repeated. */
		void AddStrip(const TArray<FStripVertex>& Strip)
		{
			const int32 Length = Strip.Num();
			const int32 MaxVertices = Limits.MaxVertices;
			FSpan StripSpan;
			TArray<uint8> StripBones;
			for (const FStripVertex& Vertex : Strip)
			{
				StripSpan.Add(Vertices[Vertex.Vertex]);
				AddBones(Vertices[Vertex.Vertex], StripBones);
			}
			if (Current.Num() + Length <= MaxVertices && Span.Merged(StripSpan).Fits() &&
				FitsPalette(Palette, StripBones))
			{
				AddAll(Strip);
				return;
			}
			if (Length <= MaxVertices && StripSpan.Fits() && FitsPalette(TArray<uint8>(), StripBones))
			{
				Close();
				AddAll(Strip);
				return;
			}
			if (Current.Num() + 3 > MaxVertices)
			{
				Close();
			}
			for (int32 Index = 0; Index < Length; ++Index)
			{
				const FQuantizedVertex& Next = Vertices[Strip[Index].Vertex];
				TArray<uint8> NextBones;
				AddBones(Next, NextBones);
				if (Current.Num() < MaxVertices && Span.With(Next).Fits() && FitsPalette(Palette, NextBones))
				{
					Add(Strip[Index]);
					continue;
				}
				// The strip goes on in a new batch from its last two vertices, which close nothing there.
				Close();
				if (Index >= 2)
				{
					Add(FStripVertex{Strip[Index - 2].Vertex, FLPS2Mesh::FlagNoKick});
					Add(FStripVertex{Strip[Index - 1].Vertex, FLPS2Mesh::FlagNoKick});
				}
				if (!Span.With(Vertices[Strip[Index].Vertex]).Fits())
				{
					// A triangle wider than a batch's texture coordinates reach (N29): the build fails (clamped, its
					// coordinates would make a blob that is not valid).
					bTexCoordOverflow = true;
				}
				Add(Strip[Index]);
			}
		}

		/** Ends the current batch (nothing when it is empty). */
		void Close()
		{
			if (Current.Num() == 0)
			{
				return;
			}
			FBatch& Batch = Batches.AddDefaulted_GetRef();
			Batch.Vertices = MoveTemp(Current);
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				Batch.TexCoordOffset[Axis] = NearestRepeat((int64(Span.Min[Axis]) + int64(Span.Max[Axis])) / 2);
			}
			Batch.Palette = MoveTemp(Palette);
			Current.Reset();
			Palette.Reset();
			Span = FSpan();
		}

		[[nodiscard]] TArray<FBatch>& GetBatches()
		{
			return Batches;
		}

		/** Whether a triangle's texture coordinates spanned more repeats than a batch holds. */
		[[nodiscard]] bool HasTexCoordOverflow() const
		{
			return bTexCoordOverflow;
		}

	private:
		/** The range of texture coordinates of a set of vertices. */
		struct FSpan
		{
			int32 Min[2] = {TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max()};
			int32 Max[2] = {TNumericLimits<int32>::Lowest(), TNumericLimits<int32>::Lowest()};

			void Add(const FQuantizedVertex& Vertex)
			{
				for (int32 Axis = 0; Axis < 2; ++Axis)
				{
					Min[Axis] = FMath::Min(Min[Axis], Vertex.TexCoord[Axis]);
					Max[Axis] = FMath::Max(Max[Axis], Vertex.TexCoord[Axis]);
				}
			}
			[[nodiscard]] FSpan With(const FQuantizedVertex& Vertex) const
			{
				FSpan Result = *this;
				Result.Add(Vertex);
				return Result;
			}
			[[nodiscard]] FSpan Merged(const FSpan& Other) const
			{
				FSpan Result = *this;
				for (int32 Axis = 0; Axis < 2; ++Axis)
				{
					Result.Min[Axis] = FMath::Min(Min[Axis], Other.Min[Axis]);
					Result.Max[Axis] = FMath::Max(Max[Axis], Other.Max[Axis]);
				}
				return Result;
			}
			[[nodiscard]] bool Fits() const
			{
				return int64(Max[0]) - int64(Min[0]) <= MaxTexCoordSpan &&
					int64(Max[1]) - int64(Min[1]) <= MaxTexCoordSpan;
			}
		};

		void Add(const FStripVertex& Vertex)
		{
			Current.Add(Vertex);
			Span.Add(Vertices[Vertex.Vertex]);
			AddBones(Vertices[Vertex.Vertex], Palette);
		}

		/** Adds a vertex's bones to Bones (each once), for a skinned mesh. */
		void AddBones(const FQuantizedVertex& Vertex, TArray<uint8>& Bones) const
		{
			if (Limits.MaxPaletteBones > 0)
			{
				for (const uint8 Bone : Vertex.Bones)
				{
					Bones.AddUnique(Bone);
				}
			}
		}

		/** True when Bones and Extra together fit a palette (always for a static mesh). */
		[[nodiscard]] bool FitsPalette(const TArray<uint8>& Bones, const TArray<uint8>& Extra) const
		{
			if (Limits.MaxPaletteBones <= 0)
			{
				return true;
			}
			int32 Count = Bones.Num();
			for (const uint8 Bone : Extra)
			{
				Count += Bones.Contains(Bone) ? 0 : 1;
			}
			return Count <= Limits.MaxPaletteBones;
		}

		void AddAll(const TArray<FStripVertex>& Strip)
		{
			for (const FStripVertex& Vertex : Strip)
			{
				Add(Vertex);
			}
		}

		const TArray<FQuantizedVertex>& Vertices;
		FBatchLimits Limits;
		bool bTexCoordOverflow = false;
		TArray<FStripVertex> Current;
		FSpan Span;
		TArray<uint8> Palette;
		TArray<FBatch> Batches;
	};

	/** A section's triangles as strips of welded vertices; returns the triangles they draw. */
	int32 MakeSectionStrips(const FMeshData& Source, const TArray<FSkinWeightInfo>* SkinWeights,
		const FMeshSection& Section, const FPositionQuantization& Positions, TArray<FQuantizedVertex>& OutVertices,
		TArray<TArray<FStripVertex>>& OutStrips)
	{
		const int32 NumCorners = Section.IndexCount - (Section.IndexCount % 3);
		TArray<FQuantizedVertex> Corners;
		Corners.Reserve(NumCorners);
		for (int32 Corner = 0; Corner < NumCorners; ++Corner)
		{
			const int32 VertexIndex = int32(Source.Indices[Section.IndexOffset + Corner]);
			Corners.Add(QuantizeVertex(Source.Vertices[VertexIndex],
				SkinWeights != nullptr ? &(*SkinWeights)[VertexIndex] : nullptr, Positions));
		}
		if (NumCorners == 0)
		{
			return 0;
		}

		// Weld the corners that quantize to the same vertex (in the order they first appear).
		TArray<uint32> Remap;
		Remap.SetNumUninitialized(NumCorners);
		const size_t NumVertices = meshopt_generateVertexRemap(Remap.GetData(), nullptr, size_t(NumCorners),
			Corners.GetData(), size_t(NumCorners), sizeof(FQuantizedVertex));
		OutVertices.SetNumUninitialized(int32(NumVertices));
		meshopt_remapVertexBuffer(
			OutVertices.GetData(), Corners.GetData(), size_t(NumCorners), sizeof(FQuantizedVertex), Remap.GetData());

		// The triangles that still have three corners: the others draw nothing.
		TArray<uint32> Triangles;
		Triangles.Reserve(NumCorners);
		for (int32 Corner = 0; Corner < NumCorners; Corner += 3)
		{
			const uint32 A = Remap[Corner];
			const uint32 B = Remap[Corner + 1];
			const uint32 C = Remap[Corner + 2];
			if (A != B && B != C && A != C)
			{
				Triangles.Append({A, B, C});
			}
		}
		if (Triangles.Num() == 0)
		{
			return 0;
		}

		TArray<uint32> Ordered;
		Ordered.SetNumUninitialized(Triangles.Num());
		meshopt_optimizeVertexCacheStrip(Ordered.GetData(), Triangles.GetData(), size_t(Triangles.Num()), NumVertices);
		TArray<uint32> Stream;
		Stream.SetNumUninitialized(int32(meshopt_stripifyBound(size_t(Ordered.Num()))));
		const size_t StreamLength =
			meshopt_stripify(Stream.GetData(), Ordered.GetData(), size_t(Ordered.Num()), NumVertices, RestartIndex);

		// Each strip's flags: its first two vertices and those closing a degenerate triangle (a swap) close nothing;
		// the others close an odd triangle reversed (meshopt_unstripify's parity).
		int32 NumTriangles = 0;
		size_t Start = 0;
		while (Start < StreamLength)
		{
			size_t End = Start;
			while (End < StreamLength && Stream[int32(End)] != RestartIndex)
			{
				++End;
			}
			TArray<FStripVertex>& Strip = OutStrips.AddDefaulted_GetRef();
			for (size_t Position = Start; Position < End; ++Position)
			{
				FStripVertex Vertex;
				Vertex.Vertex = Stream[int32(Position)];
				const size_t InStrip = Position - Start;
				if (InStrip < 2)
				{
					Vertex.Flags = FLPS2Mesh::FlagNoKick;
				}
				else
				{
					const uint32 A = Stream[int32(Position - 2)];
					const uint32 B = Stream[int32(Position - 1)];
					if (A == B || B == Vertex.Vertex || A == Vertex.Vertex)
					{
						Vertex.Flags = FLPS2Mesh::FlagNoKick;
					}
					else
					{
						Vertex.Flags = (InStrip & 1) != 0 ? FLPS2Mesh::FlagReversed : 0;
						++NumTriangles;
					}
				}
				Strip.Add(Vertex);
			}
			Start = End + 1;
		}
		return NumTriangles;
	}

	/** Appends the bytes of Record to Blob. */
	template <typename RecordType>
	void AppendRecord(TArray<uint8>& Blob, const RecordType& Record)
	{
		Blob.Append(reinterpret_cast<const uint8*>(&Record), int32(sizeof(RecordType)));
	}

	void PadToQuadword(TArray<uint8>& Blob)
	{
		Blob.AddZeroed(FLPS2Mesh::AlignQuadword(Blob.Num()) - Blob.Num());
	}

	/** The sphere around the batch's dequantized positions. */
	void ComputeBatchBounds(const TArray<FVector>& Points, FLPS2Batch& OutBatch)
	{
		FVector Min(TNumericLimits<float>::Max());
		FVector Max(TNumericLimits<float>::Lowest());
		for (const FVector& Point : Points)
		{
			Min = Min.ComponentMin(Point);
			Max = Max.ComponentMax(Point);
		}
		const FVector Center = (Min + Max) * 0.5f;
		float RadiusSquared = 0.0f;
		for (const FVector& Point : Points)
		{
			RadiusSquared = FMath::Max(RadiusSquared, FVector::DistSquared(Point, Center));
		}
		OutBatch.BoundsCenter[0] = Center.X;
		OutBatch.BoundsCenter[1] = Center.Y;
		OutBatch.BoundsCenter[2] = Center.Z;
		// A hair larger, so that the float rounding of a transform never puts a vertex outside it.
		OutBatch.BoundsRadius = (FMath::Sqrt(RadiusSquared) * 1.0001f) + 0.01f;
	}

	/** The build of Build and BuildSkinned: SkinWeights (one per vertex) makes a skinned mesh's blob. */
	bool BuildBlob(
		const FMeshData& Source, const TArray<FSkinWeightInfo>* SkinWeights, FLPS2Mesh& OutMesh, FString& OutError)
	{
		if (Source.IsEmpty())
		{
			OutError = TEXT("the mesh has no triangles");
			return false;
		}
		for (const uint32 Index : Source.Indices)
		{
			if (Index >= uint32(Source.Vertices.Num()))
			{
				OutError = FString::Printf("index %u is out of range (%d vertices)", Index, Source.Vertices.Num());
				return false;
			}
		}
		TArray<FMeshSection> Sections = Source.Submeshes;
		if (Sections.Num() == 0)
		{
			Sections.Add(FMeshSection{0, Source.Indices.Num(), 0});
		}
		for (const FMeshSection& Section : Sections)
		{
			if (Section.IndexOffset < 0 || Section.IndexCount < 0 ||
				int64(Section.IndexOffset) + int64(Section.IndexCount) > int64(Source.Indices.Num()) ||
				Section.MaterialIndex < 0)
			{
				OutError = FString::Printf("a section's range (%d + %d) is outside the %d indices", Section.IndexOffset,
					Section.IndexCount, Source.Indices.Num());
				return false;
			}
		}

		if (SkinWeights != nullptr)
		{
			if (SkinWeights->Num() != Source.Vertices.Num())
			{
				OutError =
					FString::Printf("%d skin weights for %d vertices", SkinWeights->Num(), Source.Vertices.Num());
				return false;
			}
			for (const FSkinWeightInfo& Skin : *SkinWeights)
			{
				if (int32(Skin.InfluenceWeights[0]) + int32(Skin.InfluenceWeights[1]) != 255 ||
					Skin.InfluenceBones[0] >= MaxSkinBones || Skin.InfluenceBones[1] >= MaxSkinBones)
				{
					OutError =
						TEXT("a vertex's skin weights do not add up to 255 or name a bone past the skeleton's limit");
					return false;
				}
			}
		}
		const bool bSkinned = SkinWeights != nullptr;
		FBatchLimits Limits;
		if (bSkinned)
		{
			Limits.MaxVertices = FLPS2Mesh::MaxSkinnedBatchVertices;
			Limits.MaxPaletteBones = FLPS2Mesh::MaxPaletteBones;
		}

		const FPositionQuantization Positions(Source.Vertices);

		// Each section's welded vertices and batches.
		struct FSectionBatches
		{
			TArray<FQuantizedVertex> Vertices;
			TArray<FBatcher::FBatch> Batches;
			int32 NumTriangles = 0;
		};
		TArray<FSectionBatches> Built;
		Built.SetNum(Sections.Num());
		int32 NumBatches = 0;
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			FSectionBatches& Section = Built[SectionIndex];
			TArray<TArray<FStripVertex>> Strips;
			Section.NumTriangles =
				MakeSectionStrips(Source, SkinWeights, Sections[SectionIndex], Positions, Section.Vertices, Strips);
			FBatcher Batcher(Section.Vertices, Limits);
			for (const TArray<FStripVertex>& Strip : Strips)
			{
				Batcher.AddStrip(Strip);
			}
			Batcher.Close();
			if (Batcher.HasTexCoordOverflow())
			{
				OutError =
					FString::Printf("a triangle of section %d has texture coordinates that span more than %d "
									"repeats, more than a batch's int16 4.12 coordinates hold around its offset: "
									"cut the face or its UVs",
						SectionIndex, MaxTexCoordSpan / FLPS2Mesh::TexCoordOne);
				return false;
			}
			Section.Batches = MoveTemp(Batcher.GetBatches());
			NumBatches += Section.Batches.Num();
		}

		// The blob: the header, the tables, then each batch's streams.
		FLPS2MeshHeader Header;
		Header.Magic = FLPS2Mesh::Magic;
		Header.Version = FLPS2Mesh::Version;
		Header.Flags = bSkinned ? FLPS2Mesh::FlagSkinned : 0;
		Header.NumSections = uint32(Sections.Num());
		Header.NumBatches = uint32(NumBatches);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Header.PositionScale[Axis] = Positions.Scale[Axis];
			Header.PositionBias[Axis] = Positions.Bias[Axis];
		}
		TArray<FLPS2Section> SectionTable;
		TArray<FLPS2Batch> BatchTable;
		TArray<uint8> Streams;
		int32 DataOffset = FLPS2Mesh::BatchTableOffset(Sections.Num()) + (NumBatches * int32(sizeof(FLPS2Batch)));
		TArray<FVector> BatchPoints;
		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			const FSectionBatches& Section = Built[SectionIndex];
			FLPS2Section& Record = SectionTable.AddDefaulted_GetRef();
			Record.FirstBatch = uint32(BatchTable.Num());
			Record.NumBatches = uint32(Section.Batches.Num());
			Record.MaterialIndex = uint32(Sections[SectionIndex].MaterialIndex);
			Record.NumTriangles = uint32(Section.NumTriangles);
			Header.NumTriangles += uint32(Section.NumTriangles);
			for (const FBatcher::FBatch& Batch : Section.Batches)
			{
				const int32 Count = Batch.Vertices.Num();
				FLPS2Batch& BatchRecord = BatchTable.AddDefaulted_GetRef();
				BatchRecord.DataOffset = uint32(DataOffset);
				BatchRecord.NumVertices = uint32(Count);
				BatchRecord.TexCoordOffset[0] = float(Batch.TexCoordOffset[0]);
				BatchRecord.TexCoordOffset[1] = float(Batch.TexCoordOffset[1]);
				Header.NumVertices += uint32(Count);

				// A skinned batch's palette, then positions (V3-16), normals and flags (V4-8), colours (V4-8), texture
				// coordinates (V2-16) and a skinned batch's skin (V4-8).
				if (bSkinned)
				{
					FLPS2SkinPalette Palette;
					Palette.NumBones = uint32(Batch.Palette.Num());
					for (int32 Bone = 0; Bone < Batch.Palette.Num(); ++Bone)
					{
						Palette.Bones[Bone] = Batch.Palette[Bone];
					}
					AppendRecord(Streams, Palette);
				}
				BatchPoints.Reset();
				for (const FStripVertex& Vertex : Batch.Vertices)
				{
					const FQuantizedVertex& Quantized = Section.Vertices[int32(Vertex.Vertex)];
					Streams.Append(
						reinterpret_cast<const uint8*>(Quantized.Position), int32(sizeof(Quantized.Position)));
					BatchPoints.Add(
						FVector((float(Quantized.Position[0]) * Header.PositionScale[0]) + Header.PositionBias[0],
							(float(Quantized.Position[1]) * Header.PositionScale[1]) + Header.PositionBias[1],
							(float(Quantized.Position[2]) * Header.PositionScale[2]) + Header.PositionBias[2]));
				}
				PadToQuadword(Streams);
				for (const FStripVertex& Vertex : Batch.Vertices)
				{
					const FQuantizedVertex& Quantized = Section.Vertices[int32(Vertex.Vertex)];
					Streams.Append(reinterpret_cast<const uint8*>(Quantized.Normal), int32(sizeof(Quantized.Normal)));
					Streams.Add(Vertex.Flags);
				}
				PadToQuadword(Streams);
				for (const FStripVertex& Vertex : Batch.Vertices)
				{
					Streams.Append(Section.Vertices[int32(Vertex.Vertex)].Color, 4);
				}
				PadToQuadword(Streams);
				for (const FStripVertex& Vertex : Batch.Vertices)
				{
					const FQuantizedVertex& Quantized = Section.Vertices[int32(Vertex.Vertex)];
					for (int32 Axis = 0; Axis < 2; ++Axis)
					{
						const int64 Relative = int64(Quantized.TexCoord[Axis]) -
							(int64(Batch.TexCoordOffset[Axis]) * FLPS2Mesh::TexCoordOne);
						const int16 Value = int16(FMath::Clamp(
							Relative, int64(TNumericLimits<int16>::Lowest()), int64(TNumericLimits<int16>::Max())));
						AppendRecord(Streams, Value);
					}
				}
				PadToQuadword(Streams);
				if (bSkinned)
				{
					for (const FStripVertex& Vertex : Batch.Vertices)
					{
						const FQuantizedVertex& Quantized = Section.Vertices[int32(Vertex.Vertex)];
						Streams.Add(uint8(Batch.Palette.IndexOfByKey(Quantized.Bones[0])));
						Streams.Add(uint8(Batch.Palette.IndexOfByKey(Quantized.Bones[1])));
						Streams.Add(Quantized.Weights[0]);
						Streams.Add(Quantized.Weights[1]);
					}
					PadToQuadword(Streams);
				}
				DataOffset += FLPS2Mesh::BatchBytes(Count, bSkinned);
				ComputeBatchBounds(BatchPoints, BatchRecord);
			}
		}

		TArray<uint8> Blob;
		AppendRecord(Blob, Header);
		for (const FLPS2Section& Record : SectionTable)
		{
			AppendRecord(Blob, Record);
		}
		for (const FLPS2Batch& Record : BatchTable)
		{
			AppendRecord(Blob, Record);
		}
		Blob.Append(Streams);
		if (Header.NumTriangles == 0)
		{
			OutError = TEXT("every triangle of the mesh is degenerate");
			return false;
		}
		if (!OutMesh.SetData(MoveTemp(Blob)))
		{
			OutError = TEXT("the LPS2 v2 blob it made is not valid");
			return false;
		}
		return true;
	}

} // namespace

bool FLPS2MeshBuilder::Build(const FMeshData& Source, FLPS2Mesh& OutMesh, FString& OutError)
{
	return BuildBlob(Source, nullptr, OutMesh, OutError);
}

bool FLPS2MeshBuilder::Simplify(
	const FMeshData& Source, float PercentTriangles, FMeshData& OutSimplified, FString& OutError)
{
	if (Source.IsEmpty() || (Source.Indices.Num() % 3) != 0)
	{
		OutError = TEXT("no triangles to simplify");
		return false;
	}
	OutSimplified = Source;
	OutSimplified.Indices.Reset();
	OutSimplified.Submeshes.Reset();
	TArray<FMeshSection> Sections = Source.Submeshes;
	if (Sections.Num() == 0)
	{
		FMeshSection& All = Sections.AddDefaulted_GetRef();
		All.IndexCount = Source.Indices.Num();
	}
	TArray<uint32> Simplified;
	for (const FMeshSection& Section : Sections)
	{
		if (Section.IndexOffset < 0 || Section.IndexCount < 3 ||
			Section.IndexOffset + Section.IndexCount > Source.Indices.Num())
		{
			OutError = TEXT("a section's indices are out of range");
			return false;
		}
		// meshopt_simplify collapses edges down to the target count, with no bound on the error (UE's MaxDeviation 0:
		// the screen size decides when a LOD is small enough to hide it); its attribute seams (the same position twice)
		// and borders stay, so a section may keep more than it was asked to.
		const size_t NumIndices = size_t(Section.IndexCount);
		const size_t Target = FMath::Max<size_t>(3, size_t(float(NumIndices / 3) * PercentTriangles) * 3);
		constexpr float TargetError = 1.0f;
		Simplified.SetNumUninitialized(int32(NumIndices));
		float ResultError = 0.0f;
		const size_t Kept = meshopt_simplify(Simplified.GetData(), Source.Indices.GetData() + Section.IndexOffset,
			NumIndices, &Source.Vertices[0].Position.X, size_t(Source.Vertices.Num()), sizeof(FVertex), Target,
			TargetError, 0, &ResultError);
		FMeshSection& Out = OutSimplified.Submeshes.AddDefaulted_GetRef();
		Out.MaterialIndex = Section.MaterialIndex;
		Out.IndexOffset = OutSimplified.Indices.Num();
		if (Kept < 3)
		{
			// Nothing left: the section as it is (a LOD never loses a section).
			OutSimplified.Indices.Append(Source.Indices.GetData() + Section.IndexOffset, Section.IndexCount);
		}
		else
		{
			OutSimplified.Indices.Append(Simplified.GetData(), int32(Kept));
		}
		Out.IndexCount = OutSimplified.Indices.Num() - Out.IndexOffset;
	}
	return true;
}

bool FLPS2MeshBuilder::BuildSkinned(
	const FMeshData& Source, const TArray<FSkinWeightInfo>& SkinWeights, FLPS2Mesh& OutMesh, FString& OutError)
{
	return BuildBlob(Source, &SkinWeights, OutMesh, OutError);
}
