#include "Internationalization/Text.h"

#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"

const FText& FText::GetEmpty()
{
	static const FText Empty;
	return Empty;
}

FText FText::FromString(const FString& String)
{
	return FText(FString(String));
}

FText FText::FromString(FString&& String)
{
	return FText(MoveTemp(String));
}

FText FText::FromName(const FName& Name)
{
	return FText(Name.ToString());
}

FText FText::AsCultureInvariant(const FString& String)
{
	return FText(FString(String));
}

FText FText::AsNumber(int32 Value)
{
	return FText(FString::Printf("%d", Value));
}

FText FText::AsNumber(int64 Value)
{
	return FText(FString::Printf("%lld", (long long)Value));
}

FText FText::AsNumber(uint32 Value)
{
	return FText(FString::Printf("%u", Value));
}

FText FText::AsNumber(float Value, int32 MaximumFractionalDigits)
{
	return AsNumber(double(Value), MaximumFractionalDigits);
}

FText FText::AsNumber(double Value, int32 MaximumFractionalDigits)
{
	FString String = FString::Printf("%.*f", FMath::Clamp(MaximumFractionalDigits, 0, 9), Value);
	if (String.Contains("."))
	{
		// Drop trailing zeros (and the separator when nothing is left), like UE's default number formatting.
		while (String.EndsWith("0"))
		{
			String.LeftChopInline(1, false);
		}
		if (String.EndsWith("."))
		{
			String.LeftChopInline(1, false);
		}
	}
	if (String.Equals("-0"))
	{
		String = "0";
	}
	return FText(MoveTemp(String));
}

FText FText::AsPercent(float Value)
{
	return FText(FString::Printf("%d%%", int32(Value * 100.0f + (Value >= 0.0f ? 0.5f : -0.5f))));
}

FText FText::Format(const FText& Pattern, const TArray<FText>& Arguments)
{
	const FString& Source = Pattern.DisplayString;
	FString Result;
	Result.Reserve(Source.Len());

	const int32 Length = Source.Len();
	for (int32 Index = 0; Index < Length; ++Index)
	{
		const TCHAR Char = Source[Index];

		// Backtick escapes a brace: `{ -> {.
		if (Char == TEXT('`') && Index + 1 < Length &&
			(Source[Index + 1] == TEXT('{') || Source[Index + 1] == TEXT('}')))
		{
			Result.AppendChar(Source[++Index]);
			continue;
		}

		if (Char == TEXT('{'))
		{
			int32 End = Index + 1;
			int32 ArgumentIndex = 0;
			bool bValid = End < Length && FChar::IsDigit(Source[End]);
			while (End < Length && FChar::IsDigit(Source[End]))
			{
				ArgumentIndex = ArgumentIndex * 10 + FChar::ConvertCharDigitToInt(Source[End]);
				++End;
			}
			bValid = bValid && End < Length && Source[End] == TEXT('}');
			if (bValid)
			{
				if (Arguments.IsValidIndex(ArgumentIndex))
				{
					Result.Append(Arguments[ArgumentIndex].DisplayString);
				}
				else
				{
					// Unknown argument: keep the placeholder, like UE.
					Result.Append(*Source + Index, End - Index + 1);
				}
				Index = End;
				continue;
			}
		}
		Result.AppendChar(Char);
	}
	return FText(MoveTemp(Result));
}

FText FText::Format(const FText& Pattern, std::initializer_list<FText> Arguments)
{
	TArray<FText> ArgumentArray(Arguments);
	return Format(Pattern, ArgumentArray);
}

FText FText::Join(const FText& Delimiter, const TArray<FText>& Args)
{
	FString Result;
	for (int32 Index = 0; Index < Args.Num(); ++Index)
	{
		if (Index > 0)
		{
			Result.Append(Delimiter.DisplayString);
		}
		Result.Append(Args[Index].DisplayString);
	}
	return FText(MoveTemp(Result));
}

bool FText::IsEmptyOrWhitespace() const
{
	for (TCHAR Char : DisplayString)
	{
		if (!FChar::IsWhitespace(Char))
		{
			return false;
		}
	}
	return true;
}
