#include "Misc/DateTime.h"

#include "HAL/PlatformTime.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"

namespace
{
	const int32 DaysPerMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	const int32 DaysToMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

	/** Julian day number of 0001-01-01 (the Gregorian proleptic calendar's first day). */
	constexpr int64 JulianDayOfEpoch = 1721426;
} // namespace

FDateTime::FDateTime(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second, int32 Millisecond)
{
	checkf(Validate(Year, Month, Day, Hour, Minute, Second, Millisecond),
		"Invalid date %04d-%02d-%02d %02d:%02d:%02d.%03d", Year, Month, Day, Hour, Minute, Second, Millisecond);

	int32 TotalDays = 0;

	if ((Month > 2) && IsLeapYear(Year))
	{
		++TotalDays;
	}

	--Year; // the current year is not a full year yet
	--Month; // the current month is not a full month yet

	TotalDays += Year * 365;
	TotalDays += Year / 4; // leap year day every four years...
	TotalDays -= Year / 100; // ...except every 100 years...
	TotalDays += Year / 400; // ...but also every 400 years
	TotalDays += DaysToMonth[Month]; // days in this year up to last month
	TotalDays += Day - 1; // days in this month minus today

	Ticks = TotalDays * ETimespan::TicksPerDay + Hour * ETimespan::TicksPerHour + Minute * ETimespan::TicksPerMinute +
		Second * ETimespan::TicksPerSecond + Millisecond * ETimespan::TicksPerMillisecond;
}

void FDateTime::GetDate(int32& OutYear, int32& OutMonth, int32& OutDay) const
{
	// Based on FORTRAN code in: Fliegel, H. F. and van Flandern, T. C., Communications of the ACM, Vol. 11, No. 10
	// (October 1968). UE starts from its double Julian day; the integer day number is the same value.
	int64 L = JulianDayOfEpoch + Ticks / ETimespan::TicksPerDay + 68569;
	const int64 N = 4 * L / 146097;
	L = L - (146097 * N + 3) / 4;
	int64 I = 4000 * (L + 1) / 1461001;
	L = L - 1461 * I / 4 + 31;
	int64 J = 80 * L / 2447;
	const int64 K = L - 2447 * J / 80;
	L = J / 11;
	J = J + 2 - 12 * L;
	I = 100 * (N - 49) + I + L;

	OutYear = int32(I);
	OutMonth = int32(J);
	OutDay = int32(K);
}

int32 FDateTime::GetDay() const
{
	int32 Year, Month, Day;
	GetDate(Year, Month, Day);
	return Day;
}

EDayOfWeek FDateTime::GetDayOfWeek() const
{
	// January 1, 0001 was a Monday.
	return EDayOfWeek((Ticks / ETimespan::TicksPerDay) % 7);
}

int32 FDateTime::GetDayOfYear() const
{
	int32 Year, Month, Day;
	GetDate(Year, Month, Day);

	for (int32 CurrentMonth = 1; CurrentMonth < Month; ++CurrentMonth)
	{
		Day += DaysInMonth(Year, CurrentMonth);
	}
	return Day;
}

int32 FDateTime::GetHour12() const
{
	const int32 Hour = GetHour();
	if (Hour < 1)
	{
		return 12;
	}
	if (Hour > 12)
	{
		return (Hour - 12);
	}
	return Hour;
}

int32 FDateTime::GetMonth() const
{
	int32 Year, Month, Day;
	GetDate(Year, Month, Day);
	return Month;
}

int32 FDateTime::GetYear() const
{
	int32 Year, Month, Day;
	GetDate(Year, Month, Day);
	return Year;
}

FString FDateTime::ToString() const
{
	return ToString("%Y.%m.%d-%H.%M.%S");
}

FString FDateTime::ToString(const TCHAR* Format) const
{
	FString Result;
	if (Format == nullptr)
	{
		return Result;
	}

	while (*Format != 0)
	{
		if ((*Format == '%') && (*(++Format) != 0))
		{
			switch (*Format)
			{
				case 'a':
					Result += IsMorning() ? "am" : "pm";
					break;
				case 'A':
					Result += IsMorning() ? "AM" : "PM";
					break;
				case 'd':
					Result += FString::Printf("%02i", GetDay());
					break;
				case 'D':
					Result += FString::Printf("%03i", GetDayOfYear());
					break;
				case 'm':
					Result += FString::Printf("%02i", GetMonth());
					break;
				case 'y':
					Result += FString::Printf("%02i", GetYear() % 100);
					break;
				case 'Y':
					Result += FString::Printf("%04i", GetYear());
					break;
				case 'h':
					Result += FString::Printf("%02i", GetHour12());
					break;
				case 'H':
					Result += FString::Printf("%02i", GetHour());
					break;
				case 'M':
					Result += FString::Printf("%02i", GetMinute());
					break;
				case 'S':
					Result += FString::Printf("%02i", GetSecond());
					break;
				case 's':
					Result += FString::Printf("%03i", GetMillisecond());
					break;
				default:
					Result += *Format;
			}
		}
		else
		{
			Result += *Format;
		}

		// Move to the next one.
		Format++;
	}

	return Result;
}

FString FDateTime::ToIso8601() const
{
	return ToString("%Y-%m-%dT%H:%M:%S.%sZ");
}

int64 FDateTime::ToUnixTimestamp() const
{
	return (Ticks - FDateTime(1970, 1, 1).Ticks) / ETimespan::TicksPerSecond;
}

int32 FDateTime::DaysInMonth(int32 Year, int32 Month)
{
	check((Month >= 1) && (Month <= 12));

	if ((Month == 2) && IsLeapYear(Year))
	{
		return 29;
	}
	return DaysPerMonth[Month];
}

int32 FDateTime::DaysInYear(int32 Year)
{
	return IsLeapYear(Year) ? 366 : 365;
}

bool FDateTime::IsLeapYear(int32 Year)
{
	if ((Year % 4) == 0)
	{
		return (((Year % 100) != 0) || ((Year % 400) == 0));
	}
	return false;
}

FDateTime FDateTime::FromUnixTimestamp(int64 UnixTime)
{
	return FDateTime(1970, 1, 1) + FTimespan(UnixTime * ETimespan::TicksPerSecond);
}

FDateTime FDateTime::Now()
{
	int32 Year, Month, Day, DayOfWeek;
	int32 Hour, Minute, Second, Millisecond;
	FPlatformTime::SystemTime(Year, Month, DayOfWeek, Day, Hour, Minute, Second, Millisecond);
	return FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
}

FDateTime FDateTime::UtcNow()
{
	int32 Year, Month, Day, DayOfWeek;
	int32 Hour, Minute, Second, Millisecond;
	FPlatformTime::UtcTime(Year, Month, DayOfWeek, Day, Hour, Minute, Second, Millisecond);
	return FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
}

bool FDateTime::Parse(const FString& DateTimeString, FDateTime& OutDateTime)
{
	// First replace -, : and . with space.
	FString FixedString = DateTimeString.Replace("-", " ");
	FixedString = FixedString.Replace(":", " ");
	FixedString = FixedString.Replace(".", " ");

	// Split up on a single delimiter.
	TArray<FString> Tokens;
	FixedString.ParseIntoArray(Tokens, " ", true);

	// Make sure it parsed it properly (within reason).
	if ((Tokens.Num() < 6) || (Tokens.Num() > 7))
	{
		return false;
	}

	const int32 Year = FCString::Atoi(*Tokens[0]);
	const int32 Month = FCString::Atoi(*Tokens[1]);
	const int32 Day = FCString::Atoi(*Tokens[2]);
	const int32 Hour = FCString::Atoi(*Tokens[3]);
	const int32 Minute = FCString::Atoi(*Tokens[4]);
	const int32 Second = FCString::Atoi(*Tokens[5]);
	const int32 Millisecond = Tokens.Num() > 6 ? FCString::Atoi(*Tokens[6]) : 0;

	if (!Validate(Year, Month, Day, Hour, Minute, Second, Millisecond))
	{
		return false;
	}

	// Convert the tokens to numbers.
	OutDateTime = FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
	return true;
}

bool FDateTime::Validate(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second, int32 Millisecond)
{
	return (Year >= 1) && (Year <= 9999) && (Month >= 1) && (Month <= 12) && (Day >= 1) &&
		(Day <= DaysInMonth(Year, Month)) && (Hour >= 0) && (Hour <= 23) && (Minute >= 0) && (Minute <= 59) &&
		(Second >= 0) && (Second <= 59) && (Millisecond >= 0) && (Millisecond <= 999);
}
