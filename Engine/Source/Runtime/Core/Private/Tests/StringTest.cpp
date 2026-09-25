#include "Containers/StringConv.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStringBasicsTest, "System.Core.String.Basics",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStringBasicsTest::RunTest(const FString& Parameters)
{
	FString Empty;
	TestTrue("Empty", Empty.IsEmpty() && Empty.Len() == 0);
	TestEqual("operator* of empty", *Empty, TEXT(""));
	TestTrue("FString() == FString(\"\")", Empty.Equals(FString("")));

	FString Hello("Hello");
	TestEqual("Len", Hello.Len(), 5);
	TestTrue("operator== ignores case", Hello == TEXT("hello"));
	TestFalse("Equals is case-sensitive", Hello.Equals("hello"));
	TestTrue("Equals IgnoreCase", Hello.Equals("hello", ESearchCase::IgnoreCase));
	TestTrue("operator< ignores case", FString("apple") < FString("Banana"));

	const FString Joined = Hello + ", " + FString("World");
	TestEqual("Concatenation", Joined, TEXT("Hello, World"));
	TestEqual("Find", Joined.Find("world"), 7);
	TestEqual("Find FromEnd", Joined.Find("o", ESearchCase::CaseSensitive, ESearchDir::FromEnd), 8);
	TestTrue("StartsWith", Joined.StartsWith("hello"));
	TestTrue("EndsWith", Joined.EndsWith("WORLD"));

	FString Left;
	FString Right;
	TestTrue("Split", Joined.Split(", ", &Left, &Right) && Left == "Hello" && Right == "World");
	TestEqual("Mid", Joined.Mid(7, 3), TEXT("Wor"));
	TestEqual("Left", Joined.Left(5), TEXT("Hello"));
	TestEqual("Right", Joined.Right(5), TEXT("World"));
	TestEqual("LeftChop", Joined.LeftChop(7), TEXT("Hello"));
	TestEqual("RightChop", Joined.RightChop(7), TEXT("World"));
	TestEqual("Replace", Joined.Replace("l", "L"), TEXT("HeLLo, WorLd"));
	TestEqual("ToUpper", Hello.ToUpper(), TEXT("HELLO"));
	TestEqual("TrimStartAndEnd", FString("  trim me  ").TrimStartAndEnd(), TEXT("trim me"));
	TestEqual("TrimQuotes", FString("\"quoted\"").TrimQuotes(), TEXT("quoted"));
	TestEqual("Reverse", Hello.Reverse(), TEXT("olleH"));

	FString Mutable("abc");
	Mutable.InsertAt(1, TEXT('X'));
	TestEqual("InsertAt", Mutable, TEXT("aXbc"));
	Mutable.RemoveAt(0, 2);
	TestEqual("RemoveAt", Mutable, TEXT("bc"));
	TestTrue("RemoveFromStart", Mutable.RemoveFromStart("B") && Mutable == "c");

	int32 Chars = 0;
	for (TCHAR Char : Hello)
	{
		(void)Char;
		++Chars;
	}
	TestEqual("Ranged for excludes the terminator", Chars, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStringFormattingTest, "System.Core.String.Formatting",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStringFormattingTest::RunTest(const FString& Parameters)
{
	TestEqual("Printf", FString::Printf("%d-%s-%.1f", 3, "x", 1.5), TEXT("3-x-1.5"));
	FString Long = FString::Printf("%0600d", 1);
	TestEqual("Printf beyond the stack buffer", Long.Len(), 600);
	TestEqual("SanitizeFloat integer", FString::SanitizeFloat(2.0), TEXT("2.0"));
	TestEqual("SanitizeFloat fraction", FString::SanitizeFloat(0.125), TEXT("0.125"));
	TestEqual("SanitizeFloat no fraction digits", FString::SanitizeFloat(3.0, 0), TEXT("3"));
	TestEqual("FromInt", FString::FromInt(-42), TEXT("-42"));
	TestEqual("FormatAsNumber", FString::FormatAsNumber(1234567), TEXT("1,234,567"));
	TestEqual("Path join", FString("Engine") / "Content" / "Maps", TEXT("Engine/Content/Maps"));
	TestEqual("Path join after a trailing separator", FString("Engine/") / "Content", TEXT("Engine/Content"));
	TestEqual("ChrN", FString::ChrN(3, TEXT('z')), TEXT("zzz"));
	TestEqual("LeftPad", FString("7").LeftPad(3), TEXT("  7"));

	FString Appended("n=");
	Appended.Appendf("%d", 5);
	TestEqual("Appendf", Appended, TEXT("n=5"));

	TArray<FString> Parts;
	TestEqual("ParseIntoArray culls empty parts", FString("a,b,,c").ParseIntoArray(Parts, ","), 3);
	TestEqual("Join", FString::Join(Parts, TEXT("-")), TEXT("a-b-c"));
	TestEqual("ParseIntoArray keeps empty parts", FString("a,,b").ParseIntoArray(Parts, ",", false), 3);
	TestEqual("ParseIntoArrayWS", FString(" one  two\tthree ").ParseIntoArrayWS(Parts), 3);
	TestEqual("ParseIntoArrayLines", FString("l1\r\nl2\nl3").ParseIntoArrayLines(Parts), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStringConversionTest, "System.Core.String.Conversion",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStringConversionTest::RunTest(const FString& Parameters)
{
	TestTrue("IsNumeric", FString("-12.5").IsNumeric() && !FString("12a").IsNumeric());
	TestTrue("ToBool", FString("True").ToBool() && FString("1").ToBool() && !FString("off").ToBool());
	TestEqual("Atoi", FCString::Atoi("123"), 123);
	TestEqual("Atof", FCString::Atof("0.5"), 0.5f);
	TestEqual("Strtoui64 hex", FCString::Strtoui64("ff", nullptr, 16), uint64(255));

	int32 Parsed = 0;
	TestTrue("LexTryParseString", LexTryParseString(Parsed, "77") && Parsed == 77);
	TestFalse("LexTryParseString rejects text", LexTryParseString(Parsed, "abc"));
	TestEqual("LexToString int", LexToString(12), TEXT("12"));
	TestEqual("LexToString bool", LexToString(true), TEXT("true"));

	// UTF-8 <-> wide round trip, including a code point outside the BMP.
	const TCHAR* Utf8 = "h\xC3\xA9llo \xF0\x9F\x98\x80";
	FTCHARToWide Wide(Utf8);
	TestEqual("Wide length", Wide.Length(), sizeof(WIDECHAR) == 2 ? 8 : 7);
	FString Back(Wide.Get());
	TestEqual("Round trip", Back, Utf8);
	FTCHARToWide Invalid("\xFF");
	TestEqual("Invalid UTF-8 becomes U+FFFD", uint32(Invalid.Get()[0]), 0xFFFDu);

	TestEqual("Stricmp", FCString::Stricmp("ABC", "abc"), 0);
	TestEqual("Strnicmp", FCString::Strnicmp("ABCdef", "abcXYZ", 3), 0);
	TCHAR Buffer[8];
	FCString::Strcpy(Buffer, "truncate me");
	TestEqual("Strcpy truncates", FCString::Strlen(Buffer), 7);
	TestNotNull("Stristr", FCString::Stristr("Hello World", "WORLD"));
	TestEqual("Spc", FCString::Strlen(FCString::Spc(4)), 4);

	TestEqual("MemCrc32 check value", FCrc::MemCrc32("123456789", 9), 0xCBF43926u);
	TestEqual("Strihash ignores case", FCrc::Strihash_DEPRECATED("Leon"), FCrc::Strihash_DEPRECATED("LEON"));
	TestEqual("GetTypeHash(FString) ignores case", GetTypeHash(FString("ABC")), GetTypeHash(FString("abc")));

	TestTrue(
		"FChar", FChar::IsAlpha('a') && FChar::IsDigit('7') && FChar::IsWhitespace('\t') && FChar::ToUpper('q') == 'Q');
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
