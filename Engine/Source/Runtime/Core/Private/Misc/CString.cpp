#include "Misc/CString.h"

#include <cstdio>
#include <cstdlib>

template <typename T>
int32 TCString<T>::Atoi(const CharType* String)
{
	return static_cast<int32>(std::strtol(String, nullptr, 10));
}

template <typename T>
int64 TCString<T>::Atoi64(const CharType* String)
{
	return static_cast<int64>(std::strtoll(String, nullptr, 10));
}

template <typename T>
float TCString<T>::Atof(const CharType* String)
{
	return std::strtof(String, nullptr);
}

template <typename T>
double TCString<T>::Atod(const CharType* String)
{
	return std::strtod(String, nullptr);
}

template <typename T>
int32 TCString<T>::Strtoi(const CharType* Start, CharType** End, int32 Base)
{
	return static_cast<int32>(std::strtol(Start, End, Base));
}

template <typename T>
int64 TCString<T>::Strtoi64(const CharType* Start, CharType** End, int32 Base)
{
	return static_cast<int64>(std::strtoll(Start, End, Base));
}

template <typename T>
uint64 TCString<T>::Strtoui64(const CharType* Start, CharType** End, int32 Base)
{
	return static_cast<uint64>(std::strtoull(Start, End, Base));
}

template <typename T>
float TCString<T>::Strtof(const CharType* Start, CharType** End)
{
	return std::strtof(Start, End);
}

template <typename T>
double TCString<T>::Strtod(const CharType* Start, CharType** End)
{
	return std::strtod(Start, End);
}

template <typename T>
bool TCString<T>::ToBool(const CharType* String)
{
	if (Stricmp(String, "true") == 0 || Stricmp(String, "yes") == 0 || Stricmp(String, "on") == 0)
	{
		return true;
	}
	if (Stricmp(String, "false") == 0 || Stricmp(String, "no") == 0 || Stricmp(String, "off") == 0)
	{
		return false;
	}
	return Atoi(String) != 0;
}

template <typename T>
bool TCString<T>::IsNumeric(const CharType* Str)
{
	if (*Str == '-' || *Str == '+')
	{
		Str++;
	}

	bool bHasDot = false;
	bool bHasDigit = false;
	while (*Str != '\0')
	{
		if (*Str == '.')
		{
			if (bHasDot)
			{
				return false;
			}
			bHasDot = true;
		}
		else if (!TChar<CharType>::IsDigit(*Str))
		{
			return false;
		}
		else
		{
			bHasDigit = true;
		}
		++Str;
	}
	return bHasDigit;
}

namespace
{
	constexpr int32 MaxRepeat = 255;

	template <typename CharType>
	const CharType* MakeRepeated(CharType Char)
	{
		static CharType Buffer[MaxRepeat + 1];
		if (Buffer[0] != Char)
		{
			for (int32 Index = 0; Index < MaxRepeat; ++Index)
			{
				Buffer[Index] = Char;
			}
			Buffer[MaxRepeat] = 0;
		}
		return Buffer;
	}
} // namespace

template <typename T>
const typename TCString<T>::CharType* TCString<T>::Spc(int32 NumSpaces)
{
	static const CharType* Spaces = MakeRepeated<CharType>(' ');
	NumSpaces = NumSpaces < 0 ? 0 : (NumSpaces > MaxRepeat ? MaxRepeat : NumSpaces);
	return Spaces + MaxRepeat - NumSpaces;
}

template <typename T>
const typename TCString<T>::CharType* TCString<T>::Tab(int32 NumTabs)
{
	static CharType Tabs[MaxRepeat + 1];
	if (Tabs[0] != '\t')
	{
		for (int32 Index = 0; Index < MaxRepeat; ++Index)
		{
			Tabs[Index] = '\t';
		}
		Tabs[MaxRepeat] = 0;
	}
	NumTabs = NumTabs < 0 ? 0 : (NumTabs > MaxRepeat ? MaxRepeat : NumTabs);
	return Tabs + MaxRepeat - NumTabs;
}

template <typename T>
int32 TCString<T>::GetVarArgs(CharType* Dest, SIZE_T DestSize, const CharType* Fmt, va_list ArgPtr)
{
	if (DestSize == 0)
	{
		return -1;
	}
	const int Result = std::vsnprintf(Dest, DestSize, Fmt, ArgPtr);
	if (Result < 0 || SIZE_T(Result) >= DestSize)
	{
		Dest[DestSize - 1] = 0;
		return -1;
	}
	return Result;
}

template <typename T>
int32 TCString<T>::Snprintf(CharType* Dest, int32 DestSize, const CharType* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	const int32 Result = GetVarArgs(Dest, SIZE_T(DestSize), Fmt, Args);
	va_end(Args);
	return Result;
}

// TCHAR is ANSICHAR (UTF-8): one instantiation serves FCString and FCStringAnsi.
template struct TCString<ANSICHAR>;
