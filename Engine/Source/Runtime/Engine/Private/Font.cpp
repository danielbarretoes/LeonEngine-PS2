#include "Engine/Font.h"

#include "Engine/Texture2D.h"

namespace
{

	/** Serializes an array of plain structs: its count, then each element by its operator<<. */
	template <typename ElementType>
	void SerializeArray(FArchive& Ar, TArray<ElementType>& Array)
	{
		int32 Num = Array.Num();
		Ar << Num;
		if (Ar.IsLoading())
		{
			if (Num < 0 || Num > (1 << 20) || Ar.IsError())
			{
				Ar.SetCriticalError();
				Array.Empty();
				return;
			}
			Array.Empty(Num);
			Array.AddDefaulted(Num);
		}
		for (ElementType& Element : Array)
		{
			Ar << Element;
		}
	}

} // namespace

FArchive& operator<<(FArchive& Ar, FFontCharacter& Character)
{
	Ar << Character.StartU << Character.StartV << Character.USize << Character.VSize << Character.TextureIndex
	   << Character.VerticalOffset << Character.HorizontalOffset << Character.Advance;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FFontKerningPair& KerningPair)
{
	Ar << KerningPair.Pair << KerningPair.Amount;
	return Ar;
}

UFont::UFont(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFont::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	SerializeArray(Ar, Characters);
	SerializeArray(Ar, KerningPairs);
}

const FFontCharacter* UFont::FindCharacter(uint32 CodePoint) const
{
	if (CodePoint < uint32(Characters.Num()))
	{
		const FFontCharacter& Character = Characters[int32(CodePoint)];
		if (Character.Advance > 0 || Character.USize > 0)
		{
			return &Character;
		}
	}
	return CodePoint != NullCharacter ? FindCharacter(NullCharacter) : nullptr;
}

int32 UFont::GetCharKerning(uint32 First, uint32 Second) const
{
	const uint32 Pair = FFontKerningPair::MakePair(First, Second);
	int32 Low = 0;
	int32 High = KerningPairs.Num() - 1;
	while (Low <= High)
	{
		const int32 Middle = (Low + High) / 2;
		const uint32 MiddlePair = KerningPairs[Middle].Pair;
		if (MiddlePair == Pair)
		{
			return KerningPairs[Middle].Amount + Kerning;
		}
		if (MiddlePair < Pair)
		{
			Low = Middle + 1;
		}
		else
		{
			High = Middle - 1;
		}
	}
	return Kerning;
}

void UFont::GetCharSize(uint32 CodePoint, float& OutWidth, float& OutHeight) const
{
	const FFontCharacter* Character = FindCharacter(CodePoint);
	OutWidth = Character != nullptr ? float(Character->Advance) : 0.0f;
	OutHeight = GetMaxCharHeight();
}

uint32 UFont::DecodeCodePoint(const TCHAR*& Cursor, const TCHAR* End)
{
	const uint8 Lead = uint8(*Cursor++);
	if (Lead < 0x80)
	{
		return Lead;
	}
	// Two- and three-byte sequences (the Basic Multilingual Plane; the fonts stop at Latin-1).
	int32 Continuations = 0;
	uint32 CodePoint = 0;
	if ((Lead & 0xe0) == 0xc0)
	{
		Continuations = 1;
		CodePoint = Lead & 0x1f;
	}
	else if ((Lead & 0xf0) == 0xe0)
	{
		Continuations = 2;
		CodePoint = Lead & 0x0f;
	}
	else
	{
		return Lead;
	}
	if (End - Cursor < Continuations)
	{
		return Lead;
	}
	for (int32 Index = 0; Index < Continuations; ++Index)
	{
		const uint8 Next = uint8(Cursor[Index]);
		if ((Next & 0xc0) != 0x80)
		{
			return Lead;
		}
		CodePoint = (CodePoint << 6) | (Next & 0x3f);
	}
	// An overlong sequence is not UTF-8: its lead byte is Latin-1.
	if (CodePoint < (Continuations == 1 ? 0x80u : 0x800u))
	{
		return Lead;
	}
	Cursor += Continuations;
	return CodePoint;
}

int32 UFont::GetLineWidth(const TCHAR* Text, int32 Count) const
{
	const TCHAR* Cursor = Text;
	const TCHAR* End = Text + Count;
	int32 Width = 0;
	uint32 Previous = 0;
	while (Cursor < End)
	{
		const uint32 CodePoint = DecodeCodePoint(Cursor, End);
		if (const FFontCharacter* Character = FindCharacter(CodePoint))
		{
			if (Previous != 0)
			{
				Width += GetCharKerning(Previous, CodePoint);
			}
			Width += Character->Advance;
		}
		Previous = CodePoint;
	}
	return Width;
}

int32 UFont::GetStringSize(const TCHAR* Text) const
{
	int32 Longest = 0;
	const TCHAR* Line = Text;
	for (;;)
	{
		const TCHAR* End = Line;
		while (*End != '\0' && *End != '\n')
		{
			++End;
		}
		Longest = FMath::Max(Longest, GetLineWidth(Line, int32(End - Line)));
		if (*End == '\0')
		{
			return Longest;
		}
		Line = End + 1;
	}
}

int32 UFont::GetStringHeightSize(const TCHAR* Text) const
{
	int32 Lines = 1;
	for (const TCHAR* Cursor = Text; *Cursor != '\0'; ++Cursor)
	{
		Lines += *Cursor == '\n' ? 1 : 0;
	}
	return FMath::RoundToInt(GetLineHeight() * float(Lines));
}
