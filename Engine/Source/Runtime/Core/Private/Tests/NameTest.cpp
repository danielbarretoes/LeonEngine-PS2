#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNameTest, "System.Core.Name.FName",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FNameTest::RunTest(const FString& Parameters)
{
	static_assert(sizeof(FName) == 8, "FName is 8 bytes");

	const FName None;
	TestTrue("Default is None", None.IsNone() && None == NAME_None);
	TestEqual("None string", None.ToString(), TEXT("None"));

	const FName Actor("Actor_12");
	TestEqual("Number suffix", Actor.GetNumber(), NAME_EXTERNAL_TO_INTERNAL(12));
	TestEqual("ToString", Actor.ToString(), TEXT("Actor_12"));
	TestEqual("Plain name", Actor.GetPlainNameString(), TEXT("Actor"));
	TestTrue("Case-insensitive", FName("actor_12") == Actor);
	TestTrue("Different number", FName("Actor_13") != Actor);
	TestEqual("Leading zero is not a number", FName("Name_01").GetNumber(), NAME_NO_NUMBER_INTERNAL);
	TestEqual("_0 is a number", FName("Name_0").GetNumber(), NAME_EXTERNAL_TO_INTERNAL(0));
	TestEqual("Explicit number", FName("Base", NAME_EXTERNAL_TO_INTERNAL(3)).ToString(), TEXT("Base_3"));

	TestTrue("FNAME_Find of a missing name", FName("SurelyMissingName_x7", FNAME_Find).IsNone());
	const FName Added("FoundLater");
	TestTrue("FNAME_Find of an existing name", FName("foundlater", FNAME_Find) == Added);

	TestEqual("Hard-coded", FName(NAME_Vector).ToString(), TEXT("Vector"));
	TestTrue("Hard-coded compare", FName("vector") == NAME_Vector);
	TestTrue("Compare with a string", Actor == TEXT("ACTOR_12"));

	TestTrue("Lexical order", FName("alpha") < FName("Zeta"));
	TestTrue("Number order", FName("Item_2").Compare(FName("Item_10")) < 0);
	TestTrue("IsValid", Actor.IsValid());
	TestEqual("Hash follows equality", GetTypeHash(FName("Hash_1")), GetTypeHash(FName("HASH_1")));

	TestTrue("Pool has the hard-coded names", FName::GetNumNames() >= int32(NAME_MaxHardcodedNameIndex));
	TestTrue("Pool memory accounted", FName::GetNameEntryMemorySize() > 0);

	TMap<FName, int32> ByName;
	ByName.Add(FName("Key"), 1);
	TestTrue("TMap<FName>", ByName.Contains(FName("KEY")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextTest, "System.Core.Text.Format",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTextTest::RunTest(const FString& Parameters)
{
	const FText Pattern = NSLOCTEXT("Test", "Key", "Hello {0}, {1} points");
	TestEqual("Format", FText::Format(Pattern, FText::FromString("Leon"), FText::AsNumber(42)).ToString(),
		TEXT("Hello Leon, 42 points"));
	TestEqual("Escaped brace", FText::Format(INVTEXT("`{0}"), FText::AsNumber(1)).ToString(), TEXT("{0}"));
	TestEqual("Missing argument keeps the placeholder", FText::Format(INVTEXT("{3}"), FText::AsNumber(1)).ToString(),
		TEXT("{3}"));
	TestEqual("AsNumber float", FText::AsNumber(1.5).ToString(), TEXT("1.5"));
	TestEqual("AsNumber whole float", FText::AsNumber(2.0f).ToString(), TEXT("2"));
	TestEqual("AsPercent", FText::AsPercent(0.42f).ToString(), TEXT("42%"));
	TestTrue("IsEmpty", FText::GetEmpty().IsEmpty());
	TestTrue("IsEmptyOrWhitespace", FText::FromString("  ").IsEmptyOrWhitespace());
	TestTrue("EqualTo", FText::FromString("a").EqualTo(FText::FromString("a")));
	TestFalse("EqualTo is case-sensitive", FText::FromString("a").EqualTo(FText::FromString("A")));
	TestEqual(
		"Join", FText::Join(INVTEXT(", "), {FText::FromString("a"), FText::FromString("b")}).ToString(), TEXT("a, b"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
