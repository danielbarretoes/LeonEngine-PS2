#include "GSDebugDraw.h"

#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"

namespace
{

	/** A glyph of the debug font: its rectangle in its page, its bearing from the pen and the line's top, its advance.
	 */
	struct FGSDebugGlyph
	{
		uint8 U;
		uint8 V;
		uint8 Width;
		uint8 Height;
		int8 OffsetX;
		int8 OffsetY;
		/** Pixels the pen moves; 0 when the font has no glyph for the code point. */
		uint8 Advance;
	};

	/** Pixels added to a character's advance when a given one follows it: (first << 8) | second. */
	struct FGSDebugKerningPair
	{
		uint16 Pair;
		int8 Amount;
	};

	/** One size of the debug font: its metrics, its PSMT4 page and where the page goes from the font's first block. */
	struct FGSDebugFontSize
	{
		int32 PixelHeight;
		int32 Ascent;
		int32 Descent;
		int32 Leading;
		int32 Width;
		int32 Height;
		/** TEX0's TW and TH (log2 of the size) and TBW. */
		uint8 TW;
		uint8 TH;
		uint8 TBW;
		uint32 Block;
		const uint8* Texels;
		/** By code point, 0 to 255. */
		const FGSDebugGlyph* Glyphs;
		const FGSDebugKerningPair* KerningPairs;
		int32 NumKerningPairs;
	};

} // namespace

#include "GSDebugFontData.inl"

namespace
{

	static_assert(UE_ARRAY_COUNT(GSDebugFontData::Sizes) == int32(EGSDebugFont::Small) + 1, "a size per EGSDebugFont");

	[[nodiscard]] const FGSDebugFontSize& GetSize(EGSDebugFont Font)
	{
		return GSDebugFontData::Sizes[FMath::Clamp(int32(Font), 0, int32(EGSDebugFont::Small))];
	}

	/** The glyph of a code point, else '?' (UFont's NullCharacter); null when there is neither. */
	[[nodiscard]] const FGSDebugGlyph* FindGlyph(const FGSDebugFontSize& Size, uint32 CodePoint)
	{
		if (CodePoint < 256 && Size.Glyphs[CodePoint].Advance != 0)
		{
			return &Size.Glyphs[CodePoint];
		}
		const FGSDebugGlyph& Null = Size.Glyphs[uint32('?')];
		return Null.Advance != 0 ? &Null : nullptr;
	}

	/** The kerning between two code points (a binary search of the sorted pairs). */
	[[nodiscard]] int32 GetKerning(const FGSDebugFontSize& Size, uint32 First, uint32 Second)
	{
		if (First > 255 || Second > 255)
		{
			return 0;
		}
		const uint16 Pair = uint16((First << 8) | Second);
		int32 Low = 0;
		int32 High = Size.NumKerningPairs - 1;
		while (Low <= High)
		{
			const int32 Middle = (Low + High) / 2;
			const uint16 Candidate = Size.KerningPairs[Middle].Pair;
			if (Candidate == Pair)
			{
				return Size.KerningPairs[Middle].Amount;
			}
			if (Candidate < Pair)
			{
				Low = Middle + 1;
			}
			else
			{
				High = Middle - 1;
			}
		}
		return 0;
	}

	/** The end of Text's first NumBytes bytes, or of the whole string. */
	[[nodiscard]] const char* GetEnd(const char* Text, int32 NumBytes)
	{
		if (NumBytes >= 0)
		{
			return Text + NumBytes;
		}
		const char* End = Text;
		while (*End != '\0')
		{
			++End;
		}
		return End;
	}

	[[nodiscard]] uint8 UnitToByte(float Value)
	{
		return uint8(FMath::Clamp(int32((Value * 255.0f) + 0.5f), 0, 255));
	}

	/** A UnitColor channel (0xff full) as MODULATE takes it (0x80 full). */
	[[nodiscard]] uint8 ToModulate(uint8 Value)
	{
		return uint8(((uint32(Value) * 0x80u) + 127u) / 255u);
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

uint32 FGSDebugDraw::GetFontBlocks()
{
	return GSDebugFontData::NumBlocks;
}

void FGSDebugDraw::UploadFont(FGSCommandList& List, uint32 FontBlock)
{
	check(FontBlock % 32 == 0);
	FGSBitBltBuf Clut;
	Clut.DBP = uint16(FontBlock + GSDebugFontData::ClutBlock);
	Clut.DBW = 1;
	Clut.DPSM = EGSPixelFormat::PSMCT32;
	List.UploadImageInPlace(
		Clut, 0, 0, 8, 2, MakeArrayView(GSDebugFontData::Clut, UE_ARRAY_COUNT(GSDebugFontData::Clut)));
	for (const FGSDebugFontSize& Size : GSDebugFontData::Sizes)
	{
		FGSBitBltBuf Page;
		Page.DBP = uint16(FontBlock + Size.Block);
		Page.DBW = Size.TBW;
		Page.DPSM = EGSPixelFormat::PSMT4;
		List.UploadImageInPlace(Page, 0, 0, uint16(Size.Width), uint16(Size.Height),
			MakeArrayView(Size.Texels, (Size.Width * Size.Height) / 2));
	}
	List.TexFlush();
}

int32 FGSDebugDraw::GetLineHeight(EGSDebugFont Font)
{
	const FGSDebugFontSize& Size = GetSize(Font);
	return Size.Ascent + Size.Descent + Size.Leading;
}

int32 FGSDebugDraw::MeasureString(const char* Text, EGSDebugFont Font, int32 NumBytes)
{
	if (Text == nullptr)
	{
		return 0;
	}
	const FGSDebugFontSize& Size = GetSize(Font);
	const char* End = GetEnd(Text, NumBytes);
	int32 Width = 0;
	uint32 Previous = 0;
	for (const char* Cursor = Text; Cursor < End;)
	{
		const uint32 CodePoint = FCharAnsi::DecodeCodePoint(Cursor, End);
		if (const FGSDebugGlyph* Glyph = FindGlyph(Size, CodePoint))
		{
			Width += (Previous != 0 ? GetKerning(Size, Previous, CodePoint) : 0) + Glyph->Advance;
		}
		Previous = CodePoint;
	}
	return Width;
}

int32 FGSDebugDraw::FindLineBreak(const char* Text, int32 MaxWidth, EGSDebugFont Font)
{
	if (Text == nullptr)
	{
		return 0;
	}
	const char* LineEnd = Text;
	while (*LineEnd != '\0' && *LineEnd != '\n')
	{
		++LineEnd;
	}
	const FGSDebugFontSize& Size = GetSize(Font);
	int32 Width = 0;
	uint32 Previous = 0;
	int32 Fit = 0;
	int32 AfterLastSpace = 0;
	for (const char* Cursor = Text; Cursor < LineEnd;)
	{
		const uint32 CodePoint = FCharAnsi::DecodeCodePoint(Cursor, LineEnd);
		if (const FGSDebugGlyph* Glyph = FindGlyph(Size, CodePoint))
		{
			Width += (Previous != 0 ? GetKerning(Size, Previous, CodePoint) : 0) + Glyph->Advance;
		}
		Previous = CodePoint;
		if (Width > MaxWidth && Fit > 0)
		{
			// A space past the edge ends the line (drawn as nothing); a word would be split: the line ends after its
			// last space, if it has one.
			if (CodePoint == ' ')
			{
				return int32(Cursor - Text);
			}
			return AfterLastSpace > 0 ? AfterLastSpace : Fit;
		}
		Fit = int32(Cursor - Text);
		AfterLastSpace = CodePoint == ' ' ? Fit : AfterLastSpace;
	}
	return Fit;
}

void FGSDebugDraw::DrawString(FGSCommandList& List, const FGSDrawEnvironment& Environment, uint32 FontBlock, float X,
	float Y, const char* Text, const FGSRGBAQ& Color, EGSDebugFont Font)
{
	if (Text == nullptr || *Text == '\0')
	{
		return;
	}
	const FGSDebugFontSize& Size = GetSize(Font);
	// An overlay: no depth test, so the scene cannot cover the text.
	List.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	// The page and the shared CLUT (loaded with TEX0), sampled one texel a pixel, clamped; MODULATE by the colour,
	// blended by the coverage (the environment's (Cs - Cd) * As + Cd).
	FGSTex0 Tex0;
	Tex0.TBP0 = uint16(FontBlock + Size.Block);
	Tex0.TBW = Size.TBW;
	Tex0.PSM = EGSPixelFormat::PSMT4;
	Tex0.TW = Size.TW;
	Tex0.TH = Size.TH;
	Tex0.bRGBA = true;
	Tex0.TFX = EGSTextureFunction::Modulate;
	Tex0.CBP = uint16(FontBlock + GSDebugFontData::ClutBlock);
	Tex0.CPSM = EGSPixelFormat::PSMCT32;
	Tex0.CLD = 1;
	List.SetTex0(0, Tex0);
	FGSTex1 Nearest;
	Nearest.bFixedLOD = true;
	List.SetTex1(0, Nearest);
	FGSClamp Clamp;
	Clamp.WMS = EGSWrapMode::Clamp;
	Clamp.WMT = EGSWrapMode::Clamp;
	List.SetClamp(0, Clamp);
	FGSPrim Sprite;
	Sprite.Type = EGSPrimitive::Sprite;
	Sprite.bTextured = true;
	Sprite.bUseUV = true;
	Sprite.bAlphaBlend = true;
	List.SetPrim(Sprite);
	FGSRGBAQ Tint = Color;
	Tint.R = ToModulate(Color.R);
	Tint.G = ToModulate(Color.G);
	Tint.B = ToModulate(Color.B);
	List.SetRGBAQ(Tint);

	const char* End = GetEnd(Text, INDEX_NONE);
	int32 PenX = FMath::RoundToInt(X);
	const int32 Top = FMath::RoundToInt(Y);
	uint32 Previous = 0;
	for (const char* Cursor = Text; Cursor < End;)
	{
		const uint32 CodePoint = FCharAnsi::DecodeCodePoint(Cursor, End);
		const FGSDebugGlyph* Glyph = FindGlyph(Size, CodePoint);
		if (Glyph == nullptr)
		{
			Previous = CodePoint;
			continue;
		}
		PenX += Previous != 0 ? GetKerning(Size, Previous, CodePoint) : 0;
		Previous = CodePoint;
		if (Glyph->Width != 0 && Glyph->Height != 0)
		{
			// The texels map to the pixels one to one: the corners half a pixel up and left, so each pixel's centre
			// samples its texel's (the canvas's glyphs, the TexturedCanvas scene).
			const float Left = float(PenX + Glyph->OffsetX) - 0.5f;
			const float GlyphTop = float(Top + Glyph->OffsetY) - 0.5f;
			List.SetUV(FGSUV{GSToFixed4(float(Glyph->U), 14), GSToFixed4(float(Glyph->V), 14)});
			List.AddVertex(Environment.PixelVertex(Left, GlyphTop));
			List.SetUV(
				FGSUV{GSToFixed4(float(Glyph->U + Glyph->Width), 14), GSToFixed4(float(Glyph->V + Glyph->Height), 14)});
			List.AddVertex(Environment.PixelVertex(Left + float(Glyph->Width), GlyphTop + float(Glyph->Height)));
		}
		PenX += Glyph->Advance;
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
