#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParseTest, "System.Core.Misc.Parse",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FParseTest::RunTest(const FString& Parameters)
{
	const TCHAR* Stream = "-map=/Game/Maps/de_leon -nullrhi Speed=2.5 Count=7 Name=\"With Spaces\" bOn=True MAXX=9";

	FString Value;
	TestTrue("Value string", FParse::Value(Stream, "map=", Value));
	TestEqual("Value string content", Value, TEXT("/Game/Maps/de_leon"));
	TestTrue("Quoted value", FParse::Value(Stream, "Name=", Value));
	TestEqual("Quoted value content", Value, TEXT("With Spaces"));

	float Speed = 0.f;
	TestTrue("Value float", FParse::Value(Stream, "Speed=", Speed));
	TestEqual("Value float content", Speed, 2.5f);
	int32 Count = 0;
	TestTrue("Value int", FParse::Value(Stream, "Count=", Count));
	TestEqual("Value int content", Count, 7);
	bool bOn = false;
	TestTrue("Bool", FParse::Bool(Stream, "bOn=", bOn) && bOn);
	int32 Unused = 0;
	TestFalse("A match must start a word", FParse::Value(Stream, "X=", Unused));

	TestTrue("Param", FParse::Param(Stream, "nullrhi"));
	TestFalse("Param must be whole", FParse::Param(Stream, "null"));
	TestFalse("Param needs a dash", FParse::Param(Stream, "Speed"));

	const TCHAR* Tokens = "  first \"second token\" -third=\"a b\"";
	TestEqual("Token", FParse::Token(Tokens, false), TEXT("first"));
	TestEqual("Quoted token", FParse::Token(Tokens, false), TEXT("second token"));
	TestEqual("Token keeps inner quotes", FParse::Token(Tokens, false), TEXT("-third=\"a b\""));

	const TCHAR* Command = "stat fps";
	TestTrue("Command", FParse::Command(&Command, "stat") && FCString::Strcmp(Command, "fps") == 0);
	const TCHAR* Partial = "status";
	TestFalse("Command must be a whole word", FParse::Command(&Partial, "stat"));

	FString Quoted;
	int32 Read = 0;
	TestTrue("QuotedString", FParse::QuotedString("\"a\\tb\\\"c\\u00e9\" tail", Quoted, &Read));
	TestEqual("QuotedString escapes", Quoted, TEXT("a\tb\"c\xc3\xa9"));
	TestEqual("QuotedString length", Read, 15);

	const TCHAR* Lines = "one // comment\r\ntwo|three";
	FString Line;
	TestTrue("Line", FParse::Line(&Lines, Line));
	TestEqual("Line drops the comment", Line, TEXT("one "));
	FParse::Line(&Lines, Line);
	TestEqual("Line stops at '|'", Line, TEXT("two"));

	TestEqual("HexNumber", FParse::HexNumber("fF10"), 0xff10u);

	// Command line.
	TCHAR Arg0[] = "Game.exe";
	TCHAR Arg1[] = "-map=Test";
	TCHAR Arg2[] = "Name=Two Words";
	TCHAR Arg3[] = "Loose Arg";
	TCHAR* ArgV[] = {Arg0, Arg1, Arg2, Arg3};
	const FString Built = FCommandLine::BuildFromArgV(nullptr, 4, ArgV, nullptr);
	TestEqual("BuildFromArgV", Built, TEXT("-map=Test Name=\"Two Words\" \"Loose Arg\""));

	TArray<FString> CommandTokens;
	TArray<FString> Switches;
	FCommandLine::Parse(*Built, CommandTokens, Switches);
	TestEqual("Parse switches", Switches.Num(), 1);
	TestEqual("Parse switch name", Switches[0], TEXT("map=Test"));
	TestEqual("Parse tokens", CommandTokens.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGuidAndHashTest, "System.Core.Misc.GuidAndHash",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGuidAndHashTest::RunTest(const FString& Parameters)
{
	const FGuid Guid(0x01234567, 0x89ABCDEF, 0x02468ACE, 0x13579BDF);
	TestEqual("Digits", Guid.ToString(), TEXT("0123456789ABCDEF02468ACE13579BDF"));
	TestEqual("Hyphens", Guid.ToString(EGuidFormats::DigitsWithHyphens), TEXT("01234567-89AB-CDEF-0246-8ACE13579BDF"));

	const EGuidFormats Formats[] = {EGuidFormats::Digits, EGuidFormats::DigitsWithHyphens,
		EGuidFormats::DigitsWithHyphensInBraces, EGuidFormats::DigitsWithHyphensInParentheses,
		EGuidFormats::HexValuesInBraces, EGuidFormats::UniqueObjectGuid};
	for (const EGuidFormats Format : Formats)
	{
		FGuid Parsed;
		TestTrue("Parse", FGuid::Parse(Guid.ToString(Format), Parsed) && Parsed == Guid);
	}
	FGuid Bad;
	TestFalse("Parse rejects garbage", FGuid::Parse("0123456789ABCDEF02468ACE13579BDX", Bad));

	const FGuid NewA = FGuid::NewGuid();
	const FGuid NewB = FGuid::NewGuid();
	TestTrue("NewGuid is valid", NewA.IsValid());
	TestTrue("NewGuid is unique", NewA != NewB);
	TestTrue("Deterministic",
		FGuid::NewDeterministicGuid("/Game/Maps/de_leon") == FGuid::NewDeterministicGuid("/Game/Maps/de_leon"));
	TestTrue("Deterministic differs by seed",
		FGuid::NewDeterministicGuid("/Game/Maps/de_leon", 1) != FGuid::NewDeterministicGuid("/Game/Maps/de_leon", 2));

	// RFC 1321 test suite.
	TestEqual("MD5 empty", FMD5::HashAnsiString(""), TEXT("d41d8cd98f00b204e9800998ecf8427e"));
	TestEqual("MD5 abc", FMD5::HashAnsiString("abc"), TEXT("900150983cd24fb0d6963f7d28e17f72"));
	TestEqual("MD5 message digest", FMD5::HashAnsiString("message digest"), TEXT("f96b697d7cb7938d525a2f31aaf161d0"));
	TestEqual("MD5 long",
		FMD5::HashAnsiString("12345678901234567890123456789012345678901234567890123456789012345678901234567890"),
		TEXT("57edf4a22be3c955ac49da2e2107b67a"));

	FMD5 Pieces;
	Pieces.Update(reinterpret_cast<const uint8*>("mess"), 4);
	Pieces.Update(reinterpret_cast<const uint8*>("age digest"), 10);
	FMD5Hash Hash;
	Hash.Set(Pieces);
	TestEqual("MD5 in pieces", LexToString(Hash), TEXT("F96B697D7CB7938D525A2F31AAF161D0"));

	uint8 Bytes[4];
	TestEqual("HexToBytes", HexToBytes("0aFF10", Bytes), 3);
	TestEqual("BytesToHex", BytesToHex(Bytes, 3), TEXT("0AFF10"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDateTimeTest, "System.Core.Misc.DateTime",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDateTimeTest::RunTest(const FString& Parameters)
{
	const FDateTime Date(2026, 9, 25, 14, 3, 7, 42);
	int32 Year, Month, Day;
	Date.GetDate(Year, Month, Day);
	TestTrue("GetDate", Year == 2026 && Month == 9 && Day == 25);
	TestEqual("Hour", Date.GetHour(), 14);
	TestEqual("Hour12", Date.GetHour12(), 2);
	TestEqual("Millisecond", Date.GetMillisecond(), 42);
	TestTrue("Friday", Date.GetDayOfWeek() == EDayOfWeek::Friday);
	TestEqual("Day of year", Date.GetDayOfYear(), 268);
	TestEqual("ToString", Date.ToString(), TEXT("2026.09.25-14.03.07"));
	TestEqual("ToIso8601", Date.ToIso8601(), TEXT("2026-09-25T14:03:07.042Z"));
	TestEqual("Format", Date.ToString("%d/%m/%y %h%A"), TEXT("25/09/26 02PM"));

	TestEqual("Unix epoch", FDateTime(1970, 1, 1).ToUnixTimestamp(), int64(0));
	TestEqual("Unix timestamp", FDateTime(2000, 1, 1).ToUnixTimestamp(), int64(946684800));
	TestTrue("FromUnixTimestamp", FDateTime::FromUnixTimestamp(946684800) == FDateTime(2000, 1, 1));
	TestTrue("Leap years", FDateTime::IsLeapYear(2000) && !FDateTime::IsLeapYear(1900) && FDateTime::IsLeapYear(2024));
	TestEqual("February 2024", FDateTime::DaysInMonth(2024, 2), 29);

	FDateTime Parsed;
	TestTrue("Parse", FDateTime::Parse("2026.09.25-14.03.07", Parsed) && Parsed == FDateTime(2026, 9, 25, 14, 3, 7));
	TestFalse("Parse rejects a bad date", FDateTime::Parse("2026.02.30-00.00.00", Parsed));

	const FTimespan Span = FDateTime(2026, 1, 2) - FDateTime(2026, 1, 1, 12);
	TestEqual("Timespan hours", Span.GetHours(), 12);
	TestTrue("Add timespan", FDateTime(2026, 1, 1) + FTimespan(1, 0, 0, 0) == FDateTime(2026, 1, 2));
	TestTrue("Now is after 2020 (desktop) or the PS2 epoch", FDateTime::Now() >= FDateTime(2000, 1, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArchiveTest, "System.Core.Serialization.Archive",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArchiveTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Bytes;
	{
		FMemoryWriter Writer(Bytes);
		int32 Int = -42;
		uint64 Big = 0x0123456789ABCDEFull;
		float Float = 1.5f;
		bool bFlag = true;
		FString Text = "Hello";
		FString Empty;
		FName Name = "SomeName";
		TArray<int32> Ints = {1, 2, 3};
		TArray<uint8> Raw = {9, 8, 7};
		TMap<FString, int32> Map;
		Map.Add("A", 1);
		Map.Add("B", 2);
		FVector Vector(1, 2, 3);
		FTransform Transform(FRotator(0.f, 90.f, 0.f), FVector(10, 0, 0));
		FColor Color(1, 2, 3, 4);
		FGuid Guid(1, 2, 3, 4);
		uint32 Packed = 300;

		Writer << Int << Big << Float << bFlag << Text << Empty << Name << Ints << Raw << Map << Vector << Transform
			   << Color << Guid;
		Writer.SerializeIntPacked(Packed);
		TestFalse("Writer error", Writer.IsError());
	}

	// Little-endian, UE string layout: int32 length with the terminator, then the characters.
	TestEqual("First byte", int32(Bytes[0]), 0xD6);
	TestEqual("Bool is 32-bit", int32(Bytes[16]), 1);
	TestEqual("String length", int32(Bytes[20]), 6);

	FMemoryReader Reader(Bytes);
	int32 Int = 0;
	uint64 Big = 0;
	float Float = 0.f;
	bool bFlag = false;
	FString Text;
	FString Empty = "not empty";
	FName Name;
	TArray<int32> Ints;
	TArray<uint8> Raw;
	TMap<FString, int32> Map;
	FVector Vector;
	FTransform Transform;
	FColor Color;
	FGuid Guid;
	uint32 Packed = 0;
	Reader << Int << Big << Float << bFlag << Text << Empty << Name << Ints << Raw << Map << Vector << Transform
		   << Color << Guid;
	Reader.SerializeIntPacked(Packed);

	TestFalse("Reader error", Reader.IsError());
	TestTrue("At end", Reader.AtEnd());
	TestEqual("Int", Int, -42);
	TestTrue("Big", Big == 0x0123456789ABCDEFull);
	TestEqual("Float", Float, 1.5f);
	TestTrue("Bool", bFlag);
	TestEqual("String", Text, TEXT("Hello"));
	TestTrue("Empty string", Empty.IsEmpty());
	TestTrue("Name", Name == FName("SomeName"));
	TestTrue("Ints", Ints.Num() == 3 && Ints[2] == 3);
	TestTrue("Raw bytes", Raw.Num() == 3 && Raw[0] == 9);
	TestTrue("Map", Map.Num() == 2 && Map.FindRef("B") == 2);
	TestTrue("Vector", Vector == FVector(1, 2, 3));
	TestTrue("Transform", Transform.Equals(FTransform(FRotator(0.f, 90.f, 0.f), FVector(10, 0, 0))));
	TestTrue("Color", Color == FColor(1, 2, 3, 4));
	TestTrue("Guid", Guid == FGuid(1, 2, 3, 4));
	TestEqual("Packed", Packed, 300u);

	// Reading past the end is an error, and so is a corrupt string length.
	int32 Extra = 0;
	Reader << Extra;
	TestTrue("Past the end", Reader.IsError());

	TArray<uint8> Corrupt = {0x10, 0, 0, 0, 'a'};
	FMemoryReader CorruptReader(Corrupt);
	FString Bad;
	CorruptReader << Bad;
	TestTrue("Corrupt string length", CorruptReader.IsCriticalError());

	FBufferArchive Buffer;
	int32 Value = 7;
	Buffer << Value;
	TestEqual("FBufferArchive owns its bytes", Buffer.Num(), 4);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
