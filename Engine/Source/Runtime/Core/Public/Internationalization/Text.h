#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "UObject/NameTypes.h"

#include <initializer_list>

/**
 * Display text (UE: FText, reduced). Leon has no localization yet: an FText wraps its display string, LOCTEXT /
 * NSLOCTEXT keep only the source text, and Format replaces "{0}", "{1}"... with the arguments.
 */
class CORE_API FText
{
public:
	FText() = default;

	static const FText& GetEmpty();

	static FText FromString(const FString& String);
	static FText FromString(FString&& String);
	static FText FromName(const FName& Name);

	/** Text that is the same in every culture (UE: AsCultureInvariant). */
	static FText AsCultureInvariant(const FString& String);

	static FText AsNumber(int32 Value);
	static FText AsNumber(int64 Value);
	static FText AsNumber(uint32 Value);
	static FText AsNumber(float Value, int32 MaximumFractionalDigits = 3);
	static FText AsNumber(double Value, int32 MaximumFractionalDigits = 3);

	/** Percentage of a 0..1 value, rounded ("42%"). */
	static FText AsPercent(float Value);

	/** "Hello {0}, you have {1} points" with ordered arguments (UE: FText::Format). */
	static FText Format(const FText& Pattern, const TArray<FText>& Arguments);
	static FText Format(const FText& Pattern, std::initializer_list<FText> Arguments);

	template <typename... ArgTypes>
	static FText Format(const FText& Pattern, const FText& First, const ArgTypes&... Rest)
	{
		return Format(Pattern, {First, Rest...});
	}

	/** Joins texts with a delimiter (UE: FText::Join). */
	static FText Join(const FText& Delimiter, const TArray<FText>& Args);

	const FString& ToString() const
	{
		return DisplayString;
	}

	bool IsEmpty() const
	{
		return DisplayString.IsEmpty();
	}

	bool IsEmptyOrWhitespace() const;

	/** Same display string, case-sensitive (UE: EqualTo). */
	bool EqualTo(const FText& Other) const
	{
		return DisplayString.Equals(Other.DisplayString, ESearchCase::CaseSensitive);
	}

	bool EqualToCaseIgnored(const FText& Other) const
	{
		return DisplayString.Equals(Other.DisplayString, ESearchCase::IgnoreCase);
	}

	/** Case-sensitive ordering (UE: CompareTo). */
	int32 CompareTo(const FText& Other) const
	{
		return DisplayString.Compare(Other.DisplayString, ESearchCase::CaseSensitive);
	}

	FText ToUpper() const
	{
		return FText::FromString(DisplayString.ToUpper());
	}

	FText ToLower() const
	{
		return FText::FromString(DisplayString.ToLower());
	}

	FText TrimPrecedingAndTrailing() const
	{
		return FText::FromString(DisplayString.TrimStartAndEnd());
	}

	/** Leon has no culture data: every text is culture-invariant. */
	bool IsCultureInvariant() const
	{
		return true;
	}

private:
	explicit FText(FString&& InDisplayString)
		: DisplayString(MoveTemp(InDisplayString))
	{
	}

	FString DisplayString;
};

inline FString LexToString(const FText& Text)
{
	return Text.ToString();
}

/** Localized text literal (UE: LOCTEXT); requires LOCTEXT_NAMESPACE like UE, the text is used as is. */
#define LOCTEXT(InKey, InTextLiteral) FText::AsCultureInvariant(TEXT(InTextLiteral))

/** Localized text literal with an explicit namespace (UE: NSLOCTEXT). */
#define NSLOCTEXT(InNamespace, InKey, InTextLiteral) FText::AsCultureInvariant(TEXT(InTextLiteral))

/** Text that is never localized (UE: INVTEXT). */
#define INVTEXT(InTextLiteral) FText::AsCultureInvariant(TEXT(InTextLiteral))
