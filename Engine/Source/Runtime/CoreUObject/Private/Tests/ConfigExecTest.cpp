#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreMisc.h"
#include "Misc/Paths.h"
#include "Tests/ConfigExecTestTypes.h"
#include "Tests/GarbageCollectionTestTypes.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** The GConfig key of the tests' in-memory config file. */
	const TCHAR* const ConfigTestIni = TEXT("LeonCoreUObjectConfigTest.ini");

	/** The lower layer of a hierarchy (a Base / Default file) holding every kind of config value. */
	const TCHAR* const ConfigLowerLayer = TEXT("[/Script/CoreUObject.ConfigTestObject]\n"
											   "IntValue=42\n"
											   "FloatValue=2.5\n"
											   "bFlag=True\n"
											   "StringValue=Hello World\n"
											   "NameValue=Leon\n"
											   "TextValue=Some text\n"
											   "Mode=High\n"
											   "Location=(X=1.0,Y=2.0,Z=3.0)\n"
											   "Entry=(Name=\"First\",Count=3)\n"
											   "+Items=Alpha\n"
											   "+Items=Beta\n"
											   "+Items=Gamma\n"
											   "+Entries=(Name=\"A\",Count=1)\n"
											   "+Entries=(Name=\"B\",Count=2)\n"
											   "Indexed[0]=7\n"
											   "Indexed[1]=9\n"
											   "Fixed[1]=21\n"
											   "Tags=(Red,Blue)\n"
											   "Scores=((\"One\",1),(\"Two\",2))\n"
											   "ObjectRef=/Engine/Transient\n"
											   "ClassRef=Class'/Script/CoreUObject.GCTestObject'\n"
											   "SoftPath=/Game/Maps/Arena.Arena\n"
											   "SoftClass=/Script/CoreUObject.GCTestObject\n"
											   "SoftObject=/Game/Maps/Arena.Arena:PersistentLevel\n"
											   "GlobalValue=11\n"
											   "NotConfig=99\n"
											   "[/Script/CoreUObject.ConfigTestChild]\n"
											   "IntValue=100\n"
											   "ChildValue=5\n"
											   "GlobalValue=77\n");

	/** An upper layer (a project or platform file) editing the lower one with the + - . ! operators. */
	const TCHAR* const ConfigUpperLayer = TEXT("[/Script/CoreUObject.ConfigTestObject]\n"
											   "FloatValue=3.5\n"
											   "-Items=Beta\n"
											   "+Items=Delta\n"
											   "+Items=Alpha\n"
											   ".Items=Alpha\n"
											   "!Entries=ClearArray\n"
											   "+Entries=(Name=\"C\",Count=3)\n");

	/** The layered test file in GConfig; removed by RemoveConfigTestFile. */
	FConfigFile& AddConfigTestFile()
	{
		FConfigFile& File = GConfig->Add(ConfigTestIni, FConfigFile());
		File.CombineFromBuffer(ConfigLowerLayer);
		File.CombineFromBuffer(ConfigUpperLayer);
		return File;
	}

	void RemoveConfigTestFile()
	{
		GConfig->Remove(ConfigTestIni);
	}

	/** Copies the config members (the PostConstructLink chain) of one object of Class to another. */
	void CopyConfigMembers(UClass* Class, UObject* Dest, UObject* Src)
	{
		for (FProperty* Property = Class->PostConstructLink; Property; Property = Property->PostConstructLinkNext)
		{
			Property->CopyCompleteValue_InContainer(Dest, Src);
		}
	}

	/** Collects what CallFunctionByNameWithArguments writes to its output device. */
	class FExecTestOutput final : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			(void)Category;
			Lines.Add(V);
			if ((Verbosity & ELogVerbosity::VerbosityMask) == ELogVerbosity::Warning)
			{
				++NumWarnings;
			}
		}

		TArray<FString> Lines;
		int32 NumWarnings = 0;
	};

	bool StaticExecHandler(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
	{
		(void)InWorld;
		if (FCString::Stricmp(Cmd, TEXT("LeonTestCommand")) == 0)
		{
			Ar.Log(TEXT("handled"));
			return true;
		}
		return false;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigLoadTest, "System.CoreUObject.Config.LoadConfig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FConfigLoadTest::RunTest(const FString& Parameters)
{
	if (!GConfig)
	{
		AddInfo(TEXT("No config system in this program"));
		return true;
	}
	AddConfigTestFile();
	UConfigTestObject* Object = NewObject<UConfigTestObject>();
	Object->LoadConfig(nullptr, ConfigTestIni);

	TestEqual(
		TEXT("Section"), UConfigTestObject::StaticClass()->GetPathName(), TEXT("/Script/CoreUObject.ConfigTestObject"));
	TestEqual(TEXT("int32"), Object->IntValue, 42);
	TestEqual(TEXT("float (upper layer)"), Object->FloatValue, 3.5f);
	TestTrue(TEXT("bool"), Object->bFlag);
	TestEqual(TEXT("FString"), Object->StringValue, TEXT("Hello World"));
	TestEqual(TEXT("FName"), Object->NameValue, FName(TEXT("Leon")));
	TestEqual(TEXT("FText"), Object->TextValue.ToString(), TEXT("Some text"));
	TestTrue(TEXT("enum"), Object->Mode == EConfigTestMode::High);
	TestTrue(TEXT("Core struct"), Object->Location == FVector(1.0f, 2.0f, 3.0f));
	TestTrue(TEXT("USTRUCT"), Object->Entry.Name == TEXT("First") && Object->Entry.Count == 3);
	TestEqual(TEXT("C array element"), Object->Fixed[1], 21);
	TestTrue(
		TEXT("C array elements not in the config keep their value"), Object->Fixed[0] == 10 && Object->Fixed[2] == 30);
	TestTrue(TEXT("TSet"),
		Object->Tags.Num() == 2 && Object->Tags.Contains(TEXT("Red")) && Object->Tags.Contains(TEXT("Blue")));
	TestTrue(TEXT("TMap"), Object->Scores.Num() == 2 && Object->Scores.FindRef(TEXT("Two")) == 2);
	TestTrue(TEXT("Object by path"), Object->ObjectRef == GetTransientPackage());
	TestTrue(TEXT("Class reference"), Object->ClassRef.Get() == UGCTestObject::StaticClass());
	TestEqual(TEXT("FSoftObjectPath"), Object->SoftPath.ToString(), TEXT("/Game/Maps/Arena.Arena"));
	TestTrue(TEXT("FSoftClassPath"), Object->SoftClass.ResolveClass() == UGCTestObject::StaticClass());
	TestEqual(TEXT("TSoftObjectPtr"), Object->SoftObject.ToString(), TEXT("/Game/Maps/Arena.Arena:PersistentLevel"));
	TestEqual(TEXT("GlobalConfig"), Object->GlobalValue, 11);
	TestEqual(TEXT("Not a config member"), Object->NotConfig, 5);

	// Arrays after the layers' + - . ! edits.
	TestEqual(TEXT("Array: + adds, - removes, + skips a duplicate, . adds one"), Object->Items.Num(), 4);
	TestTrue(TEXT("Array values"),
		Object->Items.Contains(TEXT("Gamma")) && Object->Items.Contains(TEXT("Delta")) &&
			!Object->Items.Contains(TEXT("Beta")) &&
			Object->Items.FilterByPredicate([](const FString& Item) { return Item == TEXT("Alpha"); }).Num() == 2);
	TestTrue(TEXT("Array: ! clears the lower layers"),
		Object->Entries.Num() == 1 && Object->Entries[0].Name == TEXT("C") && Object->Entries[0].Count == 3);
	TestTrue(TEXT("Array from Key[N]= entries"),
		Object->Indexed.Num() == 2 && Object->Indexed[0] == 7 && Object->Indexed[1] == 9);

	// A subclass reads its parents' sections first, then its own; a GlobalConfig member only its declaring class's.
	UConfigTestChild* Child = NewObject<UConfigTestChild>();
	Child->LoadConfig(nullptr, ConfigTestIni, UE4::LCPF_ReadParentSections);
	TestEqual(TEXT("Child section overrides"), Child->IntValue, 100);
	TestEqual(TEXT("Parent section fills the rest"), Child->StringValue, TEXT("Hello World"));
	TestEqual(TEXT("Child member"), Child->ChildValue, 5);
	TestEqual(TEXT("GlobalConfig ignores the child section"), Child->GlobalValue, 11);

	// Only the requested property.
	UConfigTestObject* Single = NewObject<UConfigTestObject>();
	Single->LoadConfig(
		nullptr, ConfigTestIni, UE4::LCPF_None, UConfigTestObject::StaticClass()->FindPropertyByName(TEXT("IntValue")));
	TestTrue(TEXT("PropertyToLoad"), Single->IntValue == 42 && Single->FloatValue == 1.0f);
	RemoveConfigTestFile();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigDefaultsTest, "System.CoreUObject.Config.ClassDefaults",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FConfigDefaultsTest::RunTest(const FString& Parameters)
{
	FConfigFile* GameIni = GConfig && !GGameIni.IsEmpty() ? GConfig->FindConfigFile(GGameIni) : nullptr;
	if (!GameIni)
	{
		AddInfo(TEXT("No Game config in this program"));
		return true;
	}
	UClass* Class = UConfigTestObject::StaticClass();
	UClass* ChildClass = UConfigTestChild::StaticClass();
	TestEqual(TEXT("Config=Game"), Class->ClassConfigName, FName(TEXT("Game")));
	TestTrue(TEXT("Config file of the class"), GetConfigFilename(GetMutableDefault<UConfigTestObject>()) == GGameIni);
	TestTrue(TEXT("Config is inherited"), ChildClass->HasAnyClassFlags(CLASS_Config));
	TestTrue(TEXT("Default config file"),
		GetDefault<UConfigTestObject>()->GetDefaultConfigFilename().EndsWith(TEXT("Config/DefaultGame.ini")));

	// Snapshots of the class defaults (instances copy the config members), restored at the end. Templates, so that
	// ReloadConfig does not reach them.
	UConfigTestObject* Snapshot = NewObject<UConfigTestObject>(GetTransientPackage(), NAME_None, RF_ArchetypeObject);
	UConfigTestChild* ChildSnapshot = NewObject<UConfigTestChild>(GetTransientPackage(), NAME_None, RF_ArchetypeObject);
	UConfigTestObject* ExistingInstance = NewObject<UConfigTestObject>();

	GameIni->CombineFromBuffer(TEXT("[/Script/CoreUObject.ConfigTestObject]\n"
									"IntValue=64\n"
									"StringValue=From the game config\n"
									"+Items=One\n"
									"[/Script/CoreUObject.ConfigTestChild]\n"
									"IntValue=65\n"
									"ChildValue=8\n"));
	// What the class default object does when it is created: its parents' sections, then its own.
	UConfigTestObject* CDO = GetMutableDefault<UConfigTestObject>();
	CDO->ReloadConfig();
	TestTrue(TEXT("Class defaults loaded"), CDO->IntValue == 64 && CDO->StringValue == TEXT("From the game config"));
	TestTrue(TEXT("PostReloadConfig"), CDO->ReloadCount > 0);
	TestEqual(TEXT("ReloadConfig reaches existing instances"), ExistingInstance->IntValue, 64);

	UConfigTestObject* Instance = NewObject<UConfigTestObject>();
	TestTrue(TEXT("A new instance copies the config members"),
		Instance->IntValue == 64 && Instance->Items.Num() == 1 && Instance->Items[0] == TEXT("One"));
	TestEqual(TEXT("Other members come from the constructor"), Instance->NotConfig, 5);

	UConfigTestChild* ChildCDO = GetMutableDefault<UConfigTestChild>();
	ChildCDO->ReloadConfig();
	UConfigTestChild* Child = NewObject<UConfigTestChild>();
	TestTrue(TEXT("Child class defaults: own section over the parent's"),
		Child->IntValue == 65 && Child->ChildValue == 8 && Child->StringValue == TEXT("From the game config"));

	GameIni->Remove(TEXT("/Script/CoreUObject.ConfigTestObject"));
	GameIni->Remove(TEXT("/Script/CoreUObject.ConfigTestChild"));
	CopyConfigMembers(Class, CDO, Snapshot);
	CopyConfigMembers(ChildClass, ChildCDO, ChildSnapshot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigSubclassConstructorTest,
	"System.CoreUObject.Config.SubclassConstructorDefaults",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FConfigSubclassConstructorTest::RunTest(const FString& Parameters)
{
	// A subclass constructor's values for inherited config members are its class defaults (UE): the parent's class
	// defaults are not copied over them, and a new instance copies them. No config section names these classes.
	const UConfigTestConstructedChild* ChildCDO = GetDefault<UConfigTestConstructedChild>();
	TestEqual(TEXT("The subclass constructor's int"), ChildCDO->IntValue, 7);
	TestEqual(TEXT("The subclass constructor's string"), ChildCDO->StringValue, FString(TEXT("Constructed")));
	TestEqual(TEXT("The parent's own"), GetDefault<UConfigTestObject>()->StringValue, FString(TEXT("Default")));
	const UConfigTestConstructedChild* Child = NewObject<UConfigTestConstructedChild>();
	TestTrue(TEXT("An instance"), Child->IntValue == 7 && Child->StringValue == TEXT("Constructed"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigPerObjectTest, "System.CoreUObject.Config.PerObjectConfig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FConfigPerObjectTest::RunTest(const FString& Parameters)
{
	FConfigFile* GameIni = GConfig && !GGameIni.IsEmpty() ? GConfig->FindConfigFile(GGameIni) : nullptr;
	if (!GameIni)
	{
		AddInfo(TEXT("No Game config in this program"));
		return true;
	}
	TestTrue(TEXT("PerObjectConfig flag"),
		UConfigTestPerObject::StaticClass()->HasAllClassFlags(CLASS_Config | CLASS_PerObjectConfig));
	GameIni->CombineFromBuffer(TEXT("[LeonPerObjectA ConfigTestPerObject]\n"
									"Label=First\n"
									"Level=3\n"
									"[LeonPerObjectB ConfigTestPerObject]\n"
									"Level=4\n"));
	// Each object reads its own section when it is created.
	UConfigTestPerObject* A = NewObject<UConfigTestPerObject>(GetTransientPackage(), TEXT("LeonPerObjectA"));
	UConfigTestPerObject* B = NewObject<UConfigTestPerObject>(GetTransientPackage(), TEXT("LeonPerObjectB"));
	TestTrue(TEXT("First object"), A->Label == TEXT("First") && A->Level == 3);
	TestTrue(TEXT("Second object"), B->Label.IsEmpty() && B->Level == 4);
	GameIni->Remove(TEXT("LeonPerObjectA ConfigTestPerObject"));
	GameIni->Remove(TEXT("LeonPerObjectB ConfigTestPerObject"));
	return true;
}

	#if PLATFORM_DESKTOP

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigSaveTest, "System.CoreUObject.Config.SaveConfig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FConfigSaveTest::RunTest(const FString& Parameters)
{
	if (!GConfig)
	{
		AddInfo(TEXT("No config system in this program"));
		return true;
	}
	// A user layer in the intermediate folder instead of <Project>/Saved/Config: SaveConfig writes the file it is
	// given (the default is the class's, GGameIni).
	const FString Dir = FPaths::ProjectIntermediateDir() + TEXT("Tests/CoreUObjectConfig/");
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	const FString UserIni = Dir + TEXT("Game.ini");
	GConfig->Add(UserIni, FConfigFile());

	UConfigTestObject* Object = NewObject<UConfigTestObject>();
	Object->IntValue = 123;
	Object->StringValue = TEXT("Saved Value");
	Object->Mode = EConfigTestMode::Low;
	Object->Location = FVector(4.0f, 5.0f, 6.0f);
	Object->Entry.Name = TEXT("Saved");
	Object->Entry.Count = 9;
	Object->Items = {TEXT("X"), TEXT("Y")};
	Object->Fixed[2] = 33;
	Object->SoftPath = FSoftObjectPath(TEXT("/Game/Maps/Saved.Saved"));
	Object->Tags = {TEXT("Green")};
	Object->NotConfig = 77;
	Object->SaveConfig(CPF_Config, *UserIni, GConfig, /*bAllowCopyToDefaultObject =*/false);

	FConfigFile Saved;
	Saved.Read(UserIni);
	const TCHAR* Section = TEXT("/Script/CoreUObject.ConfigTestObject");
	FString Value;
	TestTrue(TEXT("int32 saved"), Saved.GetString(Section, TEXT("IntValue"), Value) && Value == TEXT("123"));
	TestTrue(TEXT("FString saved as its text"),
		Saved.GetString(Section, TEXT("StringValue"), Value) && Value == TEXT("Saved Value"));
	TestTrue(TEXT("C array element saved"), Saved.GetString(Section, TEXT("Fixed[2]"), Value) && Value == TEXT("33"));
	TestTrue(TEXT("Soft path saved"),
		Saved.GetString(Section, TEXT("SoftPath"), Value) && Value == TEXT("/Game/Maps/Saved.Saved"));
	TArray<FString> Items;
	TestEqual(TEXT("Array saved as values"), Saved.GetArray(Section, TEXT("Items"), Items), 2);
	TestFalse(TEXT("Non-config member not saved"), Saved.GetString(Section, TEXT("NotConfig"), Value));
	TestTrue(TEXT("Class defaults untouched without the copy"), GetDefault<UConfigTestObject>()->IntValue != 123);

	// Round trip: a fresh load of the saved file gives the values back.
	GConfig->Flush(true, UserIni);
	GConfig->Remove(UserIni);
	GConfig->Find(UserIni, true);
	UConfigTestObject* Loaded = NewObject<UConfigTestObject>();
	Loaded->LoadConfig(nullptr, *UserIni);
	TestEqual(TEXT("Round trip: int32"), Loaded->IntValue, 123);
	TestEqual(TEXT("Round trip: FString"), Loaded->StringValue, TEXT("Saved Value"));
	TestTrue(TEXT("Round trip: enum"), Loaded->Mode == EConfigTestMode::Low);
	TestTrue(TEXT("Round trip: Core struct"), Loaded->Location == FVector(4.0f, 5.0f, 6.0f));
	TestTrue(TEXT("Round trip: USTRUCT"), Loaded->Entry.Name == TEXT("Saved") && Loaded->Entry.Count == 9);
	TestTrue(TEXT("Round trip: array"),
		Loaded->Items.Num() == 2 && Loaded->Items[0] == TEXT("X") && Loaded->Items[1] == TEXT("Y"));
	TestEqual(TEXT("Round trip: C array"), Loaded->Fixed[2], 33);
	TestEqual(TEXT("Round trip: soft path"), Loaded->SoftPath.ToString(), TEXT("/Game/Maps/Saved.Saved"));
	TestTrue(TEXT("Round trip: set"), Loaded->Tags.Num() == 1 && Loaded->Tags.Contains(TEXT("Green")));
	TestEqual(TEXT("Round trip: not config"), Loaded->NotConfig, 5);

	GConfig->Remove(UserIni);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}

	#endif // PLATFORM_DESKTOP

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExecCallFunctionTest, "System.CoreUObject.Exec.CallFunctionByNameWithArguments",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FExecCallFunctionTest::RunTest(const FString& Parameters)
{
	UExecTestObject* Object = NewObject<UExecTestObject>();
	FExecTestOutput Ar;
	TestTrue(TEXT("FUNC_Exec"),
		Object->FindFunction(TEXT("SetValues"))->HasAnyFunctionFlags(FUNC_Exec) &&
			!Object->FindFunction(TEXT("NotExec"))->HasAnyFunctionFlags(FUNC_Exec));

	TestTrue(TEXT("int / float / FName"),
		Object->CallFunctionByNameWithArguments(TEXT("SetValues 42 2.5 Leon"), Ar, nullptr));
	TestTrue(TEXT("Values"), Object->IntValue == 42 && Object->FloatValue == 2.5f && Object->NameValue == TEXT("Leon"));
	TestTrue(
		TEXT("Case-insensitive name"), Object->CallFunctionByNameWithArguments(TEXT("setvalues 1 1 A"), Ar, nullptr));
	TestEqual(TEXT("Case-insensitive call"), Object->IntValue, 1);

	TestTrue(TEXT("enum"), Object->CallFunctionByNameWithArguments(TEXT("SetMode High"), Ar, nullptr));
	TestTrue(TEXT("enum value"), Object->Mode == EConfigTestMode::High);
	Object->CallFunctionByNameWithArguments(TEXT("SetMode EConfigTestMode::Low"), Ar, nullptr);
	TestTrue(TEXT("Scoped enum value"), Object->Mode == EConfigTestMode::Low);

	TestTrue(
		TEXT("Object path"), Object->CallFunctionByNameWithArguments(TEXT("SetTarget /Engine/Transient"), Ar, nullptr));
	TestTrue(TEXT("Object found"), Object->Target == GetTransientPackage());
	Object->CallFunctionByNameWithArguments(TEXT("SetTarget None"), Ar, nullptr);
	TestNull(TEXT("None"), Object->Target);

	TestTrue(TEXT("Last FString takes the rest"),
		Object->CallFunctionByNameWithArguments(TEXT("Say 3 hello there, world"), Ar, nullptr));
	TestTrue(TEXT("Rest of the line"), Object->Times == 3 && Object->Message == TEXT("hello there, world"));
	Object->CallFunctionByNameWithArguments(TEXT("Say 2 \"quoted text\""), Ar, nullptr);
	TestEqual(TEXT("Quoted argument"), Object->Message, TEXT("quoted text"));

	UExecTestObject* Executor = NewObject<UExecTestObject>();
	TestTrue(TEXT("Executor"), Object->CallFunctionByNameWithArguments(TEXT("Greet Visitor"), Ar, Executor));
	TestTrue(TEXT("Executor passed first"), Object->Executor == Executor && Object->Message == TEXT("Visitor"));

	// Refusals: not Exec, unknown function; forced call of a non-Exec function.
	const int32 CallsBefore = Object->NumCalls;
	TestFalse(
		TEXT("Non-Exec function refused"), Object->CallFunctionByNameWithArguments(TEXT("NotExec 5"), Ar, nullptr));
	TestFalse(TEXT("Unknown function"), Object->CallFunctionByNameWithArguments(TEXT("NoSuchCommand 1"), Ar, nullptr));
	TestFalse(TEXT("Empty command"), Object->CallFunctionByNameWithArguments(TEXT(""), Ar, nullptr));
	TestEqual(TEXT("Nothing called"), Object->NumCalls, CallsBefore);
	TestTrue(TEXT("Forced call"), Object->CallFunctionByNameWithArguments(TEXT("NotExec 5"), Ar, nullptr, true));
	TestEqual(TEXT("Forced call ran"), Object->IntValue, 5);

	// A missing trailing argument keeps its default (with a warning); a bad one stops the call.
	Ar.Lines.Reset();
	TestTrue(TEXT("Missing arguments"), Object->CallFunctionByNameWithArguments(TEXT("SetValues 7"), Ar, nullptr));
	TestTrue(TEXT("Defaults for the missing arguments"),
		Object->IntValue == 7 && Object->FloatValue == 0.0f && Object->NameValue.IsNone() && Ar.NumWarnings == 2);
	Ar.Lines.Reset();
	const int32 CallsBeforeBad = Object->NumCalls;
	TestTrue(TEXT("Bad argument handled"),
		Object->CallFunctionByNameWithArguments(TEXT("SetValues notanumber 1 A"), Ar, nullptr));
	TestEqual(TEXT("Bad argument: not called"), Object->NumCalls, CallsBeforeBad);
	TestTrue(TEXT("Bad argument reported"), Ar.Lines.Num() > 0 && Ar.Lines.Last().Contains(TEXT("InInt")));

	// UObject::ProcessConsoleExec runs the same path; FSelfRegisteringExec handlers see StaticExec.
	TestTrue(TEXT("ProcessConsoleExec"), Object->ProcessConsoleExec(TEXT("SetValues 9 1 B"), Ar, nullptr));
	TestEqual(TEXT("ProcessConsoleExec value"), Object->IntValue, 9);
	{
		FStaticSelfRegisteringExec Handler(&StaticExecHandler);
		TestTrue(TEXT("Self-registering exec"), FSelfRegisteringExec::StaticExec(nullptr, TEXT("LeonTestCommand"), Ar));
		TestFalse(TEXT("Other commands pass"), FSelfRegisteringExec::StaticExec(nullptr, TEXT("Other"), Ar));
	}
	TestFalse(
		TEXT("Unregistered with its scope"), FSelfRegisteringExec::StaticExec(nullptr, TEXT("LeonTestCommand"), Ar));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
