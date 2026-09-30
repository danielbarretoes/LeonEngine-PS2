#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "RendererInterface.h"

namespace
{

	/** The canvas's vertices and runs as they are made: a new run where the primitive type or the texture changes. */
	struct FPrimitiveSink
	{
		TArray<FCanvasVertex>& Vertices;
		TArray<FCanvasPrimitiveRun>& Runs;

		void Begin(ECanvasPrimitive Type, const UTexture2D* Texture = nullptr, bool bNearest = false)
		{
			if (Runs.Num() == 0 || Runs.Last().Type != Type || Runs.Last().Texture != Texture ||
				(Texture != nullptr && Runs.Last().bNearest != bNearest))
			{
				FCanvasPrimitiveRun& Run = Runs.AddDefaulted_GetRef();
				Run.Type = Type;
				Run.Texture = Texture;
				Run.bNearest = Texture != nullptr && bNearest;
				Run.FirstVertex = Vertices.Num();
			}
		}

		void Add(const FCanvasVertex& Vertex)
		{
			Vertices.Add(Vertex);
			++Runs.Last().NumVertices;
		}

		/**
		 * A rectangle between two opposite corners (any order): its top-left and bottom-right, in Color, with the
		 * texture coordinates of each corner (swapped with it).
		 */
		void AddRectangle(float InX0, float InY0, float InX1, float InY1, const FCanvasVertex& Color, float InU0 = 0.0f,
			float InV0 = 0.0f, float InU1 = 0.0f, float InV1 = 0.0f)
		{
			if (InX1 < InX0)
			{
				Swap(InX0, InX1);
				Swap(InU0, InU1);
			}
			if (InY1 < InY0)
			{
				Swap(InY0, InY1);
				Swap(InV0, InV1);
			}
			FCanvasVertex TopLeft = Color;
			TopLeft.X = InX0;
			TopLeft.Y = InY0;
			TopLeft.Z = 0.0f;
			TopLeft.U = InU0;
			TopLeft.V = InV0;
			FCanvasVertex BottomRight = Color;
			BottomRight.X = InX1;
			BottomRight.Y = InY1;
			BottomRight.Z = 0.0f;
			BottomRight.U = InU1;
			BottomRight.V = InV1;
			Add(TopLeft);
			Add(BottomRight);
		}
	};

	/** A linear colour as a vertex's, its alpha kept (bAlphaBlend) or made opaque. */
	[[nodiscard]] FCanvasVertex ColorVertex(const FLinearColor& InColor, bool bAlphaBlend = true)
	{
		FCanvasVertex Out{};
		Out.R = InColor.R;
		Out.G = InColor.G;
		Out.B = InColor.B;
		Out.A = bAlphaBlend ? FMath::Clamp(InColor.A, 0.0f, 1.0f) : 1.0f;
		return Out;
	}

	/** An 8-bit colour taken as it is (UE: FColor::ReinterpretAsLinear), not through the sRGB curve. */
	[[nodiscard]] FLinearColor ReinterpretAsLinear(const FColor& InColor)
	{
		constexpr float InverseByte = 1.0f / 255.0f;
		return FLinearColor(float(InColor.R) * InverseByte, float(InColor.G) * InverseByte,
			float(InColor.B) * InverseByte, float(InColor.A) * InverseByte);
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
		const FCanvasVertex Color = ColorVertex(InColor);
		if (Dx == 0.0f || Dy == 0.0f)
		{
			Sink.Begin(ECanvasPrimitive::Rectangle);
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

	/** The width of each line of Text (up to MaxLines; the rest take the last's), and how many lines it has. */
	constexpr int32 MaxMeasuredLines = 32;

	/** Calls Visit(Begin, Count, LineIndex) for every '\n'-separated line of Text (including empty ones). */
	template <typename VisitorType>
	void ForEachLine(const TCHAR* Text, int32 Len, const VisitorType& Visit)
	{
		const TCHAR* Cursor = Text;
		const TCHAR* TextEnd = Text + Len;
		int32 LineIndex = 0;
		for (;;)
		{
			const TCHAR* End = Cursor;
			while (End < TextEnd && *End != '\n')
			{
				++End;
			}
			Visit(Cursor, int32(End - Cursor), LineIndex);
			if (End >= TextEnd)
			{
				break;
			}
			Cursor = End + 1;
			++LineIndex;
		}
	}

	/**
	 * One copy of a text item's glyphs at an offset in a colour: page by page, each line from its justified origin
	 * (whole pixels, so the glyphs' texels land on pixels), each glyph a textured rectangle.
	 */
	void AppendTextCopy(FPrimitiveSink& Sink, const UFont& Font, const TCHAR* Text, int32 Len, float X, float Y,
		float Scale, ETextJustify Justify, float OffsetX, float OffsetY, const FLinearColor& Color,
		const float* LineWidths)
	{
		const FCanvasVertex Colored = ColorVertex(Color);
		const float LineStep = Font.GetLineHeight() * Scale;
		const bool bNearest = Scale == 1.0f;
		for (int32 PageIndex = 0; PageIndex < Font.Textures.Num(); ++PageIndex)
		{
			const UTexture2D* Page = Font.Textures[PageIndex];
			if (Page == nullptr || Page->GetSizeX() <= 0 || Page->GetSizeY() <= 0)
			{
				continue;
			}
			const float InversePageX = 1.0f / float(Page->GetSizeX());
			const float InversePageY = 1.0f / float(Page->GetSizeY());
			bool bBegun = false;
			ForEachLine(Text, Len,
				[&](const TCHAR* Line, int32 Count, int32 LineIndex)
				{
					const float Width = LineWidths[FMath::Min(LineIndex, MaxMeasuredLines - 1)];
					float OriginX = X;
					if (Justify == ETextJustify::Center)
					{
						OriginX -= Width * 0.5f;
					}
					else if (Justify == ETextJustify::Right)
					{
						OriginX -= Width;
					}
					float PenX = FMath::RoundToFloat(OriginX) + OffsetX;
					const float Top = FMath::RoundToFloat(Y + (LineStep * float(LineIndex))) + OffsetY;
					const TCHAR* Cursor = Line;
					const TCHAR* End = Line + Count;
					uint32 Previous = 0;
					while (Cursor < End)
					{
						const uint32 CodePoint = UFont::DecodeCodePoint(Cursor, End);
						const FFontCharacter* Character = Font.FindCharacter(CodePoint);
						if (Character == nullptr)
						{
							continue;
						}
						if (Previous != 0)
						{
							PenX += float(Font.GetCharKerning(Previous, CodePoint)) * Scale;
						}
						Previous = CodePoint;
						if (Character->TextureIndex == PageIndex && Character->USize > 0 && Character->VSize > 0)
						{
							if (!bBegun)
							{
								Sink.Begin(ECanvasPrimitive::Rectangle, Page, bNearest);
								bBegun = true;
							}
							const float GlyphX = PenX + (float(Character->HorizontalOffset) * Scale);
							const float GlyphY = Top + (float(Character->VerticalOffset) * Scale);
							Sink.AddRectangle(GlyphX, GlyphY, GlyphX + (float(Character->USize) * Scale),
								GlyphY + (float(Character->VSize) * Scale), Colored,
								float(Character->StartU) * InversePageX, float(Character->StartV) * InversePageY,
								float(Character->StartU + Character->USize) * InversePageX,
								float(Character->StartV + Character->VSize) * InversePageY);
						}
						PenX += float(Character->Advance) * Scale;
					}
				});
		}
	}

} // namespace

// Items

void FCanvasTileItem::Draw(FCanvas* InCanvas)
{
	if (InCanvas == nullptr)
	{
		return;
	}
	FCanvas::FTileItem Tile;
	Tile.X = Position.X;
	Tile.Y = Position.Y;
	Tile.SizeX = Size.X;
	Tile.SizeY = Size.Y;
	Tile.U0 = UV0.X;
	Tile.V0 = UV0.Y;
	Tile.U1 = UV1.X;
	Tile.V1 = UV1.Y;
	Tile.Texture = Cast<UTexture2D>(const_cast<UTexture*>(Texture));
	Tile.Rotation = Rotation.Yaw;
	Tile.PivotX = PivotPoint.X;
	Tile.PivotY = PivotPoint.Y;
	Tile.Color = SetColorValue;
	InCanvas->GetBatch().Tiles.Add(Tile);
}

void FCanvasTextItem::Draw(FCanvas* InCanvas)
{
	if (InCanvas == nullptr || Text.IsEmpty())
	{
		return;
	}
	FCanvas::FTextItem Item;
	Item.Font = FCanvas::GetFontOrDefault(Font);
	Item.X = Position.X;
	Item.Y = Position.Y;
	Item.Scale = Scale.X;
	Item.Justify = bCentreX ? ETextJustify::Center : bRightJustify ? ETextJustify::Right : ETextJustify::Left;
	if (bCentreY && Item.Font != nullptr)
	{
		Item.Y -= float(Item.Font->GetStringHeightSize(*Text.ToString())) * Scale.X * 0.5f;
	}
	Item.Color = SetColorValue;
	if (ShadowOffset.X != 0.0f || ShadowOffset.Y != 0.0f)
	{
		Item.ShadowOffsetX = ShadowOffset.X;
		Item.ShadowOffsetY = ShadowOffset.Y;
		Item.ShadowColor = ShadowColor;
	}
	if (bOutlined)
	{
		Item.OutlineColor = OutlineColor;
	}
	const FString& String = Text.ToString();
	InCanvas->AddText(*String, String.Len(), Item);
}

// The canvas

FCanvas::FCanvas(int32 InSizeX, int32 InSizeY)
	: MemMark(FMemStack::Get())
	, SizeX(InSizeX)
	, SizeY(InSizeY)
{
}

const TCHAR* FCanvas::CopyText(const TCHAR* Text, int32 Len)
{
	const SIZE_T Bytes = SIZE_T(Len) * sizeof(TCHAR);
	TCHAR* Copy = reinterpret_cast<TCHAR*>(FMemStack::Get().PushBytes(Bytes + sizeof(TCHAR), alignof(TCHAR)));
	FMemory::Memcpy(Copy, Text, Bytes);
	Copy[Len] = '\0';
	return Copy;
}

const UFont* FCanvas::GetFontOrDefault(const UFont* Font)
{
	return Font != nullptr ? Font : UEngine::GetSmallFont();
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
	FTileItem Tile;
	Tile.X = X;
	Tile.Y = Y;
	Tile.SizeX = InSizeX;
	Tile.SizeY = InSizeY;
	Tile.Color = Color;
	GetBatch().Tiles.Add(Tile);
}

void FCanvas::DrawTile(float X, float Y, float InSizeX, float InSizeY, float U, float V, float SizeU, float SizeV,
	const FLinearColor& Color, const UTexture* Texture, bool bAlphaBlend)
{
	FTileItem Tile;
	Tile.X = X;
	Tile.Y = Y;
	Tile.SizeX = InSizeX;
	Tile.SizeY = InSizeY;
	Tile.U0 = U;
	Tile.V0 = V;
	Tile.U1 = U + SizeU;
	Tile.V1 = V + SizeV;
	Tile.Texture = Cast<UTexture2D>(const_cast<UTexture*>(Texture));
	Tile.Color = Color;
	if (!bAlphaBlend)
	{
		Tile.Color.A = 1.0f;
	}
	GetBatch().Tiles.Add(Tile);
}

void FCanvas::DrawLine(float X0, float Y0, float X1, float Y1, const FLinearColor& Color, float Thickness)
{
	GetBatch().Lines.Add(FLineItem{X0, Y0, X1, Y1, Thickness, Color});
}

void FCanvas::AddText(const TCHAR* Text, int32 Len, const FTextItem& Item)
{
	if (Text == nullptr || Len <= 0 || Item.Font == nullptr)
	{
		return;
	}
	FTextItem& Added = GetBatch().Texts.Add_GetRef(Item);
	Added.Text = CopyText(Text, Len);
	Added.Len = Len;
}

void FCanvas::DrawText(const UFont* Font, const FString& Text, float X, float Y, const FLinearColor& Color,
	ETextJustify Justify, const FVector2D& ShadowOffset, const FLinearColor& ShadowColor,
	const FLinearColor& OutlineColor)
{
	FTextItem Item;
	Item.Font = GetFontOrDefault(Font);
	Item.X = X;
	Item.Y = Y;
	Item.Justify = Justify;
	Item.Color = Color;
	Item.ShadowOffsetX = ShadowOffset.X;
	Item.ShadowOffsetY = ShadowOffset.Y;
	Item.ShadowColor = ShadowColor;
	Item.OutlineColor = OutlineColor;
	AddText(*Text, Text.Len(), Item);
}

void FCanvas::DrawText(
	const UFont* Font, const FString& Text, float X, float Y, const FColor& Color, ETextJustify Justify)
{
	DrawText(Font, Text, X, Y, ReinterpretAsLinear(Color), Justify);
}

int32 FCanvas::DrawShadowedString(float StartX, float StartY, const TCHAR* Text, const UFont* Font,
	const FLinearColor& Color, const FLinearColor& ShadowColor)
{
	FTextItem Item;
	Item.Font = GetFontOrDefault(Font);
	if (Item.Font == nullptr || Text == nullptr)
	{
		return 0;
	}
	Item.X = StartX;
	Item.Y = StartY;
	Item.Color = Color;
	Item.ShadowOffsetX = 1.0f;
	Item.ShadowOffsetY = 1.0f;
	Item.ShadowColor = ShadowColor;
	AddText(Text, FCString::Strlen(Text), Item);
	return Item.Font->GetStringHeightSize(Text);
}

void FCanvas::DrawItem(FCanvasItem& Item)
{
	Item.Draw(this);
}

void FCanvas::MeasureText(const UFont* Font, const FString& Text, float& OutWidth, float& OutHeight)
{
	const UFont* Measured = GetFontOrDefault(Font);
	if (Measured == nullptr || Text.IsEmpty())
	{
		OutWidth = 0.0f;
		OutHeight = Measured != nullptr ? Measured->GetLineHeight() : 0.0f;
		return;
	}
	OutWidth = float(Measured->GetStringSize(*Text));
	OutHeight = float(Measured->GetStringHeightSize(*Text));
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
			const FCanvasVertex Color = ColorVertex(Tile.Color);
			const UTexture2D* Texture =
				Tile.Texture != nullptr && Tile.Texture->HasValidPlatformData() ? Tile.Texture : nullptr;
			if (Tile.Rotation == 0.0f)
			{
				// Texels one to one with pixels: sampled nearest.
				const bool bNearest = Texture != nullptr &&
					FMath::IsNearlyEqual(
						FMath::Abs(Tile.SizeX), FMath::Abs(Tile.U1 - Tile.U0) * float(Texture->GetSizeX()), 0.01f) &&
					FMath::IsNearlyEqual(
						FMath::Abs(Tile.SizeY), FMath::Abs(Tile.V1 - Tile.V0) * float(Texture->GetSizeY()), 0.01f);
				Sink.Begin(ECanvasPrimitive::Rectangle, Texture, bNearest);
				Sink.AddRectangle(Tile.X, Tile.Y, Tile.X + Tile.SizeX, Tile.Y + Tile.SizeY, Color, Tile.U0, Tile.V0,
					Tile.U1, Tile.V1);
				continue;
			}
			// Rotated: the four corners turned about the pivot (clockwise on the screen, whose Y is down), two
			// triangles.
			const float PivotX = Tile.X + (Tile.PivotX * Tile.SizeX);
			const float PivotY = Tile.Y + (Tile.PivotY * Tile.SizeY);
			const float Radians = FMath::DegreesToRadians(Tile.Rotation);
			const float Cos = FMath::Cos(Radians);
			const float Sin = FMath::Sin(Radians);
			const float CornerX[4] = {Tile.X, Tile.X + Tile.SizeX, Tile.X + Tile.SizeX, Tile.X};
			const float CornerY[4] = {Tile.Y, Tile.Y, Tile.Y + Tile.SizeY, Tile.Y + Tile.SizeY};
			const float CornerU[4] = {Tile.U0, Tile.U1, Tile.U1, Tile.U0};
			const float CornerV[4] = {Tile.V0, Tile.V0, Tile.V1, Tile.V1};
			constexpr int32 Order[6] = {0, 1, 2, 0, 2, 3};
			Sink.Begin(ECanvasPrimitive::Triangle, Texture, false);
			for (const int32 Corner : Order)
			{
				const float Dx = CornerX[Corner] - PivotX;
				const float Dy = CornerY[Corner] - PivotY;
				FCanvasVertex Out = Color;
				Out.X = PivotX + (Dx * Cos) - (Dy * Sin);
				Out.Y = PivotY + (Dx * Sin) + (Dy * Cos);
				Out.Z = 0.0f;
				Out.U = CornerU[Corner];
				Out.V = CornerV[Corner];
				Sink.Add(Out);
			}
		}
		for (const FLineItem& Line : Batch.Lines)
		{
			AppendThickScreenLine(Sink, Line.X0, Line.Y0, Line.X1, Line.Y1, Line.Thickness, Line.Color);
		}
		for (const FTextItem& Text : Batch.Texts)
		{
			// The lines' widths once (for their justification), then the outline, the shadow and the text.
			float LineWidths[MaxMeasuredLines] = {};
			ForEachLine(Text.Text, Text.Len,
				[&](const TCHAR* Line, int32 Count, int32 LineIndex)
				{
					if (LineIndex < MaxMeasuredLines && Text.Justify != ETextJustify::Left)
					{
						LineWidths[LineIndex] = float(Text.Font->GetLineWidth(Line, Count)) * Text.Scale;
					}
				});
			if (Text.OutlineColor.A > 0.0f)
			{
				constexpr float Offsets[4][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
				for (const auto& Offset : Offsets)
				{
					AppendTextCopy(Sink, *Text.Font, Text.Text, Text.Len, Text.X, Text.Y, Text.Scale, Text.Justify,
						Offset[0], Offset[1], Text.OutlineColor, LineWidths);
				}
			}
			if (Text.ShadowColor.A > 0.0f)
			{
				AppendTextCopy(Sink, *Text.Font, Text.Text, Text.Len, Text.X, Text.Y, Text.Scale, Text.Justify,
					Text.ShadowOffsetX, Text.ShadowOffsetY, Text.ShadowColor, LineWidths);
			}
			AppendTextCopy(Sink, *Text.Font, Text.Text, Text.Len, Text.X, Text.Y, Text.Scale, Text.Justify, 0.0f, 0.0f,
				Text.Color, LineWidths);
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
