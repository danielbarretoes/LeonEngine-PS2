#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJsonReadTest, "System.Engine.FileSystem.Json.Read",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJsonReadTest::RunTest(const FString& Parameters)
{
	const FString Text = "{\n"
						 "  \"FileVersion\": 3,\n"
						 "  \"Name\": \"Caf\\u00e9 \\\"quoted\\\" \\ud83d\\ude00\",\n"
						 "  \"Ratio\": -1.5e2,\n"
						 "  \"Enabled\": true,\n"
						 "  \"Nothing\": null,\n"
						 "  \"Modules\": [ { \"Name\": \"Game\", \"Type\": \"Runtime\" }, { \"Name\": \"Tools\" } ],\n"
						 "  \"Platforms\": [ \"Win64\", \"PS2\" ],\n"
						 "  \"Empty\": {}\n"
						 "}";

	TSharedPtr<FJsonObject> Object;
	TestTrue(
		"Deserialize", FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) && Object.IsValid());
	if (!Object.IsValid())
	{
		return false;
	}

	TestEqual("Integer", Object->GetIntegerField("FileVersion"), 3);
	TestEqual("Escapes and UTF-8", Object->GetStringField("Name"), TEXT("Caf\xc3\xa9 \"quoted\" \xf0\x9f\x98\x80"));
	TestEqual("Exponent", float(Object->GetNumberField("Ratio")), -150.0f);
	TestTrue("Bool", Object->GetBoolField("Enabled"));
	TestTrue("Null", Object->HasField("Nothing") && Object->TryGetField("Nothing")->IsNull());
	TestTrue("Field names ignore case", Object->HasField("fileversion"));

	const TArray<TSharedPtr<FJsonValue>>& Modules = Object->GetArrayField("Modules");
	TestEqual("Array of objects", Modules.Num(), 2);
	TestEqual("Nested object", Modules[0]->AsObject()->GetStringField("Type"), TEXT("Runtime"));

	TArray<FString> Platforms;
	TestTrue("String array", Object->TryGetStringArrayField("Platforms", Platforms) && Platforms.Num() == 2);
	TestEqual("Field order is kept", Object->Values.Num(), 8);

	FString Missing;
	TestFalse("TryGet of a missing field", Object->TryGetStringField("Missing", Missing));
	int32 NotANumber = 0;
	TestFalse("TryGet with the wrong type", Object->TryGetNumberField("Modules", NotANumber));

	// Errors come with a position.
	TSharedPtr<FJsonObject> Bad;
	const TSharedRef<TJsonReader<>> BadReader = TJsonReaderFactory<>::Create("{\n  \"A\": 1,\n  \"B\" 2\n}");
	TestFalse("Syntax error", FJsonSerializer::Deserialize(BadReader, Bad));
	TestTrue("Error position", BadReader->GetErrorMessage().Contains("Line: 3"));
	TestFalse("Trailing comma", FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create("[1, 2,]"), Bad));
	TestFalse("Trailing garbage", FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create("{} x"), Bad));
	TestFalse("Array is not an object", FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create("[1]"), Bad));

	TArray<TSharedPtr<FJsonValue>> Array;
	TestTrue("Root array", FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(" [1, \"two\", [3]] "), Array));
	TestEqual("Root array size", Array.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJsonWriteTest, "System.Engine.FileSystem.Json.Write",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJsonWriteTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField("FileVersion", 3);
	Object->SetStringField("Description", "Tab\there \"quoted\"");
	Object->SetNumberField("Ratio", 0.1);
	Object->SetBoolField("Enabled", false);

	TArray<TSharedPtr<FJsonValue>> Numbers;
	Numbers.Add(MakeShared<FJsonValueNumber>(1));
	Numbers.Add(MakeShared<FJsonValueNumber>(2));
	Object->SetArrayField("Numbers", Numbers);

	TSharedPtr<FJsonObject> Module = MakeShared<FJsonObject>();
	Module->SetStringField("Name", "Game");
	TArray<TSharedPtr<FJsonValue>> Modules;
	Modules.Add(MakeShared<FJsonValueObject>(Module));
	Object->SetArrayField("Modules", Modules);

	FString Condensed;
	TestTrue("Serialize condensed",
		FJsonSerializer::Serialize(
			Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Condensed)));
	TestEqual("Condensed text", Condensed,
		TEXT("{\"FileVersion\":3,\"Description\":\"Tab\\there "
			 "\\\"quoted\\\"\",\"Ratio\":0.1,\"Enabled\":false,\"Numbers\":[1,2],"
			 "\"Modules\":[{\"Name\":\"Game\"}]}"));

	FString Pretty;
	TestTrue("Serialize pretty", FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Pretty)));
	TestTrue("Pretty: short values of an array on one line", Pretty.Contains("\"Numbers\": [ 1, 2 ]"));
	TestTrue("Pretty: field with a tab", Pretty.Contains("\t\"FileVersion\": 3,"));

	// Both forms read back to the same DOM.
	TSharedPtr<FJsonObject> Reread;
	TestTrue("Pretty reads back", FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Pretty), Reread));
	const FJsonValueObject Original(Object);
	TestTrue("Round trip", Reread.IsValid() && FJsonValueObject(Reread) == Original);

	// The writer API directly.
	FString Direct;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Direct);
	Writer->WriteObjectStart();
	Writer->WriteValue("Int", 7);
	Writer->WriteValue("Text", FString("x"));
	Writer->WriteArrayStart("List");
	Writer->WriteValue(FString("a"));
	Writer->WriteNull();
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	TestTrue("Close", Writer->Close());
	TestEqual("Writer output", Direct, TEXT("{\"Int\":7,\"Text\":\"x\",\"List\":[\"a\",null]}"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
