#include "GSDebugDraw.h"

#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"

namespace
{

	/** A glyph's cell at scale 1, in pixels, and its advance in cells (5 columns and a space). */
	constexpr float Cell = 2.0f;
	constexpr float GlyphAdvanceCells = 6.0f;
	constexpr float GlyphHeightCells = 7.0f;

	/** 5x7 glyphs: seven rows, bit 4 the leftmost column. */
	[[nodiscard]] const unsigned char* GlyphRows(char Ch)
	{
		switch (Ch)
		{
			case ' ':
			{
				static const unsigned char R[7] = {0, 0, 0, 0, 0, 0, 0};
				return R;
			}
			case '.':
			{
				static const unsigned char R[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
				return R;
			}
			case '0':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
				return R;
			}
			case '1':
			{
				static const unsigned char R[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
				return R;
			}
			case '2':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
				return R;
			}
			case '3':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E};
				return R;
			}
			case '4':
			{
				static const unsigned char R[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
				return R;
			}
			case '5':
			{
				static const unsigned char R[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
				return R;
			}
			case '6':
			{
				static const unsigned char R[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
				return R;
			}
			case '7':
			{
				static const unsigned char R[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
				return R;
			}
			case '8':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
				return R;
			}
			case '9':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
				return R;
			}
			case 'F':
			{
				static const unsigned char R[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
				return R;
			}
			case 'P':
			{
				static const unsigned char R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
				return R;
			}
			case 'S':
			{
				static const unsigned char R[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
				return R;
			}
			case 'M':
			{
				static const unsigned char R[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
				return R;
			}
			case 'm':
			{
				static const unsigned char R[7] = {0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15};
				return R;
			}
			case 's':
			{
				static const unsigned char R[7] = {0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E};
				return R;
			}
			case 'C':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
				return R;
			}
			case 'D':
			{
				static const unsigned char R[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
				return R;
			}
			case 'E':
			{
				static const unsigned char R[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
				return R;
			}
			case 'W':
			{
				static const unsigned char R[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
				return R;
			}
			case 'N':
			{
				static const unsigned char R[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
				return R;
			}
			case 'X':
			{
				static const unsigned char R[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
				return R;
			}
			case 'Y':
			{
				static const unsigned char R[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
				return R;
			}
			case 'I':
			{
				static const unsigned char R[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
				return R;
			}
			case 'T':
			{
				static const unsigned char R[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
				return R;
			}
			// Engine stats HUD letters (RAM / VRAM / RES / MB).
			case 'R':
			{
				static const unsigned char R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
				return R;
			}
			case 'A':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
				return R;
			}
			case 'B':
			{
				static const unsigned char R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
				return R;
			}
			case 'K':
			{
				static const unsigned char R[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
				return R;
			}
			case 'V':
			{
				static const unsigned char R[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
				return R;
			}
			// Remaining uppercase + ':' so any HUD label renders.
			case 'G':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
				return R;
			}
			case 'H':
			{
				static const unsigned char R[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
				return R;
			}
			case 'J':
			{
				static const unsigned char R[7] = {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C};
				return R;
			}
			case 'L':
			{
				static const unsigned char R[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
				return R;
			}
			case 'O':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
				return R;
			}
			case 'Q':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
				return R;
			}
			case 'U':
			{
				static const unsigned char R[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
				return R;
			}
			case 'Z':
			{
				static const unsigned char R[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
				return R;
			}
			case ':':
			{
				static const unsigned char R[7] = {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
				return R;
			}
			case '-':
			{
				static const unsigned char R[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
				return R;
			}
			case '/':
			{
				static const unsigned char R[7] = {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
				return R;
			}
			// Punctuation for the error screen's messages (paths, log lines).
			case '(':
			{
				static const unsigned char R[7] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
				return R;
			}
			case ')':
			{
				static const unsigned char R[7] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
				return R;
			}
			case ',':
			{
				static const unsigned char R[7] = {0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08};
				return R;
			}
			case '\'':
			{
				static const unsigned char R[7] = {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00};
				return R;
			}
			case '_':
			{
				static const unsigned char R[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
				return R;
			}
			case '>':
			{
				static const unsigned char R[7] = {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08};
				return R;
			}
			case '<':
			{
				static const unsigned char R[7] = {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02};
				return R;
			}
			case '=':
			{
				static const unsigned char R[7] = {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00};
				return R;
			}
			case '!':
			{
				static const unsigned char R[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
				return R;
			}
			case '[':
			{
				static const unsigned char R[7] = {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E};
				return R;
			}
			case ']':
			{
				static const unsigned char R[7] = {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E};
				return R;
			}
			case '?':
			{
				static const unsigned char R[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
				return R;
			}
			case '+':
			{
				static const unsigned char R[7] = {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
				return R;
			}
			default:
				return nullptr;
		}
	}

	/** Appends one glyph as a sprite per run of set pixels in each row (the colour and PRIM are already set). */
	void AppendGlyphRuns(
		FGSCommandList& List, const FGSDrawEnvironment& Environment, float X, float Y, char Ch, float InCell)
	{
		const unsigned char* Rows = GlyphRows(Ch);
		if (Rows == nullptr)
		{
			return;
		}
		for (int32 Row = 0; Row < 7; ++Row)
		{
			const unsigned char Bits = Rows[Row];
			int32 Col = 0;
			while (Col < 5)
			{
				while (Col < 5 && (Bits & static_cast<unsigned char>(0x10 >> Col)) == 0)
				{
					++Col;
				}
				if (Col >= 5)
				{
					break;
				}
				const int32 RunStart = Col;
				while (Col < 5 && (Bits & static_cast<unsigned char>(0x10 >> Col)) != 0)
				{
					++Col;
				}
				const float X0 = X + (float(RunStart) * InCell);
				const float X1 = X + (float(Col) * InCell);
				const float Y0 = Y + (float(Row) * InCell);
				List.AddVertex(Environment.PixelVertex(X0, Y0));
				List.AddVertex(Environment.PixelVertex(X1, Y0 + InCell));
			}
		}
	}

	/** The glyph a character draws as: lowercase is uppercase but for the "ms" of the timings. */
	[[nodiscard]] char GlyphOf(char Ch)
	{
		return (Ch >= 'a' && Ch <= 'z' && Ch != 'm' && Ch != 's') ? char(FChar::ToUpper(Ch)) : Ch;
	}

	[[nodiscard]] uint8 UnitToByte(float Value)
	{
		return uint8(FMath::Clamp(int32((Value * 255.0f) + 0.5f), 0, 255));
	}

} // namespace

FGSRGBAQ FGSDebugDraw::UnitColor(float R, float G, float B, float Alpha)
{
	FGSRGBAQ Color;
	Color.R = UnitToByte(R);
	Color.G = UnitToByte(G);
	Color.B = UnitToByte(B);
	Color.A = uint8(FMath::Clamp(int32((Alpha * 128.0f) + 0.5f), 0, 0x80));
	return Color;
}

float FGSDebugDraw::GetTextWidth(const char* Text, float Scale)
{
	int32 Length = 0;
	for (const char* P = Text; P != nullptr && *P != '\0'; ++P)
	{
		++Length;
	}
	return float(Length) * GlyphAdvanceCells * Cell * (Scale > 0.0f ? Scale : 1.0f);
}

float FGSDebugDraw::GetTextHeight(float Scale)
{
	return GlyphHeightCells * Cell * (Scale > 0.0f ? Scale : 1.0f);
}

void FGSDebugDraw::DrawString(FGSCommandList& List, const FGSDrawEnvironment& Environment, float X, float Y,
	const char* Text, const FGSRGBAQ& Color, float Scale)
{
	if (Text == nullptr || *Text == '\0')
	{
		return;
	}
	const float LocalCell = Cell * (Scale > 0.0f ? Scale : 1.0f);
	// An overlay: no depth test, so the scene cannot cover the text; opaque sprites.
	List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	FGSPrim Sprite;
	Sprite.Type = EGSPrimitive::Sprite;
	List.SetPrim(Sprite);
	List.SetRGBAQ(Color);
	float Cx = X;
	for (const char* P = Text; *P != '\0'; ++P)
	{
		AppendGlyphRuns(List, Environment, Cx, Y, GlyphOf(*P), LocalCell);
		Cx += GlyphAdvanceCells * LocalCell;
	}
	List.SetTest(0, FGSDrawEnvironment::DepthTest(true));
}

void FGSDebugDraw::DrawRect(FGSCommandList& List, const FGSDrawEnvironment& Environment, float X0, float Y0, float X1,
	float Y1, const FGSRGBAQ& Color)
{
	List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	FGSPrim Sprite;
	Sprite.Type = EGSPrimitive::Sprite;
	// Below 0x80 the environment's blending: (Cs - Cd) * As + Cd.
	Sprite.bAlphaBlend = Color.A < 0x80;
	List.SetPrim(Sprite);
	List.SetRGBAQ(Color);
	List.AddVertex(Environment.PixelVertex(FMath::Min(X0, X1), FMath::Min(Y0, Y1)));
	List.AddVertex(Environment.PixelVertex(FMath::Max(X0, X1), FMath::Max(Y0, Y1)));
	List.SetTest(0, FGSDrawEnvironment::DepthTest(true));
}
