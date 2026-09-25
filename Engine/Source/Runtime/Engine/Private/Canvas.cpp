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
		Scratch.SetNumUninitialized(Count + 1);
		FMemory::Memcpy(Scratch.GetData(), Begin, Count);
		Scratch[Count] = '\0';
		return Scratch.GetData();
	}

	/** Calls Visit(Begin, Count) for every '\n'-separated line of Text (including empty ones). */
	template <typename VisitorType>
	void ForEachLine(const FString& Text, const VisitorType& Visit)
	{
		const ANSICHAR* Cursor = *Text;
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

	float RawTextWidth(const ANSICHAR* Begin, int32 Count)
	{
		TArray<ANSICHAR> Scratch;
		return static_cast<float>(stb_easy_font_width(TerminatedCopy(Scratch, Begin, Count)));
	}

	void AppendTextMesh(TArray<FCanvasVertex>& Tris, const ANSICHAR* InText, int32 Count, float OriginX, float OriginY,
		float InPixelScale, const FColor& InColor)
	{
		if (Count <= 0)
		{
			return;
		}

		TArray<ANSICHAR> MutableText;
		TArray<ANSICHAR> FontBuf;
		FontBuf.SetNumZeroed(Count * 300 + 64);
		uint8 ColorCopy[4] = {InColor.R, InColor.G, InColor.B, InColor.A};
		const int32 Quads = stb_easy_font_print(
			0.0f, 0.0f, TerminatedCopy(MutableText, InText, Count), ColorCopy, FontBuf.GetData(), FontBuf.Num());
		if (Quads <= 0)
		{
			return;
		}

		const auto* Packed = reinterpret_cast<const FPackedVert*>(FontBuf.GetData());
		auto Push = [&](const FPackedVert& V)
		{
			FCanvasVertex Out{};
			Out.X = (V.X * InPixelScale) + OriginX;
			Out.Y = (V.Y * InPixelScale) + OriginY;
			Out.Z = 0.0f;
			Out.R = InColor.R / 255.0f;
			Out.G = InColor.G / 255.0f;
			Out.B = InColor.B / 255.0f;
			Out.A = InColor.A / 255.0f;
			Tris.Add(Out);
		};

		for (int32 Q = 0; Q < Quads; ++Q)
		{
			Push(Packed[(Q * 4) + 0]);
			Push(Packed[(Q * 4) + 1]);
			Push(Packed[(Q * 4) + 2]);
			Push(Packed[(Q * 4) + 0]);
			Push(Packed[(Q * 4) + 2]);
			Push(Packed[(Q * 4) + 3]);
		}
	}

	/** Draws multiline text. AnchorX is the left / center / right of each line per InJustify. */
	void AppendJustifiedLines(TArray<FCanvasVertex>& Tris, const FString& InText, float AnchorX, float OriginY,
		float InPixelScale, ETextJustify InJustify, const FColor& InColor)
	{
		if (InText.IsEmpty())
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
					AppendTextMesh(Tris, Line, Count, OriginX, LocalY, InPixelScale, InColor);
				}
				LocalY += 14.0f * InPixelScale;
			});
	}

	void AppendScreenQuad(TArray<FCanvasVertex>& Tris, float InX0, float InY0, float InX1, float InY1, float X2,
		float Y2, float X3, float Y3, const FLinearColor& InColor)
	{
		auto Push = [&](float InX, float InY)
		{
			FCanvasVertex Out{};
			Out.X = InX;
			Out.Y = InY;
			Out.Z = 0.0f;
			Out.R = InColor.R;
			Out.G = InColor.G;
			Out.B = InColor.B;
			Out.A = 1.0f;
			Tris.Add(Out);
		};
		Push(InX0, InY0);
		Push(InX1, InY1);
		Push(X2, Y2);
		Push(InX0, InY0);
		Push(X2, Y2);
		Push(X3, Y3);
	}

	void AppendThickScreenLine(TArray<FCanvasVertex>& Tris, float InX0, float InY0, float InX1, float InY1,
		float InThickness, const FLinearColor& InColor)
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
		AppendScreenQuad(
			Tris, InX0 - Hx, InY0 - Hy, InX0 + Hx, InY0 + Hy, InX1 + Hx, InY1 + Hy, InX1 - Hx, InY1 - Hy, InColor);
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
	: SizeX(InSizeX)
	, SizeY(InSizeY)
{
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
	GetBatch().Texts.Add(FTextItem{Text, X, Y, Scale, Justify, Color, false});
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
	GetBatch().Texts.Add(FTextItem{Text, X, Y, Scale, ETextJustify::Left, Color, true});
}

void FCanvas::MeasureText(const FString& Text, float Scale, float& OutWidth, float& OutHeight)
{
	float MaxRawWidth = 0.0f;
	int32 LineCount = 0;
	ForEachLine(Text,
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

void FCanvas::GetTriangles(TArray<FCanvasVertex>& OutVertices) const
{
	OutVertices.Reset();
	for (const FBatch& Batch : Batches)
	{
		// Panels first, then lines, then labels on top.
		for (const FTileItem& Tile : Batch.Tiles)
		{
			AppendScreenQuad(OutVertices, Tile.X, Tile.Y, Tile.X + Tile.SizeX, Tile.Y, Tile.X + Tile.SizeX,
				Tile.Y + Tile.SizeY, Tile.X, Tile.Y + Tile.SizeY, Tile.Color);
		}
		for (const FLineItem& Line : Batch.Lines)
		{
			AppendThickScreenLine(OutVertices, Line.X0, Line.Y0, Line.X1, Line.Y1, Line.Thickness, Line.Color);
		}
		for (const FTextItem& Text : Batch.Texts)
		{
			if (Text.bBlock)
			{
				AppendTextMesh(OutVertices, *Text.Text, Text.Text.Len(), Text.X, Text.Y, Text.Scale, Text.Color);
			}
			else
			{
				AppendJustifiedLines(OutVertices, Text.Text, Text.X, Text.Y, Text.Scale, Text.Justify, Text.Color);
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
