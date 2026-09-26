#include "Misc/Char.h"
#include "PS2GSContext.h"
#include "PS2RHI.h"

namespace
{

	constexpr float Cell = 2.0f;
	constexpr float GlyphAdvanceCells = 6.0f; // 5 columns + 1 spacing

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
			// Clip-debug HUD letters (Draw3D counters).
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
			default:
				return nullptr;
		}
	}

	/** Appends one glyph as a sprite per run of set pixels in each row (the color and PRIM are already set). */
	void AppendGlyphRuns(FGSCommandList& List, float X, float Y, char Ch, float InCell)
	{
		const unsigned char* Rows = GlyphRows(Ch);
		if (Rows == nullptr)
		{
			return;
		}
		for (int Row = 0; Row < 7; ++Row)
		{
			const unsigned char Bits = Rows[Row];
			int Col = 0;
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
				const int RunStart = Col;
				while (Col < 5 && (Bits & static_cast<unsigned char>(0x10 >> Col)) != 0)
				{
					++Col;
				}
				const float X0 = X + static_cast<float>(RunStart) * InCell;
				const float X1 = X + static_cast<float>(Col) * InCell;
				const float Y0 = Y + static_cast<float>(Row) * InCell;
				List.AddVertex(Leon::PS2::ScreenVertex(X0, Y0));
				List.AddVertex(Leon::PS2::ScreenVertex(X1, Y0 + InCell));
			}
		}
	}

} // namespace

void FPS2RHI::DrawDebugText(float X, float Y, const char* Text, float InR, float G, float B, float Scale)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (Text == nullptr || !Gs.bReady)
	{
		return;
	}
	const float LocalCell = Cell * (Scale > 0.0f ? Scale : 1.0f);

	// Overlay: no depth test, so 3D geometry near the camera cannot cover the text.
	FGSPrim Sprite;
	Sprite.Type = EGSPrimitive::Sprite;
	Leon::PS2::AppendDepthTest(Gs, false);
	Gs.FrameList.SetPrim(Sprite);
	Gs.FrameList.SetRGBAQ(Leon::PS2::UnitColor(InR, G, B));
	float Cx = X;
	for (const char* P = Text; *P != '\0'; ++P)
	{
		char Ch = *P;
		if (Ch >= 'a' && Ch <= 'z' && Ch != 'm' && Ch != 's')
		{
			Ch = FChar::ToUpper(Ch);
		}
		AppendGlyphRuns(Gs.FrameList, Cx, Y, Ch, LocalCell);
		Cx += GlyphAdvanceCells * LocalCell;
	}
	Leon::PS2::AppendDepthTest(Gs, true);
}
