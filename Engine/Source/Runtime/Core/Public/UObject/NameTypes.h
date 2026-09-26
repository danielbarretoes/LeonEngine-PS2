#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Templates/TypeHash.h"

// Names (UE: UObject/NameTypes.h). An FName is 8 bytes: the id of an entry in the global name pool plus a number
// suffix. Comparison ignores case; the pool keeps the casing of the first registration ("Foo" then "FOO" both
// display "Foo"). "Actor_7" is stored as ("Actor", 7 + 1).

/** Hard-coded names, registered in order at pool creation (UE: EName). */
enum EName : uint32
{
#define REGISTER_NAME(Num, Name) NAME_##Name = Num,
#include "UObject/UnrealNames.inl"
#undef REGISTER_NAME
	NAME_MaxHardcodedNameIndex,
};

/** Maximum length of a name including the terminator (UE: NAME_SIZE). */
enum
{
	NAME_SIZE = 1024
};

/** Find an existing name only, or add it (UE: EFindName). */
enum EFindName
{
	FNAME_Find,
	FNAME_Add,
};

/** How IsEqual compares the string part (UE: ENameCase). */
enum class ENameCase : uint8
{
	CaseSensitive,
	IgnoreCase,
};

/** Internal number 0 means "no number"; internal N means external N - 1 (UE). */
#define NAME_NO_NUMBER_INTERNAL 0
#define NAME_EXTERNAL_TO_INTERNAL(x) ((x) + 1)
#define NAME_INTERNAL_TO_EXTERNAL(x) ((x) - 1)
#define NAME_NO_NUMBER NAME_INTERNAL_TO_EXTERNAL(NAME_NO_NUMBER_INTERNAL)

/** Opaque id of a name pool entry (UE: FNameEntryId). 0 is "None". */
struct FNameEntryId
{
	FNameEntryId()
		: Value(0)
	{
	}

	explicit FNameEntryId(ENoInit)
	{
	}

	FORCEINLINE bool IsNone() const
	{
		return Value == 0;
	}

	FORCEINLINE bool operator==(FNameEntryId Rhs) const
	{
		return Value == Rhs.Value;
	}
	FORCEINLINE bool operator!=(FNameEntryId Rhs) const
	{
		return Value != Rhs.Value;
	}
	/** Fast, non-lexical order (entry creation order). */
	FORCEINLINE bool operator<(FNameEntryId Rhs) const
	{
		return Value < Rhs.Value;
	}

	/** The raw value; not stable across runs (UE: ToUnstableInt). */
	FORCEINLINE uint32 ToUnstableInt() const
	{
		return Value;
	}

	static FORCEINLINE FNameEntryId FromUnstableInt(uint32 UnstableInt)
	{
		FNameEntryId Id;
		Id.Value = UnstableInt;
		return Id;
	}

	FORCEINLINE friend uint32 GetTypeHash(FNameEntryId Id)
	{
		return Id.Value;
	}

private:
	uint32 Value;
};

/** A name pool entry: the characters of a name without its number (UE: FNameEntry). */
struct CORE_API FNameEntry
{
	/** Length without the terminator. */
	FORCEINLINE int32 GetNameLength() const
	{
		return Length;
	}

	/** The characters (null-terminated). */
	FORCEINLINE const TCHAR* GetUnterminatedName() const
	{
		return Chars;
	}

	FString GetPlainNameString() const
	{
		return FString(Length, Chars);
	}

	void AppendNameToString(FString& Out) const
	{
		Out.Append(Chars, Length);
	}

	/** Pool chaining (internal). */
	uint32 NextInBucket;
	uint32 Hash;
	uint16 Length;
	TCHAR Chars[1];
};

/** Case-insensitive interned string with a number suffix (UE: FName). */
class CORE_API FName
{
public:
	FORCEINLINE FName()
		: Number(NAME_NO_NUMBER_INTERNAL)
	{
	}

	explicit FName(ENoInit)
		: ComparisonIndex(NoInit)
	{
	}

	FName(EName Name)
		: ComparisonIndex(GetHardcodedEntryId(Name))
		, Number(NAME_NO_NUMBER_INTERNAL)
	{
	}

	/** Hard-coded name with an internal number. */
	FName(EName Name, int32 InNumber)
		: ComparisonIndex(GetHardcodedEntryId(Name))
		, Number(uint32(InNumber))
	{
	}

	/** Parses "Base_N" into ("Base", N + 1). FNAME_Find returns NAME_None when the base name does not exist. */
	FName(const TCHAR* Name, EFindName FindType = FNAME_Add);

	/** Len characters of Name. */
	FName(int32 Len, const TCHAR* Name, EFindName FindType = FNAME_Add);

	/** Name with an explicit internal number (the string is not parsed for a suffix). */
	FName(const TCHAR* Name, int32 InNumber, EFindName FindType = FNAME_Add);

	/** Other's string with a different internal number. */
	FORCEINLINE FName(FName Other, int32 InNumber)
		: ComparisonIndex(Other.ComparisonIndex)
		, Number(uint32(InNumber))
	{
	}

	FORCEINLINE FName(FNameEntryId InComparisonIndex, FNameEntryId /*InDisplayIndex*/, int32 InNumber)
		: ComparisonIndex(InComparisonIndex)
		, Number(uint32(InNumber))
	{
	}

	FORCEINLINE FNameEntryId GetComparisonIndex() const
	{
		return ComparisonIndex;
	}

	FORCEINLINE FNameEntryId GetDisplayIndex() const
	{
		return ComparisonIndex;
	}

	/** Internal number (0 = none). */
	FORCEINLINE int32 GetNumber() const
	{
		return int32(Number);
	}

	FORCEINLINE void SetNumber(const int32 NewNumber)
	{
		Number = uint32(NewNumber);
	}

	/** The pool entry of the string part. */
	const FNameEntry* GetDisplayNameEntry() const;

	/** The string part, without the number. */
	FString GetPlainNameString() const;

	/** Copies the string part into a fixed buffer. */
	void GetPlainANSIString(ANSICHAR (&AnsiName)[NAME_SIZE]) const;

	/** "Base" or "Base_N". */
	FString ToString() const;
	void ToString(FString& Out) const;
	void AppendString(FString& Out) const;

	/** Same entry and number. */
	FORCEINLINE bool operator==(FName Other) const
	{
		return ComparisonIndex == Other.ComparisonIndex && Number == Other.Number;
	}
	FORCEINLINE bool operator!=(FName Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE bool operator==(EName Name) const
	{
		return ComparisonIndex == GetHardcodedEntryId(Name) && Number == NAME_NO_NUMBER_INTERNAL;
	}
	FORCEINLINE bool operator!=(EName Name) const
	{
		return !(*this == Name);
	}

	/** Compares with a string ("Base_N" included), ignoring case. */
	bool operator==(const TCHAR* Other) const;
	FORCEINLINE bool operator!=(const TCHAR* Other) const
	{
		return !(*this == Other);
	}

	/** Leon keeps one casing per name (UE without WITH_CASE_PRESERVING_NAME): CaseSensitive compares like IgnoreCase.
	 */
	bool IsEqual(const FName& Other, const ENameCase CompareMethod = ENameCase::IgnoreCase,
		const bool bCompareNumber = true) const;

	/** Lexical, case-insensitive order, then the number; stable across runs. */
	int32 Compare(const FName& Other) const;

	/** Entry-id order: fast but depends on registration order. */
	FORCEINLINE int32 CompareIndexes(const FName& Other) const
	{
		if (ComparisonIndex != Other.ComparisonIndex)
		{
			return ComparisonIndex < Other.ComparisonIndex ? -1 : 1;
		}
		return int32(Number) - int32(Other.Number);
	}

	FORCEINLINE bool FastLess(const FName& Other) const
	{
		return CompareIndexes(Other) < 0;
	}

	FORCEINLINE bool LexicalLess(const FName& Other) const
	{
		return Compare(Other) < 0;
	}

	/** Lexical order (deterministic sorting). */
	FORCEINLINE bool operator<(const FName& Other) const
	{
		return Compare(Other) < 0;
	}

	FORCEINLINE bool IsNone() const
	{
		return ComparisonIndex.IsNone() && Number == NAME_NO_NUMBER_INTERNAL;
	}

	/** True when the entry id belongs to the pool. */
	bool IsValid() const;

	FORCEINLINE friend uint32 GetTypeHash(FName Name)
	{
		return GetTypeHash(Name.ComparisonIndex) + Name.Number;
	}

	/** Bytes of pool blocks in use (budgets / stats). */
	static int32 GetNameEntryMemorySize();

	/** Bytes of pool blocks allocated plus the hash table. */
	static int32 GetNameTableMemorySize();

	/** Number of pool entries (distinct strings). */
	static int32 GetNumNames();

	/** Pool entry id of a hard-coded name. */
	static FNameEntryId GetHardcodedEntryId(EName Name);

private:
	void Init(const TCHAR* Name, int32 Len, int32 InNumber, EFindName FindType, bool bParseNumber);

	FNameEntryId ComparisonIndex;
	uint32 Number;
};

static_assert(sizeof(FName) == 8, "FName must stay 8 bytes");

FORCEINLINE bool operator==(EName Lhs, FName Rhs)
{
	return Rhs == Lhs;
}
FORCEINLINE bool operator!=(EName Lhs, FName Rhs)
{
	return Rhs != Lhs;
}

template <>
struct TIsZeroConstructType<FName>
{
	enum
	{
		Value = true
	};
};

inline FString LexToString(const FName& Name)
{
	return Name.ToString();
}

inline void LexFromString(FName& Name, const TCHAR* Str)
{
	Name = FName(Str);
}

/** Predicate for sorting FNames lexically (UE: FNameLexicalLess). */
struct FNameLexicalLess
{
	FORCEINLINE bool operator()(const FName& A, const FName& B) const
	{
		return A.LexicalLess(B);
	}
};

/** Predicate for sorting FNames by entry id (UE: FNameFastLess). */
struct FNameFastLess
{
	FORCEINLINE bool operator()(const FName& A, const FName& B) const
	{
		return A.FastLess(B);
	}
};
