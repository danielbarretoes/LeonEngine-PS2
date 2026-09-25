#include "Engine/EngineBaseTypes.h"
#include "GameMapsSettings.h"
#include "Misc/CString.h"

namespace
{

	/** The key part of an option: the text before '=', or all of it. */
	[[nodiscard]] int32 OptionKeyLength(const TCHAR* Option)
	{
		const TCHAR* Equals = FCString::Strchr(Option, '=');
		return Equals != nullptr ? static_cast<int32>(Equals - Option) : FCString::Strlen(Option);
	}

	/** Option starts with the key Key (Key has no '='), followed by '=' or the end. */
	[[nodiscard]] bool OptionHasKey(const FString& Option, const TCHAR* Key, int32 KeyLength)
	{
		if (Option.Len() < KeyLength || FCString::Strnicmp(*Option, Key, KeyLength) != 0)
		{
			return false;
		}
		return Option.Len() == KeyLength || (*Option)[KeyLength] == '=';
	}

} // namespace

FURL::FURL(const TCHAR* Filename)
	: Map(Filename != nullptr ? FString(Filename) : FString())
{
}

FURL::FURL(const FURL* Base, const TCHAR* TextURL, ETravelType Type)
	: Map(UGameMapsSettings::GetGameDefaultMap())
{
	if (Base != nullptr && Type == TRAVEL_Relative)
	{
		Map = Base->Map;
		Portal = Base->Portal;
	}
	if (Base != nullptr && (Type == TRAVEL_Relative || Type == TRAVEL_Partial))
	{
		// The base's options carry over, except the ones that only apply to one travel (UE).
		for (const FString& Option : Base->Op)
		{
			if (!Option.Equals(TEXT("Restart"), ESearchCase::IgnoreCase) &&
				!Option.Equals(TEXT("Quiet"), ESearchCase::IgnoreCase))
			{
				Op.Add(Option);
			}
		}
	}

	FString Text = TextURL != nullptr ? FString(TextURL) : FString();
	Text.TrimStartInline();

	// The options and the portal: "Map?A=1?B#Portal?C".
	int32 FirstMarker = INDEX_NONE;
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		if (Text[Index] == '?' || Text[Index] == '#')
		{
			FirstMarker = Index;
			break;
		}
	}
	FString MapText = FirstMarker == INDEX_NONE ? Text : Text.Left(FirstMarker);
	if (FirstMarker != INDEX_NONE)
	{
		const FString Rest = Text.Mid(FirstMarker);
		int32 Start = 0;
		while (Start < Rest.Len())
		{
			const TCHAR Marker = Rest[Start];
			int32 End = Start + 1;
			while (End < Rest.Len() && Rest[End] != '?' && Rest[End] != '#')
			{
				++End;
			}
			const FString Part = Rest.Mid(Start + 1, End - Start - 1);
			if (Marker == '#')
			{
				Portal = Part;
			}
			else if (!Part.IsEmpty())
			{
				AddOption(*Part);
			}
			Start = End;
		}
	}

	// A long package name, a file name ("C:/...", a '/' path that is not a package name) or a content key: the map as
	// it is (UE parses a host and a protocol here; Leon has no networking).
	MapText.TrimEndInline();
	if (!MapText.IsEmpty())
	{
		Map = MapText;
	}
	Valid = 1;
}

bool FURL::HasOption(const TCHAR* Test) const
{
	return GetOption(Test, nullptr) != nullptr;
}

const TCHAR* FURL::GetOption(const TCHAR* Match, const TCHAR* Default) const
{
	const int32 Length = FCString::Strlen(Match);
	if (Length > 0)
	{
		for (const FString& Option : Op)
		{
			const TCHAR* Chars = *Option;
			if (FCString::Strnicmp(Chars, Match, Length) == 0 &&
				(Chars[Length - 1] == '=' || Chars[Length] == '=' || Chars[Length] == '\0'))
			{
				return Chars + Length;
			}
		}
	}
	return Default;
}

void FURL::AddOption(const TCHAR* Str)
{
	const int32 KeyLength = OptionKeyLength(Str);
	for (FString& Option : Op)
	{
		if (OptionHasKey(Option, Str, KeyLength))
		{
			Option = Str;
			return;
		}
	}
	Op.Add(FString(Str));
}

void FURL::RemoveOption(const TCHAR* Key)
{
	const int32 KeyLength = OptionKeyLength(Key);
	Op.RemoveAll([Key, KeyLength](const FString& Option) { return OptionHasKey(Option, Key, KeyLength); });
}

FString FURL::ToString(bool /*FullyQualified*/) const
{
	FString Result = Map;
	for (const FString& Option : Op)
	{
		Result += TEXT("?");
		Result += Option;
	}
	if (!Portal.IsEmpty())
	{
		Result += TEXT("#");
		Result += Portal;
	}
	return Result;
}

bool FURL::operator==(const FURL& Other) const
{
	if (!Map.Equals(Other.Map, ESearchCase::IgnoreCase) || !Portal.Equals(Other.Portal, ESearchCase::IgnoreCase) ||
		Op.Num() != Other.Op.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Op.Num(); ++Index)
	{
		if (!Op[Index].Equals(Other.Op[Index], ESearchCase::IgnoreCase))
		{
			return false;
		}
	}
	return true;
}
