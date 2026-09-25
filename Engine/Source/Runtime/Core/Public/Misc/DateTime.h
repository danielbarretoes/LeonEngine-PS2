#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "Serialization/Archive.h"
#include "Templates/TypeHash.h"

/** Day of the week (UE: EDayOfWeek). */
enum class EDayOfWeek
{
	Monday = 0,
	Tuesday,
	Wednesday,
	Thursday,
	Friday,
	Saturday,
	Sunday
};

/** Month of the year (UE: EMonthOfYear). */
enum class EMonthOfYear
{
	January = 1,
	February,
	March,
	April,
	May,
	June,
	July,
	August,
	September,
	October,
	November,
	December
};

/**
 * A date and time: 100 ns ticks since 0001-01-01 00:00:00 in the Gregorian calendar (UE: FDateTime). Integer only:
 * the Julian-day helpers of UE, which use doubles, are left out.
 */
struct CORE_API FDateTime
{
	constexpr FDateTime()
		: Ticks(0)
	{
	}

	constexpr explicit FDateTime(int64 InTicks)
		: Ticks(InTicks)
	{
	}

	FDateTime(
		int32 Year, int32 Month, int32 Day, int32 Hour = 0, int32 Minute = 0, int32 Second = 0, int32 Millisecond = 0);

	FDateTime operator+(const FTimespan& Other) const
	{
		return FDateTime(Ticks + Other.GetTicks());
	}
	FDateTime& operator+=(const FTimespan& Other)
	{
		Ticks += Other.GetTicks();
		return *this;
	}
	FTimespan operator-(const FDateTime& Other) const
	{
		return FTimespan(Ticks - Other.Ticks);
	}
	FDateTime operator-(const FTimespan& Other) const
	{
		return FDateTime(Ticks - Other.GetTicks());
	}

	bool operator==(const FDateTime& Other) const
	{
		return Ticks == Other.Ticks;
	}
	bool operator!=(const FDateTime& Other) const
	{
		return Ticks != Other.Ticks;
	}
	bool operator<(const FDateTime& Other) const
	{
		return Ticks < Other.Ticks;
	}
	bool operator<=(const FDateTime& Other) const
	{
		return Ticks <= Other.Ticks;
	}
	bool operator>(const FDateTime& Other) const
	{
		return Ticks > Other.Ticks;
	}
	bool operator>=(const FDateTime& Other) const
	{
		return Ticks >= Other.Ticks;
	}

	/** Midnight of the same day. */
	FDateTime GetDate() const
	{
		return FDateTime(Ticks - (Ticks % ETimespan::TicksPerDay));
	}

	void GetDate(int32& OutYear, int32& OutMonth, int32& OutDay) const;
	int32 GetDay() const;
	EDayOfWeek GetDayOfWeek() const;
	int32 GetDayOfYear() const;
	int32 GetHour() const
	{
		return int32((Ticks / ETimespan::TicksPerHour) % 24);
	}
	int32 GetHour12() const;
	int32 GetMillisecond() const
	{
		return int32((Ticks / ETimespan::TicksPerMillisecond) % 1000);
	}
	int32 GetMinute() const
	{
		return int32((Ticks / ETimespan::TicksPerMinute) % 60);
	}
	int32 GetMonth() const;
	EMonthOfYear GetMonthOfYear() const
	{
		return EMonthOfYear(GetMonth());
	}
	int32 GetSecond() const
	{
		return int32((Ticks / ETimespan::TicksPerSecond) % 60);
	}
	int64 GetTicks() const
	{
		return Ticks;
	}
	FTimespan GetTimeOfDay() const
	{
		return FTimespan(Ticks % ETimespan::TicksPerDay);
	}
	int32 GetYear() const;
	bool IsAfternoon() const
	{
		return GetHour() >= 12;
	}
	bool IsMorning() const
	{
		return GetHour() < 12;
	}

	/** "%Y.%m.%d-%H.%M.%S", UE's default form (used in log backups). */
	FString ToString() const;

	/**
	 * Formats with %a (am/pm), %A (AM/PM), %d, %D (day of year), %m, %y, %Y, %h (12 h), %H, %M, %S, %s (ms)
	 * (UE: ToString(Format)).
	 */
	FString ToString(const TCHAR* Format) const;

	/** "YYYY-MM-DDTHH:MM:SS.sssZ" (UE: ToIso8601). */
	FString ToIso8601() const;

	/** Seconds since 1970-01-01 (UE: ToUnixTimestamp). */
	int64 ToUnixTimestamp() const;

	static int32 DaysInMonth(int32 Year, int32 Month);
	static int32 DaysInYear(int32 Year);
	static bool IsLeapYear(int32 Year);

	static FDateTime FromUnixTimestamp(int64 UnixTime);

	static FDateTime MaxValue()
	{
		return FDateTime(3652059 * ETimespan::TicksPerDay - 1);
	}
	static FDateTime MinValue()
	{
		return FDateTime(0);
	}

	/** Local time (UE: Now). */
	static FDateTime Now();

	/** Local midnight today (UE: Today). */
	static FDateTime Today()
	{
		return Now().GetDate();
	}

	static FDateTime UtcNow();

	/** Parses "YYYY.MM.DD-HH.MM.SS[.MS]" (also with '-' / ':' separators) (UE: Parse). */
	static bool Parse(const FString& DateTimeString, FDateTime& OutDateTime);

	static bool Validate(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second, int32 Millisecond);

	friend FArchive& operator<<(FArchive& Ar, FDateTime& DateTime)
	{
		return Ar << DateTime.Ticks;
	}

	friend uint32 GetTypeHash(const FDateTime& DateTime)
	{
		return GetTypeHash(DateTime.Ticks);
	}

private:
	int64 Ticks;
};
