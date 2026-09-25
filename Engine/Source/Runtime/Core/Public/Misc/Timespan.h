#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Serialization/Archive.h"
#include "Templates/TypeHash.h"

/** Tick constants: one tick is 100 nanoseconds (UE: ETimespan). */
namespace ETimespan
{
	constexpr int64 TicksPerMicrosecond = 10;
	constexpr int64 TicksPerMillisecond = 10000;
	constexpr int64 TicksPerSecond = 10000000;
	constexpr int64 TicksPerMinute = 600000000;
	constexpr int64 TicksPerHour = 36000000000;
	constexpr int64 TicksPerDay = 864000000000;
	constexpr int64 TicksPerWeek = 6048000000000;
	constexpr int64 TicksPerYear = 365 * TicksPerDay;
	constexpr int64 MaxTicks = 9223372036854775807;
	constexpr int64 MinTicks = -9223372036854775807 - 1;
} // namespace ETimespan

/** A duration in 100 ns ticks (UE: FTimespan, the subset FDateTime needs). */
struct CORE_API FTimespan
{
	constexpr FTimespan()
		: Ticks(0)
	{
	}

	constexpr explicit FTimespan(int64 InTicks)
		: Ticks(InTicks)
	{
	}

	constexpr FTimespan(int32 Hours, int32 Minutes, int32 Seconds)
		: Ticks(Hours * ETimespan::TicksPerHour + Minutes * ETimespan::TicksPerMinute +
			  Seconds * ETimespan::TicksPerSecond)
	{
	}

	constexpr FTimespan(int32 Days, int32 Hours, int32 Minutes, int32 Seconds)
		: Ticks(Days * ETimespan::TicksPerDay + Hours * ETimespan::TicksPerHour + Minutes * ETimespan::TicksPerMinute +
			  Seconds * ETimespan::TicksPerSecond)
	{
	}

	FTimespan operator+(const FTimespan& Other) const
	{
		return FTimespan(Ticks + Other.Ticks);
	}
	FTimespan operator-(const FTimespan& Other) const
	{
		return FTimespan(Ticks - Other.Ticks);
	}
	FTimespan operator-() const
	{
		return FTimespan(-Ticks);
	}
	FTimespan& operator+=(const FTimespan& Other)
	{
		Ticks += Other.Ticks;
		return *this;
	}
	FTimespan& operator-=(const FTimespan& Other)
	{
		Ticks -= Other.Ticks;
		return *this;
	}

	bool operator==(const FTimespan& Other) const
	{
		return Ticks == Other.Ticks;
	}
	bool operator!=(const FTimespan& Other) const
	{
		return Ticks != Other.Ticks;
	}
	bool operator<(const FTimespan& Other) const
	{
		return Ticks < Other.Ticks;
	}
	bool operator<=(const FTimespan& Other) const
	{
		return Ticks <= Other.Ticks;
	}
	bool operator>(const FTimespan& Other) const
	{
		return Ticks > Other.Ticks;
	}
	bool operator>=(const FTimespan& Other) const
	{
		return Ticks >= Other.Ticks;
	}

	int32 GetDays() const
	{
		return int32(Ticks / ETimespan::TicksPerDay);
	}
	int32 GetHours() const
	{
		return int32((Ticks / ETimespan::TicksPerHour) % 24);
	}
	int32 GetMinutes() const
	{
		return int32((Ticks / ETimespan::TicksPerMinute) % 60);
	}
	int32 GetSeconds() const
	{
		return int32((Ticks / ETimespan::TicksPerSecond) % 60);
	}
	int32 GetFractionMilli() const
	{
		return int32((Ticks % ETimespan::TicksPerSecond) / ETimespan::TicksPerMillisecond);
	}
	int64 GetTicks() const
	{
		return Ticks;
	}

	/** Whole milliseconds (Leon: integer so EE code stays off doubles). */
	int64 GetTotalMillisecondsInt() const
	{
		return Ticks / ETimespan::TicksPerMillisecond;
	}

	/** Total seconds as a double (UE: GetTotalSeconds). */
	double GetTotalSeconds() const
	{
		return double(Ticks) / double(ETimespan::TicksPerSecond);
	}

	bool IsZero() const
	{
		return Ticks == 0;
	}

	static FTimespan FromMilliseconds(int64 Milliseconds)
	{
		return FTimespan(Milliseconds * ETimespan::TicksPerMillisecond);
	}
	static FTimespan FromSeconds(int64 Seconds)
	{
		return FTimespan(Seconds * ETimespan::TicksPerSecond);
	}
	static FTimespan FromMinutes(int64 Minutes)
	{
		return FTimespan(Minutes * ETimespan::TicksPerMinute);
	}
	static FTimespan FromHours(int64 Hours)
	{
		return FTimespan(Hours * ETimespan::TicksPerHour);
	}
	static FTimespan FromDays(int64 Days)
	{
		return FTimespan(Days * ETimespan::TicksPerDay);
	}

	static constexpr FTimespan Zero()
	{
		return FTimespan(0);
	}

	friend FArchive& operator<<(FArchive& Ar, FTimespan& Timespan)
	{
		return Ar << Timespan.Ticks;
	}

	friend uint32 GetTypeHash(const FTimespan& Timespan)
	{
		return GetTypeHash(Timespan.Ticks);
	}

private:
	int64 Ticks;
};
