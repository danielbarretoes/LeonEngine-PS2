#include "CanvasTypes.h"
#include "RendererInterface.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4505) // unused statics in stb_easy_font.h
#endif
#include <stb_easy_font.h>
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

namespace
{

	struct FPackedVert
	{
		float X, Y, Z;
		uint8 Rgba[4];
	};

	/** stb_easy_font takes mutable, null-terminated text: copy [Begin, Begin + Count) into Scratch. */
	ANSICHAR* TerminatedCopy(TArray<ANSICHAR>& Scratch, const ANSICHAR* Begin, int32 Count)
	{
		Scratch.SetNumUninitialized(Count + 1, false);
		FMemory::Memcpy(Scratch.GetData(), Begin, Count);
		Scratch[Count] = '\0';
		return Scratch.GetData();
	}

	/** Calls Visit(Begin, Count) for every '\n'-separated line of Text (including empty ones). */
	template <typename VisitorType>
	void ForEachLine(const TCHAR* Text, const VisitorType& Visit)
	{
		const ANSICHAR* Cursor = Text;
		for (;;)
		{
			const ANSICHAR* End = Cursor;
			while (*End != '\0' && *End != '\n')
			{
				++End;
			}
			Visit(Cursor, static_cast<int32>(End - Cursor));
			if (*End == '\0')
			{
				break;
			}
			Cursor = End + 1;
		}
	}

	/**
	 * The text scratch buffers, kept between frames (the canvas draws on the game thread only): every label of every
	 * frame goes through them, and on the PS2 an allocation and a zeroed 300 bytes a character each time cost.
	 */
	TArray<ANSICHAR>& GetTextScratch()
	{
		static TArray<ANSICHAR> Scratch;
		return Scratch;
	}

	TArray<ANSICHAR>& GetFontScratch()
	{
		static TArray<ANSICHAR> FontBuf;
		return FontBuf;
	}

	float RawTextWidth(const ANSICHAR* Begin, int32 Count)
	{
		return static_cast<float>(stb_easy_font_width(TerminatedCopy(GetTextScratch(), Begin, Count)));
	}

	/** The canvas's vertices and runs as they are made: a new run where the primitive type changes. */
	struct FPrimitiveSink
	{
		TArray<FCanvasVertex>& Vertices;
		TArray<FCanvasPrimitiveRun>& Runs;

		void Begin(ECanvasPrimitive Type)
		{
			if (Runs.Num() == 0 || Runs.Last().Type != Type)
			{
				FCanvasPrimitiveRun& Run = Runs.AddDefaulted_GetRef();
				Run.Type = Type;
				Run.FirstVertex = Vertices.Num();
			}
		}

		void Add(const FCanvasVertex& Vertex)
		{
			Vertices.Add(Vertex);
			++Runs.Last().NumVertices;
		}

		/** A rectangle between two opposite corners (any order): its top-left and bottom-right, in Color. */
		void AddRectangle(float InX0, float InY0, float InX1, float InY1, const FCanvasVertex& Color)
		{
			Begin(ECanvasPrimitive::Rectangle);
			FCanvasVertex TopLeft = Color;
			TopLeft.X = FMath::Min(InX0, InX1);
			TopLeft.Y = FMath::Min(InY0, InY1);
			TopLeft.Z = 0.0f;
			FCanvasVertex BottomRight = Color;
			BottomRight.X = FMath::Max(InX0, InX1);
			BottomRight.Y = FMath::Max(InY0, InY1);
			BottomRight.Z = 0.0f;
			Add(TopLeft);
			Add(BottomRight);
		}
	};

	/** A linear colour as a vertex's, opaque. */
	[[nodiscard]] FCanvasVertex OpaqueVertex(const FLinearColor& InColor)
	{
		FCanvasVertex Out{};
		Out.R = InColor.R;
		Out.G = InColor.G;
		Out.B = InColor.B;
		Out.A = 1.0f;
		return Out;
	}

	void AppendTextMesh(FPrimitiveSink& Sink, const ANSICHAR* InText, int32 Count, float OriginX, float OriginY,
		float InPixelScale, const FColor& InColor)
	{
		if (Count <= 0)
		{
			return;
		}

		// stb_easy_font writes the quads it makes (at most 300 bytes a character); the rest of the buffer is not read.
		TArray<ANSICHAR>& FontBuf = GetFontScratch();
		FontBuf.SetNumUninitialized(Count * 300 + 64, false);
		uint8 ColorCopy[4] = {InColor.R, InColor.G, InColor.B, InColor.A};
		const int32 Quads = stb_easy_font_print(
			0.0f, 0.0f, TerminatedCopy(GetTextScratch(), InText, Count), ColorCopy, FontBuf.GetData(), FontBuf.Num());
		if (Quads <= 0)
		{
			return;
		}

		const auto* Packed = reinterpret_cast<const FPackedVert*>(FontBuf.GetData());
		// One colour for the whole label: converted once, not per vertex.
		FCanvasVertex Colored{};
		Colored.R = InColor.R / 255.0f;
		Colored.G = InColor.G / 255.0f;
		Colored.B = InColor.B / 255.0f;
		Colored.A = InColor.A / 255.0f;
		Sink.Vertices.Reserve(Sink.Vertices.Num() + (Quads * 2));
		// Each of the font's quads is an axis-aligned bar (corners 0 and 2 opposite): a rectangle, the GS's sprite.
		for (int32 Q = 0; Q < Quads; ++Q)
		{
			const FPackedVert& First = Packed[(Q * 4) + 0];
			const FPackedVert& Opposite = Packed[(Q * 4) + 2];
			Sink.AddRectangle((First.X * InPixelScale) + OriginX, (First.Y * InPixelScale) + OriginY,
				(Opposite.X * InPixelScale) + OriginX, (Opposite.Y * InPixelScale) + OriginY, Colored);
		}
	}

	/** Draws multiline text. AnchorX is the left / center / right of each line per InJustify. */
	void AppendJustifiedLines(FPrimitiveSink& Sink, const TCHAR* InText, float AnchorX, float OriginY,
		float InPixelScale, ETextJustify InJustify, const FColor& InColor)
	{
		if (InText == nullptr || InText[0] == '\0')
		{
			return;
		}

		float LocalY = OriginY;
		ForEachLine(InText,
			[&](const ANSICHAR* Line, int32 Count)
			{
				if (Count > 0)
				{
					const float LineW = RawTextWidth(Line, Count) * InPixelScale;
					float OriginX = AnchorX;
					if (InJustify == ETextJustify::Center)
					{
						OriginX = AnchorX - (LineW * 0.5f);
					}
					else if (InJustify == ETextJustify::Right)
					{
						OriginX = AnchorX - LineW;
					}
					AppendTextMesh(Sink, Line, Count, OriginX, LocalY, InPixelScale, InColor);
				}
				LocalY += 14.0f * InPixelScale;
			});
	}

	/**
	 * A line Thickness wide: along an axis a rectangle, else two triangles of the quad around it (the corners 0, 1, 2
	 * and 0, 2, 3, in the quad's order).
	 */
	void AppendThickScreenLine(FPrimitiveSink& Sink, float InX0, float InY0, float InX1, float InY1, float InThickness,
		const FLinearColor& InColor)
	{
		const float Dx = InX1 - InX0;
		const float Dy = InY1 - InY0;
		const float Len = FMath::Sqrt((Dx * Dx) + (Dy * Dy));
		if (Len < 1.0e-4f)
		{
			return;
		}
		const float Hx = (-Dy / Len) * (InThickness * 0.5f);
		const float Hy = (Dx / Len) * (InThickness * 0.5f);
		const FCanvasVertex Color = OpaqueVertex(InColor);
		if (Dx == 0.0f || Dy == 0.0f)
		{
			Sink.AddRectangle(InX0 - Hx, InY0 - Hy, InX1 + Hx, InY1 + Hy, Color);
			return;
		}
		const float Corners[4][2] = {
			{InX0 - Hx, InY0 - Hy}, {InX0 + Hx, InY0 + Hy}, {InX1 + Hx, InY1 + Hy}, {InX1 - Hx, InY1 - Hy}};
		constexpr int32 Order[6] = {0, 1, 2, 0, 2, 3};
		Sink.Begin(ECanvasPrimitive::Triangle);
		for (const int32 Corner : Order)
		{
			FCanvasVertex Out = Color;
			Out.X = Corners[Corner][0];
			Out.Y = Corners[Corner][1];
			Out.Z = 0.0f;
			Sink.Add(Out);
		}
	}

	/** A linear colour as the font's 8-bit colour, opaque (each channel clamped to [0, 1]). */
	[[nodiscard]] FColor OpaqueColor(const FLinearColor& Rgb)
	{
		return FColor(static_cast<uint8>(FMath::Clamp(Rgb.R, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(FMath::Clamp(Rgb.G, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(FMath::Clamp(Rgb.B, 0.0f, 1.0f) * 255.0f), 255);
	}

} // namespace

FCanvas::FCanvas(int32 InSizeX, int32 InSizeY)
	: MemMark(FMemStack::Get())
	, SizeX(InSizeX)
	, SizeY(InSizeY)
{
}

const TCHAR* FCanvas::CopyText(const FString& Text)
{
	const SIZE_T Bytes = SIZE_T(Text.Len() + 1) * sizeof(TCHAR);
	TCHAR* Copy = reinterpret_cast<TCHAR*>(FMemStack::Get().PushBytes(Bytes, alignof(TCHAR)));
	FMemory::Memcpy(Copy, *Text, Bytes);
	return Copy;
}

void FCanvas::PushDepthSortKey(int32 Key)
{
	DepthSortKeyStack.Add(Key);
}

void FCanvas::PopDepthSortKey()
{
	if (DepthSortKeyStack.Num() > 0)
	{
		DepthSortKeyStack.Pop();
	}
}

int32 FCanvas::GetDepthSortKey() const
{
	return DepthSortKeyStack.Num() > 0 ? DepthSortKeyStack.Last() : 0;
}

FCanvas::FBatch& FCanvas::GetBatch()
{
	const int32 Key = GetDepthSortKey();
	// Batches stay sorted: higher keys first.
	int32 Index = 0;
	while (Index < Batches.Num() && Batches[Index].DepthSortKey > Key)
	{
		++Index;
	}
	if (Index < Batches.Num() && Batches[Index].DepthSortKey == Key)
	{
		return Batches[Index];
	}
	FBatch NewBatch;
	NewBatch.DepthSortKey = Key;
	Batches.Insert(MoveTemp(NewBatch), Index);
	return Batches[Index];
}

void FCanvas::DrawTile(float X, float Y, float InSizeX, float InSizeY, const FLinearColor& Color)
{
	GetBatch().Tiles.Add(FTileItem{X, Y, InSizeX, InSizeY, Color});
}

void FCanvas::DrawLine(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness)
{
	GetBatch().Lines.Add(FLineItem{X0, Y0, X1, Y1, Thickness, Color});
}

void FCanvas::DrawText(const FString& Text, float X, float Y, const FColor& Color, float Scale, ETextJustify Justify)
{
	if (Text.IsEmpty())
	{
		return;
	}
	GetBatch().Texts.Add(FTextItem{CopyText(Text), Text.Len(), X, Y, Scale, Justify, Color, false});
}

void FCanvas::DrawText(
	const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, ETextJustify Justify)
{
	DrawText(Text, X, Y, OpaqueColor(Color), Scale, Justify);
}

void FCanvas::DrawTextBlock(const FString& Text, float X, float Y, const FColor& Color, float Scale)
{
	if (Text.IsEmpty())
	{
		return;
	}
	GetBatch().Texts.Add(FTextItem{CopyText(Text), Text.Len(), X, Y, Scale, ETextJustify::Left, Color, true});
}

void FCanvas::MeasureText(const FString& Text, float Scale, float& OutWidth, float& OutHeight)
{
	float MaxRawWidth = 0.0f;
	int32 LineCount = 0;
	ForEachLine(*Text,
		[&](const ANSICHAR* Line, int32 Count)
		{
			if (Count > 0)
			{
				MaxRawWidth = FMath::Max(MaxRawWidth, RawTextWidth(Line, Count));
			}
			++LineCount;
		});
	OutWidth = MaxRawWidth * Scale;
	OutHeight = 14.0f * Scale * static_cast<float>(LineCount);
}

bool FCanvas::IsEmpty() const
{
	for (const FBatch& Batch : Batches)
	{
		if (Batch.Tiles.Num() > 0 || Batch.Lines.Num() > 0 || Batch.Texts.Num() > 0)
		{
			return false;
		}
	}
	return true;
}

void FCanvas::GetPrimitives(TArray<FCanvasVertex>& OutVertices, TArray<FCanvasPrimitiveRun>& OutRuns) const
{
	OutVertices.Reset();
	OutRuns.Reset();
	FPrimitiveSink Sink{OutVertices, OutRuns};
	for (const FBatch& Batch : Batches)
	{
		// Panels first, then lines, then labels on top.
		for (const FTileItem& Tile : Batch.Tiles)
		{
			Sink.AddRectangle(Tile.X, Tile.Y, Tile.X + Tile.SizeX, Tile.Y + Tile.SizeY, OpaqueVertex(Tile.Color));
		}
		for (const FLineItem& Line : Batch.Lines)
		{
			AppendThickScreenLine(Sink, Line.X0, Line.Y0, Line.X1, Line.Y1, Line.Thickness, Line.Color);
		}
		for (const FTextItem& Text : Batch.Texts)
		{
			if (Text.bBlock)
			{
				AppendTextMesh(Sink, Text.Text, Text.Len, Text.X, Text.Y, Text.Scale, Text.Color);
			}
			else
			{
				AppendJustifiedLines(Sink, Text.Text, Text.X, Text.Y, Text.Scale, Text.Justify, Text.Color);
			}
		}
	}
}

void FCanvas::Flush_GameThread()
{
	if (IRendererModule* RendererModule = GetRendererModulePtr())
	{
		RendererModule->DrawCanvas(*this);
	}
}
