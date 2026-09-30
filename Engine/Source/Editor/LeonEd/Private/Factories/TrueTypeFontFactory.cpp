#include "Factories/TrueTypeFontFactory.h"

#include "Engine/Texture2D.h"
#include "GSTextureLayout.h"
#include "LeonEdLog.h"
#include "UObject/Package.h"

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable : 4244 4456 4457 4505 4701 4702) // stb_truetype's conversions, shadowing, unused and dead
// code
#endif
// The engine's only TrueType reader: internal to this file, from memory only, through the engine's allocator.
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#define STBTT_malloc(Size, User) ((void)(User), FMemory::Malloc(Size))
#define STBTT_free(Pointer, User) ((void)(User), FMemory::Free(Pointer))
#define STBTT_assert(Condition) check(Condition)
#include <stb_truetype.h>
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

namespace
{

	/** The code points a font keeps: UFont indexes its characters by code point. */
	constexpr uint32 MaxCodePoint = 255;
	/** The coverage levels of a page (PF_P4's 16 CLUT entries). */
	constexpr int32 CoverageLevels = 16;

	/** A glyph's rectangle to place in a page. */
	struct FGlyphBox
	{
		uint32 CodePoint = 0;
		int32 Width = 0;
		int32 Height = 0;
		/** Its coverage (0 to 15), Width x Height, top row first. */
		TArray<uint8> Coverage;
	};

	/** Where a glyph went: its page and its top-left texel there. */
	struct FPlacement
	{
		int32 Page = INDEX_NONE;
		int32 X = 0;
		int32 Y = 0;
	};

	/**
	 * Places Boxes[Order[First...]] on shelves in a Width x Height page, in order, until one does not fit; returns how
	 * many went. Each glyph keeps XPadding / YPadding empty texels to its left and above, and the page's right and
	 * bottom edges keep them too.
	 */
	int32 PackShelves(const TArray<FGlyphBox>& Boxes, const TArray<int32>& Order, int32 First, int32 Width,
		int32 Height, int32 XPadding, int32 YPadding, int32 Page, TArray<FPlacement>* OutPlacements)
	{
		int32 X = XPadding;
		int32 Y = YPadding;
		int32 ShelfHeight = 0;
		int32 Placed = 0;
		for (int32 Index = First; Index < Order.Num(); ++Index)
		{
			const FGlyphBox& Box = Boxes[Order[Index]];
			if (X + Box.Width + XPadding > Width)
			{
				X = XPadding;
				Y += ShelfHeight + YPadding;
				ShelfHeight = 0;
			}
			if (Box.Width + (2 * XPadding) > Width || Y + Box.Height + YPadding > Height)
			{
				break;
			}
			if (OutPlacements != nullptr)
			{
				FPlacement& Placement = (*OutPlacements)[Order[Index]];
				Placement.Page = Page;
				Placement.X = X;
				Placement.Y = Y;
			}
			X += Box.Width + XPadding;
			ShelfHeight = FMath::Max(ShelfHeight, Box.Height);
			++Placed;
		}
		return Placed;
	}

	/** A hexadecimal number of Text; false when it is not one. */
	bool ParseHex(const FString& Text, uint32& OutValue)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.Len() > 6)
		{
			return false;
		}
		OutValue = 0;
		for (int32 Index = 0; Index < Trimmed.Len(); ++Index)
		{
			const TCHAR Char = Trimmed[Index];
			uint32 Digit = 0;
			if (Char >= '0' && Char <= '9')
			{
				Digit = uint32(Char - '0');
			}
			else if (Char >= 'a' && Char <= 'f')
			{
				Digit = uint32(Char - 'a' + 10);
			}
			else if (Char >= 'A' && Char <= 'F')
			{
				Digit = uint32(Char - 'A' + 10);
			}
			else
			{
				return false;
			}
			OutValue = (OutValue << 4) | Digit;
		}
		return true;
	}

	/** The family name of the font's name table (name ID 1, Windows Unicode English, then Mac Roman). */
	FString GetFamilyName(const stbtt_fontinfo& Info)
	{
		int32 Length = 0;
		if (const char* Name = stbtt_GetFontNameString(
				&Info, &Length, STBTT_PLATFORM_ID_MICROSOFT, STBTT_MS_EID_UNICODE_BMP, STBTT_MS_LANG_ENGLISH, 1))
		{
			// UTF-16 big endian: the low bytes of the ASCII family name.
			FString Out;
			for (int32 Index = 1; Index < Length; Index += 2)
			{
				Out.AppendChar(TCHAR(Name[Index]));
			}
			return Out;
		}
		if (const char* Name = stbtt_GetFontNameString(
				&Info, &Length, STBTT_PLATFORM_ID_MAC, STBTT_MAC_EID_ROMAN, STBTT_MAC_LANG_ENGLISH, 1))
		{
			FString Out;
			for (int32 Index = 0; Index < Length; ++Index)
			{
				Out.AppendChar(TCHAR(Name[Index]));
			}
			return Out;
		}
		return FString();
	}

} // namespace

UTrueTypeFontFactory::UTrueTypeFontFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UFont::StaticClass();
	Formats.Add(TEXT("ttf;TrueType font"));
	bEditorImport = 1;
}

bool UTrueTypeFontFactory::ParseUnicodeRange(const FString& Range, TArray<uint32>& OutCodePoints)
{
	OutCodePoints.Reset();
	TArray<FString> Parts;
	Range.ParseIntoArray(Parts, TEXT(","), true);
	for (const FString& Part : Parts)
	{
		FString FirstText;
		FString LastText;
		if (!Part.Split(TEXT("-"), &FirstText, &LastText))
		{
			FirstText = Part;
			LastText = Part;
		}
		uint32 First = 0;
		uint32 Last = 0;
		if (!ParseHex(FirstText, First) || !ParseHex(LastText, Last) || Last < First)
		{
			return false;
		}
		for (uint32 CodePoint = First; CodePoint <= FMath::Min(Last, MaxCodePoint); ++CodePoint)
		{
			OutCodePoints.AddUnique(CodePoint);
		}
	}
	OutCodePoints.Sort();
	return OutCodePoints.Num() > 0;
}

bool UTrueTypeFontFactory::RasterizeFont(const uint8* Buffer, int64 Size, FRasterizedFont& Out, FString& OutError) const
{
	Out = FRasterizedFont();
	if (Buffer == nullptr || Size < 12 || Size > int64(MAX_int32))
	{
		OutError = TEXT("empty font file");
		return false;
	}
	if (Height < 4 || Height > 128)
	{
		OutError = FString::Printf("Height %d is outside 4 to 128 pixels", Height);
		return false;
	}
	if (TexturePageWidth < 8 || TexturePageWidth > 256 || !FMath::IsPowerOfTwo(TexturePageWidth) ||
		TexturePageMaxHeight < 8 || TexturePageMaxHeight > 256 || !FMath::IsPowerOfTwo(TexturePageMaxHeight))
	{
		OutError = TEXT("the page sizes must be powers of two from 8 to 256");
		return false;
	}
	TArray<uint32> CodePoints;
	if (!ParseUnicodeRange(UnicodeRange, CodePoints))
	{
		OutError = FString::Printf("bad UnicodeRange '%s'", *UnicodeRange);
		return false;
	}
	stbtt_fontinfo Info;
	const int32 Offset = stbtt_GetFontOffsetForIndex(Buffer, 0);
	if (Offset < 0 || stbtt_InitFont(&Info, Buffer, Offset) == 0)
	{
		OutError = TEXT("not a TrueType font");
		return false;
	}
	Out.FamilyName = GetFamilyName(Info);

	// The scale that makes the ascent plus the descent Height pixels, and the metrics in whole pixels.
	const float Scale = stbtt_ScaleForPixelHeight(&Info, float(Height));
	int32 FontAscent = 0;
	int32 FontDescent = 0;
	int32 FontLineGap = 0;
	stbtt_GetFontVMetrics(&Info, &FontAscent, &FontDescent, &FontLineGap);
	const int32 Ascent = FMath::RoundToInt(float(FontAscent) * Scale);
	Out.Ascent = float(Ascent);
	Out.Descent = float(FMath::RoundToInt(float(-FontDescent) * Scale));
	Out.Leading = float(FMath::RoundToInt(float(FontLineGap) * Scale));

	// Each glyph: its advance and bearing, and its coverage quantized to the page's 16 levels.
	Out.Characters.SetNum(int32(MaxCodePoint) + 1);
	TArray<FGlyphBox> Boxes;
	TArray<uint32> Kept;
	TArray<int32> GlyphIndices;
	TArray<uint8> Bitmap;
	for (const uint32 CodePoint : CodePoints)
	{
		const int32 Glyph = stbtt_FindGlyphIndex(&Info, int32(CodePoint));
		if (Glyph == 0)
		{
			continue;
		}
		int32 AdvanceWidth = 0;
		int32 LeftSideBearing = 0;
		stbtt_GetGlyphHMetrics(&Info, Glyph, &AdvanceWidth, &LeftSideBearing);
		int32 X0 = 0;
		int32 Y0 = 0;
		int32 X1 = 0;
		int32 Y1 = 0;
		stbtt_GetGlyphBitmapBox(&Info, Glyph, Scale, Scale, &X0, &Y0, &X1, &Y1);
		FFontCharacter& Character = Out.Characters[int32(CodePoint)];
		Character.Advance = FMath::Max(1, FMath::RoundToInt(float(AdvanceWidth) * Scale));
		Character.HorizontalOffset = X0;
		Character.VerticalOffset = Ascent + Y0;
		Kept.Add(CodePoint);
		GlyphIndices.Add(Glyph);
		const int32 Width = X1 - X0;
		const int32 GlyphHeight = Y1 - Y0;
		if (Width <= 0 || GlyphHeight <= 0)
		{
			continue;
		}
		Bitmap.SetNumZeroed(Width * GlyphHeight);
		stbtt_MakeGlyphBitmap(&Info, Bitmap.GetData(), Width, GlyphHeight, Width, Scale, Scale, Glyph);
		FGlyphBox& Box = Boxes.AddDefaulted_GetRef();
		Box.CodePoint = CodePoint;
		Box.Width = Width;
		Box.Height = GlyphHeight;
		Box.Coverage.SetNumUninitialized(Width * GlyphHeight);
		for (int32 Index = 0; Index < Bitmap.Num(); ++Index)
		{
			Box.Coverage[Index] = uint8(((int32(Bitmap[Index]) * (CoverageLevels - 1)) + 127) / 255);
		}
		Character.USize = Width;
		Character.VSize = GlyphHeight;
	}
	if (Kept.Num() == 0)
	{
		OutError = TEXT("the font has none of the characters of UnicodeRange");
		return false;
	}

	// The kerning pairs of the kept characters, rounded (the font's kern table or GPOS pair adjustments).
	for (int32 First = 0; First < Kept.Num(); ++First)
	{
		for (int32 Second = 0; Second < Kept.Num(); ++Second)
		{
			const int32 Kern = stbtt_GetGlyphKernAdvance(&Info, GlyphIndices[First], GlyphIndices[Second]);
			const int32 Amount = Kern != 0 ? FMath::RoundToInt(float(Kern) * Scale) : 0;
			if (Amount != 0)
			{
				Out.KerningPairs.Add(FFontKerningPair{FFontKerningPair::MakePair(Kept[First], Kept[Second]), Amount});
			}
		}
	}

	// Pages: the glyphs tallest first on shelves; the smallest power-of-two page that holds the rest, else a full
	// page and the next ones.
	TArray<int32> Order;
	for (int32 Index = 0; Index < Boxes.Num(); ++Index)
	{
		Order.Add(Index);
	}
	Order.Sort(
		[&Boxes](int32 A, int32 B)
		{
			if (Boxes[A].Height != Boxes[B].Height)
			{
				return Boxes[A].Height > Boxes[B].Height;
			}
			if (Boxes[A].Width != Boxes[B].Width)
			{
				return Boxes[A].Width > Boxes[B].Width;
			}
			return Boxes[A].CodePoint < Boxes[B].CodePoint;
		});
	TArray<FPlacement> Placements;
	Placements.SetNum(Boxes.Num());
	int32 Next = 0;
	while (Next < Order.Num())
	{
		int32 PageWidth = TexturePageWidth;
		int32 PageHeight = TexturePageMaxHeight;
		bool bFound = false;
		// The candidates by area, then the wider first (a page holds whole shelves).
		for (int32 Area = 64; Area <= TexturePageWidth * TexturePageMaxHeight && !bFound; Area *= 2)
		{
			for (int32 Width = TexturePageWidth; Width >= 8 && !bFound; Width /= 2)
			{
				const int32 CandidateHeight = Area / Width;
				if (CandidateHeight < 8 || CandidateHeight > TexturePageMaxHeight || CandidateHeight > Width)
				{
					continue;
				}
				if (PackShelves(Boxes, Order, Next, Width, CandidateHeight, XPadding, YPadding, 0, nullptr) ==
					Order.Num() - Next)
				{
					PageWidth = Width;
					PageHeight = CandidateHeight;
					bFound = true;
				}
			}
		}
		const int32 Page = Out.Pages.Num();
		const int32 Placed =
			PackShelves(Boxes, Order, Next, PageWidth, PageHeight, XPadding, YPadding, Page, &Placements);
		if (Placed == 0)
		{
			OutError = FString::Printf("a glyph does not fit in a %dx%d page", PageWidth, PageHeight);
			return false;
		}
		FRasterizedFont::FPage& NewPage = Out.Pages.AddDefaulted_GetRef();
		NewPage.Width = PageWidth;
		NewPage.Height = PageHeight;
		NewPage.Coverage.SetNumZeroed(PageWidth * PageHeight);
		Next += Placed;
	}
	for (int32 Index = 0; Index < Boxes.Num(); ++Index)
	{
		const FGlyphBox& Box = Boxes[Index];
		const FPlacement& Placement = Placements[Index];
		FRasterizedFont::FPage& Page = Out.Pages[Placement.Page];
		for (int32 Row = 0; Row < Box.Height; ++Row)
		{
			FMemory::Memcpy(&Page.Coverage[((Placement.Y + Row) * Page.Width) + Placement.X],
				&Box.Coverage[Row * Box.Width], SIZE_T(Box.Width));
		}
		FFontCharacter& Character = Out.Characters[int32(Box.CodePoint)];
		Character.StartU = Placement.X;
		Character.StartV = Placement.Y;
		Character.TextureIndex = uint8(Placement.Page);
	}
	return true;
}

void UTrueTypeFontFactory::FillFont(UFont& Font, const FRasterizedFont& Rasterized, int32 InHeight)
{
	Font.Ascent = Rasterized.Ascent;
	Font.Descent = Rasterized.Descent;
	Font.Leading = Rasterized.Leading;
	Font.Kerning = 0;
	Font.LegacyFontSize = InHeight;
	Font.LegacyFontName = FName(*Rasterized.FamilyName);
	Font.Characters = Rasterized.Characters;
	Font.KerningPairs = Rasterized.KerningPairs;

	// The CLUT: white, its alpha the coverage (16 levels).
	TArray<uint32> Palette;
	for (int32 Level = 0; Level < CoverageLevels; ++Level)
	{
		Palette.Add(0x00ffffffu | (uint32((Level * 255) / (CoverageLevels - 1)) << 24));
	}
	TArray<uint8> ClutImage;
	uint16 ClutWidth = 0;
	uint16 ClutHeight = 0;
	FGSTextureLayout::MakeClutImage(Palette, ClutImage, ClutWidth, ClutHeight);

	// The pages, reused by name on a reimport (the same subobjects, the same bytes); pages the font no longer has
	// leave it.
	TArray<UTexture2D*> Pages;
	for (int32 PageIndex = 0; PageIndex < Rasterized.Pages.Num(); ++PageIndex)
	{
		const FRasterizedFont::FPage& Page = Rasterized.Pages[PageIndex];
		const FString Name = FString::Printf("Texture%d", PageIndex);
		UTexture2D* Texture = FindObjectFast<UTexture2D>(&Font, FName(*Name));
		if (Texture == nullptr)
		{
			Texture =
				NewObject<UTexture2D>(&Font, FName(*Name), Font.HasAnyFlags(RF_Transient) ? RF_Transient : RF_NoFlags);
		}
		Texture->SRGB = 0;
		// PF_P4: the CLUT image, then the indices bottom row first, the first texel of a byte in its low nibble.
		TArray<uint8> Data = ClutImage;
		const int32 IndexStart = Data.Num();
		Data.AddZeroed((Page.Width * Page.Height) / 2);
		for (int32 Row = 0; Row < Page.Height; ++Row)
		{
			const int32 SourceRow = Page.Height - 1 - Row;
			for (int32 X = 0; X < Page.Width; ++X)
			{
				const uint8 Level = Page.Coverage[(SourceRow * Page.Width) + X];
				const int32 Texel = (Row * Page.Width) + X;
				Data[IndexStart + (Texel / 2)] |= uint8((Texel & 1) != 0 ? Level << 4 : Level);
			}
		}
		(void)Texture->SetPlatformData(Page.Width, Page.Height, PF_P4, Data.GetData());
		Pages.Add(Texture);
	}
	for (UTexture2D* Old : Font.Textures)
	{
		if (Old != nullptr && !Pages.Contains(Old))
		{
			(void)Old->Rename(nullptr, GetTransientPackage());
		}
	}
	Font.Textures = Pages;
}

UObject* UTrueTypeFontFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled)
{
	(void)InClass;
	(void)Context;
	(void)Type;
	bOutOperationCanceled = false;
	FRasterizedFont Rasterized;
	FString Error;
	if (!RasterizeFont(Buffer, BufferEnd - Buffer, Rasterized, Error))
	{
		UE_LOG(LogLeonEd, Error, "TrueTypeFontFactory: cannot import '%s' (%s)", *CurrentFilename, *Error);
		return nullptr;
	}
	UFont* Font = CreateOrOverwriteAsset<UFont>(InParent, InName, Flags);
	if (Font == nullptr)
	{
		return nullptr;
	}
	FillFont(*Font, Rasterized, Height);
	UpdateAssetImportData(Font, CurrentFilename);
	Buffer = BufferEnd;
	return Font;
}

bool UTrueTypeFontFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return FactoryCanReimport(Obj, OutFilenames);
}

void UTrueTypeFontFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	FactorySetReimportPaths(Obj, NewReimportPaths);
}

EReimportResult::Type UTrueTypeFontFactory::Reimport(UObject* Obj)
{
	return FactoryReimport(Obj);
}

void UTrueTypeFontFactory::GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
{
	FactoryGetAdditionalReimportedObjects(OutObjects);
}
