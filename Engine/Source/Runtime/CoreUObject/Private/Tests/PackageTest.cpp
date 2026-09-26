#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Tests/PackageTestTypes.h"
#include "UObject/GarbageCollection.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** The tests' mount point. Most tests save to memory (FLinkerLoad::RegisterInMemoryPackage), on every platform. */
	const TCHAR* const TestRoot = TEXT("/PackageTest/");

	/** The folder of the mount point: files are only written by the desktop file test. */
	FString GetTestContentDir()
	{
		return FPaths::ProjectIntermediateDir() + TEXT("Tests/CoreUObjectPackage/");
	}

	/** Marks a package and everything in it pending kill and collects it, as if the process had restarted. */
	void DestroyPackage(const FString& PackageName)
	{
		if (UPackage* Package = FindPackage(nullptr, *PackageName))
		{
			TArray<UObject*> Objects;
			GetObjectsWithOuter(Package, Objects, /*bIncludeNestedObjects =*/true);
			for (UObject* Object : Objects)
			{
				Object->MarkPendingKill();
			}
			Package->MarkPendingKill();
		}
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, /*bPerformFullPurge =*/true);
	}

	/**
	 * Registers the test mount point for a test's lifetime; the packages it saved to memory are unregistered and
	 * destroyed at the end.
	 */
	class FPackageTestScope
	{
	public:
		FPackageTestScope()
		{
			FPackageName::RegisterMountPoint(TestRoot, GetTestContentDir());
			UPackageTestObject::GetPostLoadLog().Reset();
		}

		~FPackageTestScope()
		{
			for (const FString& PackageName : PackageNames)
			{
				FLinkerLoad::UnregisterInMemoryPackage(PackageName);
				DestroyPackage(PackageName);
			}
			FPackageName::UnRegisterMountPoint(TestRoot, GetTestContentDir());
		}

		FPackageTestScope(const FPackageTestScope&) = delete;
		FPackageTestScope& operator=(const FPackageTestScope&) = delete;

		/** A new package under the test root. */
		UPackage* NewPackage(const TCHAR* ShortName)
		{
			const FString PackageName = FString(TestRoot) + ShortName;
			PackageNames.AddUnique(PackageName);
			return CreatePackage(*PackageName);
		}

		/** Saves Package (its RF_Public objects) to memory and registers the bytes under its name. */
		bool Save(FAutomationTestBase& Test, UPackage* Package, TArray<uint8>* OutBytes = nullptr)
		{
			TArray<uint8> Bytes;
			const FSavePackageResultStruct Result = UPackage::SaveToMemory(Package, nullptr, RF_Public, Bytes);
			if (!Test.TestTrue(*FString::Printf(TEXT("%s saves"), *Package->GetName()), Result.IsSuccessful()))
			{
				return false;
			}
			Test.TestEqual(TEXT("TotalFileSize is the byte count"), Result.TotalFileSize, int64(Bytes.Num()));
			FLinkerLoad::RegisterInMemoryPackage(Package->GetName(), Bytes);
			PackageNames.AddUnique(Package->GetName());
			if (OutBytes)
			{
				*OutBytes = MoveTemp(Bytes);
			}
			return true;
		}

		/** Registers bytes (a patched package) under PackageName. */
		void Register(const FString& PackageName, const TArray<uint8>& Bytes)
		{
			FLinkerLoad::RegisterInMemoryPackage(PackageName, Bytes);
			PackageNames.AddUnique(PackageName);
		}

		TArray<FString> PackageNames;
	};

	/** Collects the warnings logged while it exists. */
	class FWarningCapture final : public FOutputDevice
	{
	public:
		FWarningCapture()
		{
			GLog->AddOutputDevice(this);
		}

		virtual ~FWarningCapture() override
		{
			GLog->RemoveOutputDevice(this);
		}

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			(void)Category;
			if ((Verbosity & ELogVerbosity::VerbosityMask) == ELogVerbosity::Warning)
			{
				Warnings.Add(V);
			}
		}

		/** True when a warning containing Text was logged. */
		bool HasWarning(const TCHAR* Text) const
		{
			for (const FString& Warning : Warnings)
			{
				if (Warning.Contains(Text))
				{
					return true;
				}
			}
			return false;
		}

		TArray<FString> Warnings;
	};

	/** The summary and tables of package bytes, without loading any object (the cooker's view). */
	TUniquePtr<FLinkerLoad> ReadTables(const FString& PackageName, const TArray<uint8>& Bytes)
	{
		return TUniquePtr<FLinkerLoad>(FLinkerLoad::CreateLinkerFromMemory(nullptr, *PackageName, LOAD_None, Bytes));
	}

	/** True when the package's name table has Name. */
	bool NameTableHas(const FLinkerLoad& Linker, const TCHAR* Name)
	{
		for (const FName& Entry : Linker.NameMap)
		{
			if (Entry.ToString().Equals(Name, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	/** A byte pattern for bulk data. */
	void FillPattern(FByteBulkData& BulkData, int32 Size, uint8 Seed)
	{
		uint8* Data = (uint8*)BulkData.Lock(LOCK_READ_WRITE);
		Data = (uint8*)BulkData.Realloc(Size);
		for (int32 Index = 0; Index < Size; ++Index)
		{
			Data[Index] = uint8(Index * 7 + Seed);
		}
		BulkData.Unlock();
	}

	bool HasPattern(const FByteBulkData& BulkData, int32 Size, uint8 Seed)
	{
		if (BulkData.GetElementCount() != Size)
		{
			return false;
		}
		const uint8* Data = (const uint8*)BulkData.LockReadOnly();
		bool bMatches = true;
		for (int32 Index = 0; Index < Size && bMatches; ++Index)
		{
			bMatches = Data[Index] == uint8(Index * 7 + Seed);
		}
		BulkData.Unlock();
		return bMatches;
	}

	/** Fills every property of Object with values that differ from the defaults. */
	void FillAllProperties(UPackageTestObject* Object)
	{
		Object->Int8Value = -8;
		Object->Int16Value = -1600;
		Object->IntValue = 42;
		Object->Int64Value = -(1ll << 40);
		Object->ByteValue = 200;
		Object->UInt16Value = 60000;
		Object->UInt32Value = 4000000000u;
		Object->UInt64Value = 1ull << 63;
		Object->FloatValue = 2.5f;
		Object->DoubleValue = 0.125;
		Object->bNativeBool = true;
		Object->bBitA = 1;
		Object->bBitB = 0;
		Object->bBitC = 1;
		Object->StringValue = TEXT("Hello package");
		Object->NameValue = FName(TEXT("Leon_7"));
		Object->TextValue = FText::FromString(TEXT("Some text"));
		Object->Mode = EPackageTestMode::Third;
		Object->LegacyMode = PTL_Gamma;
		Object->Location = FVector(10.0f, -20.0f, 30.5f);
		Object->Transform = FTransform(
			FQuat(0.0f, 0.0f, 0.70710678f, 0.70710678f), FVector(1.0f, 2.0f, 3.0f), FVector(2.0f, 2.0f, 2.0f));
		Object->Color = FLinearColor(0.25f, 0.5f, 0.75f, 1.0f);
		Object->Struct.Inner.Count = 9;
		Object->Struct.Inner.Tag = FName(TEXT("InnerTag"));
		Object->Struct.Numbers = {4, 5, 6};
		Object->Struct.Scale = 3.0f;
		Object->ClassRef = UPackageTestSubobject::StaticClass();
		Object->ScriptRef = StaticEnum<EPackageTestMode>();
		Object->SoftClassRef = UPackageTestOwner::StaticClass();
		Object->SoftPath = FSoftObjectPath(TEXT("/PackageTest/Elsewhere.Thing:Sub"));
		Object->IntArray = {1, 2, 3};
		Object->StringArray = {TEXT("A"), TEXT(""), TEXT("C")};
		FPackageTestInner Entry;
		Entry.Count = 1;
		Entry.Label = TEXT("First");
		Object->StructArray.Add(Entry);
		Entry.Count = 5;
		Entry.Label = TEXT("Inner");
		Object->StructArray.Add(Entry);
		Object->VectorArray = {FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, -4.5f)};
		Object->BoolArray = {true, false, true};
		Object->NameSet.Add(FName(TEXT("Red")));
		Object->NameSet.Add(FName(TEXT("Blue")));
		Object->IntSet.Add(3);
		Object->IntSet.Add(-7);
		Object->NameToInt.Add(FName(TEXT("One")), 1);
		Object->NameToInt.Add(FName(TEXT("Two")), 2);
		Entry.Count = 77;
		Entry.Label = TEXT("Mapped");
		Object->StringToStruct.Add(TEXT("Key"), Entry);
		Object->FixedArray[0] = 10;
		Object->FixedArray[2] = 30;
		Object->TransientValue = 99;
		Object->AfterEditorOnly = 11;
		Object->NativeValue = 1234;
	}

	/** Checks the values FillAllProperties set, read back from a loaded object. */
	void CheckAllProperties(FAutomationTestBase& Test, const UPackageTestObject* Object)
	{
		Test.TestEqual(TEXT("int8"), int32(Object->Int8Value), -8);
		Test.TestEqual(TEXT("int16"), int32(Object->Int16Value), -1600);
		Test.TestEqual(TEXT("int32"), Object->IntValue, 42);
		Test.TestEqual(TEXT("int64"), Object->Int64Value, -(1ll << 40));
		Test.TestEqual(TEXT("uint8"), int32(Object->ByteValue), 200);
		Test.TestEqual(TEXT("uint16"), int32(Object->UInt16Value), 60000);
		Test.TestEqual(TEXT("uint32"), Object->UInt32Value, 4000000000u);
		Test.TestEqual(TEXT("uint64"), Object->UInt64Value, 1ull << 63);
		Test.TestEqual(TEXT("float"), Object->FloatValue, 2.5f);
		Test.TestEqual(TEXT("double"), Object->DoubleValue, 0.125);
		Test.TestTrue(TEXT("native bool"), Object->bNativeBool);
		Test.TestTrue(TEXT("bitfield A"), Object->bBitA == 1);
		Test.TestTrue(TEXT("bitfield B"), Object->bBitB == 0);
		Test.TestTrue(TEXT("bitfield C"), Object->bBitC == 1);
		Test.TestEqual(TEXT("FString"), Object->StringValue, TEXT("Hello package"));
		Test.TestEqual(TEXT("FName with a number"), Object->NameValue, FName(TEXT("Leon_7")));
		Test.TestEqual(TEXT("FText"), Object->TextValue.ToString(), TEXT("Some text"));
		Test.TestTrue(TEXT("enum class"), Object->Mode == EPackageTestMode::Third);
		Test.TestTrue(TEXT("TEnumAsByte"), Object->LegacyMode == PTL_Gamma);
		Test.TestTrue(TEXT("FVector (binary struct)"), Object->Location == FVector(10.0f, -20.0f, 30.5f));
		Test.TestTrue(TEXT("FTransform translation"), Object->Transform.GetTranslation() == FVector(1.0f, 2.0f, 3.0f));
		Test.TestTrue(TEXT("FTransform scale"), Object->Transform.GetScale3D() == FVector(2.0f, 2.0f, 2.0f));
		Test.TestEqual(TEXT("FTransform rotation"), Object->Transform.GetRotation().Z, 0.70710678f);
		Test.TestTrue(TEXT("FLinearColor (tagged struct)"), Object->Color == FLinearColor(0.25f, 0.5f, 0.75f, 1.0f));
		Test.TestEqual(TEXT("nested struct: changed member"), Object->Struct.Inner.Count, 9);
		Test.TestEqual(TEXT("nested struct: default member"), Object->Struct.Inner.Label, TEXT("Inner"));
		Test.TestEqual(TEXT("nested struct: name"), Object->Struct.Inner.Tag, FName(TEXT("InnerTag")));
		Test.TestTrue(TEXT("struct array member"), Object->Struct.Numbers == TArray<int32>({4, 5, 6}));
		Test.TestEqual(TEXT("struct float"), Object->Struct.Scale, 3.0f);
		Test.TestTrue(TEXT("/Script class import"), Object->ClassRef.Get() == UPackageTestSubobject::StaticClass());
		Test.TestTrue(TEXT("/Script enum import"), Object->ScriptRef == StaticEnum<EPackageTestMode>());
		Test.TestEqual(
			TEXT("soft class path"), Object->SoftClassRef.ToString(), UPackageTestOwner::StaticClass()->GetPathName());
		Test.TestTrue(TEXT("soft class resolves"), Object->SoftClassRef.Get() == UPackageTestOwner::StaticClass());
		Test.TestEqual(TEXT("soft object path"), Object->SoftPath.ToString(), TEXT("/PackageTest/Elsewhere.Thing:Sub"));
		Test.TestTrue(TEXT("int array"), Object->IntArray == TArray<int32>({1, 2, 3}));
		Test.TestTrue(TEXT("string array"), Object->StringArray == TArray<FString>({TEXT("A"), TEXT(""), TEXT("C")}));
		if (Test.TestEqual(TEXT("struct array"), Object->StructArray.Num(), 2))
		{
			Test.TestEqual(TEXT("struct array [0]"), Object->StructArray[0].Label, TEXT("First"));
			Test.TestEqual(TEXT("struct array [1] default values"), Object->StructArray[1].Count, 5);
		}
		Test.TestTrue(TEXT("FVector array"),
			Object->VectorArray == TArray<FVector>({FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, -4.5f)}));
		Test.TestTrue(TEXT("bool array"), Object->BoolArray == TArray<bool>({true, false, true}));
		Test.TestEqual(TEXT("name set"), Object->NameSet.Num(), 2);
		Test.TestTrue(TEXT("name set content"),
			Object->NameSet.Contains(FName(TEXT("Red"))) && Object->NameSet.Contains(FName(TEXT("Blue"))));
		Test.TestTrue(TEXT("int set"), Object->IntSet.Num() == 2 && Object->IntSet.Contains(-7));
		const int32* Two = Object->NameToInt.Find(FName(TEXT("Two")));
		Test.TestTrue(TEXT("name map"), Object->NameToInt.Num() == 2 && Two && *Two == 2);
		const FPackageTestInner* Mapped = Object->StringToStruct.Find(TEXT("Key"));
		Test.TestTrue(TEXT("struct map"), Mapped && Mapped->Count == 77 && Mapped->Label == TEXT("Mapped"));
		Test.TestEqual(TEXT("C array [0]"), Object->FixedArray[0], 10);
		Test.TestEqual(TEXT("C array [1] keeps its default"), Object->FixedArray[1], 0);
		Test.TestEqual(TEXT("C array [2]"), Object->FixedArray[2], 30);
		Test.TestEqual(TEXT("transient is not saved"), Object->TransientValue, 0);
		Test.TestEqual(TEXT("after the editor-only members"), Object->AfterEditorOnly, 11);
		Test.TestEqual(TEXT("native tail"), Object->NativeValue, 1234);
	}

	/** The MD5 of package bytes with the saving engine's version cleared, so it only changes with the format. */
	FString HashWithoutEngineVersion(const TArray<uint8>& Bytes)
	{
		TArray<uint8> Normalized = Bytes;
		FPackageFileSummary Summary;
		{
			FMemoryReader Reader(Normalized);
			Reader << Summary;
		}
		Summary.SavedByEngineVersion = FEngineVersion(0, 0, 0, 0, Summary.SavedByEngineVersion.GetBranch());
		{
			FMemoryWriter Writer(Normalized);
			Writer << Summary;
		}
		return FMD5::HashBytes(Normalized.GetData(), uint64(Normalized.Num()));
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageRoundTripTest, "System.CoreUObject.Package.RoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageRoundTripTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;

	// A dependency package, and the package under test referencing it, itself and /Script.
	UPackage* DepPackage = Scope.NewPackage(TEXT("RoundTripDep"));
	UPackageTestObject* DepTarget = NewObject<UPackageTestObject>(DepPackage, TEXT("Target"), RF_Public);
	DepTarget->IntValue = 77;

	UPackage* Package = Scope.NewPackage(TEXT("RoundTrip"));
	UPackageTestObject* Asset = NewObject<UPackageTestObject>(Package, TEXT("Asset"), RF_Public);
	FillAllProperties(Asset);
	// Not public: exported because Asset references it.
	UPackageTestObject* Sibling = NewObject<UPackageTestObject>(Package, TEXT("Sibling"));
	Sibling->IntValue = 11;
	// Neither public nor referenced: not saved.
	NewObject<UPackageTestObject>(Package, TEXT("Unreferenced"));
	// Transient: never saved, and saved references to it are null.
	UPackageTestObject* TransientObject =
		NewObject<UPackageTestObject>(Package, TEXT("TransientObject"), RF_Public | RF_Transient);
	UPackageTestObject* OutsideObject = NewObject<UPackageTestObject>(GetTransientPackage());
	Asset->ObjectRef = Sibling;
	Asset->WeakRef = DepTarget;
	Asset->SoftRef = DepTarget;
	Asset->ObjectArray = {Sibling, DepTarget, nullptr, TransientObject, OutsideObject};
	Asset->IntToObject.Add(1, Sibling);
	Asset->IntToObject.Add(2, DepTarget);

	TArray<uint8> Bytes;
	if (!Scope.Save(*this, DepPackage) || !Scope.Save(*this, Package, &Bytes))
	{
		return false;
	}
	{
		TUniquePtr<FLinkerLoad> Tables = ReadTables(Package->GetName(), Bytes);
		if (TestNotNull(TEXT("tables"), Tables.Get()))
		{
			TestEqual(TEXT("exports: Asset, Sibling"), Tables->ExportMap.Num(), 2);
			TestEqual(TEXT("summary version"), Tables->Summary.GetFileVersionUE(), int32(VER_LEON_LATEST));
			TestTrue(TEXT("summary GUID from the package name"),
				Tables->Summary.Guid == FGuid::NewDeterministicGuid(Package->GetName()));
			TestTrue(TEXT("saved by this engine"), Tables->Summary.SavedByEngineVersion == FEngineVersion::Current());
			TestTrue(TEXT("imports the dependency package"), NameTableHas(*Tables, TEXT("/PackageTest/RoundTripDep")));
			TestTrue(TEXT("imports /Script/CoreUObject"), NameTableHas(*Tables, TEXT("/Script/CoreUObject")));
			TestFalse(TEXT("the transient object is not saved"), NameTableHas(*Tables, TEXT("TransientObject")));
			TestFalse(TEXT("the unreferenced object is not saved"), NameTableHas(*Tables, TEXT("Unreferenced")));
			TestFalse(TEXT("a transient property is not saved"), NameTableHas(*Tables, TEXT("TransientValue")));
			for (int32 Index = 1; Index < Tables->NameMap.Num(); ++Index)
			{
				const FString Previous = Tables->NameMap[Index - 1].ToString();
				const FString Current = Tables->NameMap[Index].ToString();
				if (!TestTrue(TEXT("name table sorted, no duplicates"),
						Previous.Compare(Current, ESearchCase::IgnoreCase) < 0))
				{
					break;
				}
			}
		}
	}

	DestroyPackage(Package->GetName());
	DestroyPackage(DepPackage->GetName());
	TestNull(TEXT("destroyed"), FindPackage(nullptr, TEXT("/PackageTest/RoundTrip")));

	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/RoundTrip"), LOAD_None);
	if (!TestNotNull(TEXT("LoadPackage"), Loaded))
	{
		return false;
	}
	TestTrue(TEXT("fully loaded"), Loaded->IsFullyLoaded());
	TestNull(TEXT("the linker is released"), Loaded->LinkerLoad);
	TestTrue(
		TEXT("the GUID comes from the summary"), Loaded->GetGuid() == FGuid::NewDeterministicGuid(Loaded->GetName()));
	UPackageTestObject* LoadedAsset = FindObject<UPackageTestObject>(Loaded, TEXT("Asset"));
	if (!TestNotNull(TEXT("Asset loaded"), LoadedAsset))
	{
		return false;
	}
	CheckAllProperties(*this, LoadedAsset);
	TestTrue(TEXT("flags restored"), LoadedAsset->HasAnyFlags(RF_Public));
	TestTrue(TEXT("RF_WasLoaded"), LoadedAsset->HasAllFlags(RF_WasLoaded | RF_LoadCompleted));
	TestFalse(TEXT("RF_NeedLoad / RF_NeedPostLoad cleared"), LoadedAsset->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad));
	TestNull(TEXT("unreferenced object"), FindObject<UPackageTestObject>(Loaded, TEXT("Unreferenced")));
	TestNull(TEXT("transient object"), FindObject<UPackageTestObject>(Loaded, TEXT("TransientObject")));

	UPackageTestObject* LoadedSibling = FindObject<UPackageTestObject>(Loaded, TEXT("Sibling"));
	UPackageTestObject* LoadedDep = FindObject<UPackageTestObject>(nullptr, TEXT("/PackageTest/RoundTripDep.Target"));
	if (!TestNotNull(TEXT("internal export"), LoadedSibling) || !TestNotNull(TEXT("dependency loaded"), LoadedDep))
	{
		return false;
	}
	TestFalse(TEXT("the sibling is not public"), LoadedSibling->HasAnyFlags(RF_Public));
	TestEqual(TEXT("sibling value"), LoadedSibling->IntValue, 11);
	TestTrue(TEXT("object reference to an export"), LoadedAsset->ObjectRef == LoadedSibling);
	TestTrue(TEXT("weak reference to an import"), LoadedAsset->WeakRef.Get() == LoadedDep);
	TestTrue(TEXT("soft reference resolves once loaded"), LoadedAsset->SoftRef.Get() == LoadedDep);
	if (TestEqual(TEXT("object array"), LoadedAsset->ObjectArray.Num(), 5))
	{
		TestTrue(TEXT("[0] export"), LoadedAsset->ObjectArray[0] == LoadedSibling);
		TestTrue(TEXT("[1] import"), LoadedAsset->ObjectArray[1] == LoadedDep);
		TestNull(TEXT("[2] null"), LoadedAsset->ObjectArray[2]);
		TestNull(TEXT("[3] transient object in the package"), LoadedAsset->ObjectArray[3]);
		TestNull(TEXT("[4] object in the transient package"), LoadedAsset->ObjectArray[4]);
	}
	UObject* const* MapSibling = LoadedAsset->IntToObject.Find(1);
	UObject* const* MapDep = LoadedAsset->IntToObject.Find(2);
	TestTrue(TEXT("object map"), MapSibling && *MapSibling == LoadedSibling && MapDep && *MapDep == LoadedDep);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageHardReferenceTest, "System.CoreUObject.Package.HardReference",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageHardReferenceTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	// Main -> Middle -> Leaf, plus a reference back from Leaf to Main (a circular dependency).
	UPackage* LeafPackage = Scope.NewPackage(TEXT("HardLeaf"));
	UPackage* MiddlePackage = Scope.NewPackage(TEXT("HardMiddle"));
	UPackage* MainPackage = Scope.NewPackage(TEXT("HardMain"));
	UPackageTestObject* Leaf = NewObject<UPackageTestObject>(LeafPackage, TEXT("Leaf"), RF_Public);
	UPackageTestObject* Middle = NewObject<UPackageTestObject>(MiddlePackage, TEXT("Middle"), RF_Public);
	UPackageTestObject* Main = NewObject<UPackageTestObject>(MainPackage, TEXT("Main"), RF_Public);
	Leaf->IntValue = 3;
	Middle->IntValue = 2;
	Main->IntValue = 1;
	Main->ObjectRef = Middle;
	Middle->ObjectRef = Leaf;
	Leaf->ObjectArray.Add(Main);
	if (!Scope.Save(*this, LeafPackage) || !Scope.Save(*this, MiddlePackage) || !Scope.Save(*this, MainPackage))
	{
		return false;
	}
	for (const FString& PackageName : Scope.PackageNames)
	{
		DestroyPackage(PackageName);
	}

	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/HardMain"), LOAD_None);
	if (!TestNotNull(TEXT("main package"), Loaded))
	{
		return false;
	}
	UPackage* LoadedMiddlePackage = FindPackage(nullptr, TEXT("/PackageTest/HardMiddle"));
	UPackage* LoadedLeafPackage = FindPackage(nullptr, TEXT("/PackageTest/HardLeaf"));
	TestTrue(TEXT("the imports loaded their packages"), LoadedMiddlePackage && LoadedLeafPackage);
	TestTrue(TEXT("dependencies fully loaded"),
		LoadedMiddlePackage && LoadedMiddlePackage->IsFullyLoaded() && LoadedLeafPackage &&
			LoadedLeafPackage->IsFullyLoaded());
	UPackageTestObject* LoadedMain = FindObject<UPackageTestObject>(Loaded, TEXT("Main"));
	UPackageTestObject* LoadedMiddle = LoadedMain ? Cast<UPackageTestObject>(LoadedMain->ObjectRef) : nullptr;
	UPackageTestObject* LoadedLeaf = LoadedMiddle ? Cast<UPackageTestObject>(LoadedMiddle->ObjectRef) : nullptr;
	if (!TestNotNull(TEXT("Main -> Middle"), LoadedMiddle) || !TestNotNull(TEXT("Middle -> Leaf"), LoadedLeaf))
	{
		return false;
	}
	TestEqual(TEXT("Middle's values"), LoadedMiddle->IntValue, 2);
	TestEqual(TEXT("Leaf's values"), LoadedLeaf->IntValue, 3);
	TestTrue(TEXT("Leaf -> Main (circular)"),
		LoadedLeaf->ObjectArray.Num() == 1 && LoadedLeaf->ObjectArray[0] == LoadedMain);
	TestTrue(TEXT("LoadPackage again returns the loaded package"),
		LoadPackage(nullptr, TEXT("/PackageTest/HardMain"), LOAD_None) == Loaded);
	TestTrue(TEXT("LoadObject finds the loaded object"),
		LoadObject<UPackageTestObject>(nullptr, TEXT("/PackageTest/HardLeaf.Leaf")) == LoadedLeaf);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageSoftReferenceTest, "System.CoreUObject.Package.SoftReference",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageSoftReferenceTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	UPackage* DepPackage = Scope.NewPackage(TEXT("SoftDep"));
	UPackageTestObject* Target = NewObject<UPackageTestObject>(DepPackage, TEXT("Target"), RF_Public);
	Target->IntValue = 21;
	UPackageTestSubobject* Inner = NewObject<UPackageTestSubobject>(Target, TEXT("Inner"));
	Inner->Value = 8;
	Target->ObjectRef = Inner;

	UPackage* Package = Scope.NewPackage(TEXT("SoftMain"));
	UPackageTestObject* Asset = NewObject<UPackageTestObject>(Package, TEXT("Asset"), RF_Public);
	Asset->SoftRef = Target;
	Asset->SoftPath = FSoftObjectPath(Inner);
	Asset->SoftClassRef = UPackageTestOwner::StaticClass();

	TArray<uint8> Bytes;
	if (!Scope.Save(*this, DepPackage) || !Scope.Save(*this, Package, &Bytes))
	{
		return false;
	}
	TUniquePtr<FLinkerLoad> Tables = ReadTables(Package->GetName(), Bytes);
	if (!TestNotNull(TEXT("tables"), Tables.Get()))
	{
		return false;
	}
	// Soft references are not imports: the package is listed in the soft package references table only.
	TestEqual(TEXT("one soft package reference (no /Script one)"), Tables->SoftPackageReferenceList.Num(), 1);
	TestTrue(TEXT("the soft package"),
		Tables->SoftPackageReferenceList.Num() == 1 &&
			Tables->SoftPackageReferenceList[0] == FName(TEXT("/PackageTest/SoftDep")));
	bool bImportsDep = false;
	for (const FObjectImport& Import : Tables->ImportMap)
	{
		bImportsDep |= Import.ObjectName == FName(TEXT("/PackageTest/SoftDep"));
	}
	TestFalse(TEXT("no import of the soft package"), bImportsDep);

	DestroyPackage(Package->GetName());
	DestroyPackage(DepPackage->GetName());
	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/SoftMain"), LOAD_None);
	UPackageTestObject* LoadedAsset = Loaded ? FindObject<UPackageTestObject>(Loaded, TEXT("Asset")) : nullptr;
	if (!TestNotNull(TEXT("loaded"), LoadedAsset))
	{
		return false;
	}
	TestNull(TEXT("a soft reference does not load its package"), FindPackage(nullptr, TEXT("/PackageTest/SoftDep")));
	TestTrue(TEXT("pending"), LoadedAsset->SoftRef.IsPending());
	UPackageTestObject* LoadedTarget = Cast<UPackageTestObject>(LoadedAsset->SoftRef.LoadSynchronous());
	if (!TestNotNull(TEXT("LoadSynchronous loads the package"), LoadedTarget))
	{
		return false;
	}
	TestEqual(TEXT("the loaded target"), LoadedTarget->IntValue, 21);
	TestTrue(TEXT("valid now"), LoadedAsset->SoftRef.IsValid());
	UObject* LoadedInner = LoadedAsset->SoftPath.TryLoad();
	TestTrue(TEXT("TryLoad of a subobject path"), LoadedInner && LoadedInner == LoadedTarget->ObjectRef);
	TestTrue(TEXT("soft class"), LoadedAsset->SoftClassRef.LoadSynchronous() == UPackageTestOwner::StaticClass());

	// A path to a package that does not exist: null, with a warning.
	FWarningCapture WarningLog;
	TestNull(TEXT("missing soft target"), FSoftObjectPath(TEXT("/PackageTest/NoSuchPackage.Thing")).TryLoad());
	TestTrue(TEXT("warned"), WarningLog.HasWarning(TEXT("NoSuchPackage")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageSchemaEvolutionTest, "System.CoreUObject.Package.SchemaEvolution",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageSchemaEvolutionTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	UPackage* Package = Scope.NewPackage(TEXT("Schema"));
	UPackageTestSchemaV1* Old = NewObject<UPackageTestSchemaV1>(Package, TEXT("Versioned"), RF_Public);
	Old->Kept = 1;
	Old->Removed = 2;
	Old->OldName = 3;
	Old->Widened = -123456;
	Old->Precise = 0.75f;
	Old->ModeByte = 10;
	Old->LegacyMode = PTL_Beta;
	Old->Mismatched = TEXT("not a number");
	Old->Nested.Count = 12;
	Old->After = 4;
	TArray<uint8> Bytes;
	if (!Scope.Save(*this, Package, &Bytes))
	{
		return false;
	}

	// The class is now version 2: rename it in the name table (the same length, so nothing else moves).
	const char* OldClassName = "PackageTestSchemaV1";
	const int32 NameLength = FCString::Strlen(OldClassName);
	int32 Found = 0;
	for (int32 Offset = 0; Offset + NameLength <= Bytes.Num(); ++Offset)
	{
		if (FMemory::Memcmp(&Bytes[Offset], OldClassName, SIZE_T(NameLength)) == 0)
		{
			Bytes[Offset + NameLength - 1] = uint8('2');
			++Found;
		}
	}
	TestEqual(TEXT("the class name is in the name table once"), Found, 1);
	DestroyPackage(Package->GetName());
	Scope.Register(TEXT("/PackageTest/Schema"), Bytes);

	FWarningCapture WarningLog;
	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/Schema"), LOAD_None);
	UPackageTestSchemaV2* New = Loaded ? FindObject<UPackageTestSchemaV2>(Loaded, TEXT("Versioned")) : nullptr;
	if (!TestNotNull(TEXT("loaded as version 2"), New))
	{
		return false;
	}
	TestEqual(TEXT("kept property"), New->Kept, 1);
	TestEqual(TEXT("renamed property keeps its default"), New->NewName, 0);
	TestEqual(TEXT("int32 to int64"), New->Widened, int64(-123456));
	TestEqual(TEXT("float to double"), New->Precise, 0.75);
	TestTrue(TEXT("byte to enum by value"), New->ModeByte == EPackageTestMode::Third);
	TestTrue(TEXT("TEnumAsByte to enum class by name"), New->LegacyMode == EPackageTestLegacyV2::PTL_Beta);
	TestEqual(TEXT("an unconvertible type keeps its default"), New->Mismatched, 7);
	TestEqual(TEXT("nested struct"), New->Nested.Count, 12);
	TestEqual(TEXT("a property after the skipped ones"), New->After, 4);
	TestTrue(TEXT("the type mismatch is a warning"), WarningLog.HasWarning(TEXT("Mismatched")));
	TestFalse(TEXT("a removed property is not a warning"), WarningLog.HasWarning(TEXT("Removed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageMissingImportTest, "System.CoreUObject.Package.MissingImport",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageMissingImportTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	UPackage* DepPackage = Scope.NewPackage(TEXT("MissingDep"));
	UPackage* GonePackage = Scope.NewPackage(TEXT("GoneDep"));
	UPackage* Package = Scope.NewPackage(TEXT("MissingMain"));
	UPackageTestObject* Kept = NewObject<UPackageTestObject>(DepPackage, TEXT("Kept"), RF_Public);
	UPackageTestObject* Removed = NewObject<UPackageTestObject>(DepPackage, TEXT("Removed"), RF_Public);
	UPackageTestObject* InGone = NewObject<UPackageTestObject>(GonePackage, TEXT("Thing"), RF_Public);
	UPackageTestObject* Asset = NewObject<UPackageTestObject>(Package, TEXT("Asset"), RF_Public);
	Asset->ObjectArray = {Kept, Removed, InGone};
	Asset->IntValue = 5;
	if (!Scope.Save(*this, DepPackage) || !Scope.Save(*this, GonePackage) || !Scope.Save(*this, Package))
	{
		return false;
	}
	for (const FString& PackageName : Scope.PackageNames)
	{
		DestroyPackage(PackageName);
	}

	// A new version of the dependency without Removed; the other package goes away.
	DepPackage = Scope.NewPackage(TEXT("MissingDep"));
	NewObject<UPackageTestObject>(DepPackage, TEXT("Kept"), RF_Public);
	TArray<uint8> DepBytes;
	if (!Scope.Save(*this, DepPackage, &DepBytes))
	{
		return false;
	}
	TUniquePtr<FLinkerLoad> DepTables = ReadTables(DepPackage->GetName(), DepBytes);
	TestTrue(TEXT("the dependency lost an export"), DepTables && DepTables->ExportMap.Num() == 1);
	DestroyPackage(DepPackage->GetName());
	FLinkerLoad::UnregisterInMemoryPackage(TEXT("/PackageTest/GoneDep"));

	FWarningCapture WarningLog;
	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/MissingMain"), LOAD_None);
	UPackageTestObject* LoadedAsset = Loaded ? FindObject<UPackageTestObject>(Loaded, TEXT("Asset")) : nullptr;
	if (!TestNotNull(TEXT("the package loads anyway"), LoadedAsset))
	{
		return false;
	}
	TestEqual(TEXT("its own data"), LoadedAsset->IntValue, 5);
	if (TestEqual(TEXT("array kept"), LoadedAsset->ObjectArray.Num(), 3))
	{
		TestNotNull(TEXT("a present import"), LoadedAsset->ObjectArray[0]);
		if (!TestNull(TEXT("a removed object resolves to null"), LoadedAsset->ObjectArray[1]))
		{
			AddError(FString::Printf(TEXT("it resolved to %s"), *LoadedAsset->ObjectArray[1]->GetFullName()));
		}
		TestNull(TEXT("an object of a missing package resolves to null"), LoadedAsset->ObjectArray[2]);
	}
	TestTrue(TEXT("warned about the removed object"), WarningLog.HasWarning(TEXT("/PackageTest/MissingDep.Removed")));
	TestTrue(TEXT("warned about the missing package"), WarningLog.HasWarning(TEXT("/PackageTest/GoneDep")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageDeterministicTest, "System.CoreUObject.Package.Deterministic",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageDeterministicTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	UPackage* Package = Scope.NewPackage(TEXT("Deterministic"));
	// Filtered like a cooked package, so the flags are the same on builds with and without editor-only data.
	Package->SetPackageFlags(uint32(PKG_FilterEditorOnly));
	UPackageTestObject* Asset = NewObject<UPackageTestObject>(Package, TEXT("Asset"), RF_Public);
	FillAllProperties(Asset);
	UPackageTestObject* Second = NewObject<UPackageTestObject>(Package, TEXT("Second"), RF_Public);
	Second->IntValue = 2;
	Second->StringArray = {TEXT("x")};
	Asset->ObjectRef = Second;
	Second->ObjectRef = Asset;
	FillPattern(Asset->BulkData, 300, 1);

	TArray<uint8> First;
	TArray<uint8> Again;
	if (!Scope.Save(*this, Package, &First) || !Scope.Save(*this, Package, &Again))
	{
		return false;
	}
	TestTrue(TEXT("saving twice gives the same bytes"), First == Again);

	// A load and a new save give the same bytes again: nothing depends on the objects' history or addresses.
	DestroyPackage(Package->GetName());
	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/Deterministic"), LOAD_None);
	if (!TestNotNull(TEXT("loaded"), Loaded))
	{
		return false;
	}
	TArray<uint8> Resaved;
	if (!Scope.Save(*this, Loaded, &Resaved))
	{
		return false;
	}
	if (!TestTrue(TEXT("a loaded package saves to the same bytes"), First == Resaved))
	{
		for (int32 Index = 0; Index < FMath::Min(First.Num(), Resaved.Num()); ++Index)
		{
			if (First[Index] != Resaved[Index])
			{
				AddError(
					FString::Printf(TEXT("first difference at byte %d of %d / %d"), Index, First.Num(), Resaved.Num()));
				break;
			}
		}
	}

	// The same bytes as every other build and platform that ran this test (Win64, the PS2 TestPAL): the hash of the
	// file with the engine version cleared. It changes only when the format (or this fixture) does; update it then.
	const FString Hash = HashWithoutEngineVersion(First);
	UE_LOG(
		LogTemp, Display, TEXT("Package determinism: %d bytes, MD5 without the engine version %s"), First.Num(), *Hash);
	TestEqual(TEXT("golden hash"), Hash, TEXT("4588866aed5cfdf92d196e4fe300e4ce"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageBulkDataTest, "System.CoreUObject.Package.BulkData",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageBulkDataTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	UPackage* Package = Scope.NewPackage(TEXT("Bulk"));
	UPackageTestObject* AtEnd = NewObject<UPackageTestObject>(Package, TEXT("AtEnd"), RF_Public);
	UPackageTestObject* Inline = NewObject<UPackageTestObject>(Package, TEXT("Inline"), RF_Public);
	UPackageTestObject* Empty = NewObject<UPackageTestObject>(Package, TEXT("Empty"), RF_Public);
	FillPattern(AtEnd->BulkData, 4096, 3);
	FillPattern(Inline->BulkData, 64, 9);
	Inline->BulkData.SetBulkDataFlags(BULKDATA_ForceInlinePayload);
	Empty->IntValue = 1;

	TArray<uint8> Bytes;
	if (!Scope.Save(*this, Package, &Bytes))
	{
		return false;
	}
	TUniquePtr<FLinkerLoad> Tables = ReadTables(Package->GetName(), Bytes);
	if (!TestNotNull(TEXT("tables"), Tables.Get()))
	{
		return false;
	}
	const FPackageFileSummary& Summary = Tables->Summary;
	int64 ExportDataEnd = 0;
	for (const FObjectExport& Export : Tables->ExportMap)
	{
		ExportDataEnd = FMath::Max(ExportDataEnd, Export.SerialOffset + Export.SerialSize);
	}
	TestTrue(TEXT("the bulk data follows every export"), Summary.BulkDataStartOffset == ExportDataEnd);
	// Only the end-of-file payload is there (plus the closing tag).
	TestEqual(TEXT("one payload at the end"), int64(Bytes.Num()) - Summary.BulkDataStartOffset, int64(4096 + 4));
	TestTrue(TEXT("the payload bytes"),
		Bytes[int32(Summary.BulkDataStartOffset)] == 3 && Bytes[int32(Summary.BulkDataStartOffset) + 1] == 10);

	DestroyPackage(Package->GetName());
	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/Bulk"), LOAD_None);
	UPackageTestObject* LoadedAtEnd = Loaded ? FindObject<UPackageTestObject>(Loaded, TEXT("AtEnd")) : nullptr;
	UPackageTestObject* LoadedInline = Loaded ? FindObject<UPackageTestObject>(Loaded, TEXT("Inline")) : nullptr;
	UPackageTestObject* LoadedEmpty = Loaded ? FindObject<UPackageTestObject>(Loaded, TEXT("Empty")) : nullptr;
	if (!TestNotNull(TEXT("AtEnd"), LoadedAtEnd) || !TestNotNull(TEXT("Inline"), LoadedInline) ||
		!TestNotNull(TEXT("Empty"), LoadedEmpty))
	{
		return false;
	}
	TestTrue(TEXT("end-of-file payload loaded"), HasPattern(LoadedAtEnd->BulkData, 4096, 3));
	TestEqual(TEXT("read at the start of the bulk data"), LoadedAtEnd->BulkData.GetBulkDataOffsetInFile(), int64(0));
	TestTrue(TEXT("inline payload loaded"), HasPattern(LoadedInline->BulkData, 64, 9));
	TestTrue(TEXT("inline flag kept"), (LoadedInline->BulkData.GetBulkDataFlags() & BULKDATA_ForceInlinePayload) != 0);
	TestEqual(TEXT("empty bulk data"), LoadedEmpty->BulkData.GetElementCount(), int64(0));

	// Outside a package the payload is inline.
	TArray<uint8> Memory;
	FMemoryWriter Writer(Memory);
	LoadedAtEnd->BulkData.Serialize(Writer, LoadedAtEnd);
	FByteBulkData Copy;
	FMemoryReader Reader(Memory);
	Copy.Serialize(Reader, nullptr);
	TestTrue(TEXT("inline in a plain archive"), HasPattern(Copy, 4096, 3) && !Reader.IsError());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageEditorOnlyTest, "System.CoreUObject.Package.EditorOnlyData",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageEditorOnlyTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	UPackage* Unfiltered = Scope.NewPackage(TEXT("EditorData"));
	UPackage* Filtered = Scope.NewPackage(TEXT("CookedData"));
	Filtered->SetPackageFlags(uint32(PKG_FilterEditorOnly));
	UPackageTestObject* WithEditorData = NewObject<UPackageTestObject>(Unfiltered, TEXT("Asset"), RF_Public);
	UPackageTestObject* WithoutEditorData = NewObject<UPackageTestObject>(Filtered, TEXT("Asset"), RF_Public);
	WithEditorData->AfterEditorOnly = 21;
	WithoutEditorData->AfterEditorOnly = 22;
	// An editor-only object each asset owns and references: a filtered package leaves it out.
	WithEditorData->ObjectRef = NewObject<UPackageTestEditorOnlyObject>(WithEditorData, TEXT("ImportData"));
	WithoutEditorData->ObjectRef = NewObject<UPackageTestEditorOnlyObject>(WithoutEditorData, TEXT("ImportData"));
	#if WITH_EDITORONLY_DATA
	WithEditorData->EditorNote = TEXT("Kept");
	WithEditorData->EditorCount = 30;
	WithoutEditorData->EditorNote = TEXT("Stripped");
	WithoutEditorData->EditorCount = 40;
	#endif
	TArray<uint8> UnfilteredBytes;
	TArray<uint8> FilteredBytes;
	if (!Scope.Save(*this, Unfiltered, &UnfilteredBytes) || !Scope.Save(*this, Filtered, &FilteredBytes))
	{
		return false;
	}
	TUniquePtr<FLinkerLoad> UnfilteredTables = ReadTables(Unfiltered->GetName(), UnfilteredBytes);
	TUniquePtr<FLinkerLoad> FilteredTables = ReadTables(Filtered->GetName(), FilteredBytes);
	if (!TestNotNull(TEXT("tables"), UnfilteredTables.Get()) || !TestNotNull(TEXT("tables"), FilteredTables.Get()))
	{
		return false;
	}
	TestTrue(TEXT("PKG_FilterEditorOnly saved"),
		(FilteredTables->Summary.GetPackageFlags() & uint32(PKG_FilterEditorOnly)) != 0);
	TestFalse(TEXT("filtered: no editor-only property"),
		NameTableHas(*FilteredTables, TEXT("EditorNote")) || NameTableHas(*FilteredTables, TEXT("EditorCount")));
	TestEqual(TEXT("filtered: no editor-only object"), FilteredTables->ExportMap.Num(), 1);
	TestFalse(TEXT("filtered: not even its name"), NameTableHas(*FilteredTables, TEXT("ImportData")));
	#if WITH_EDITORONLY_DATA
	TestEqual(TEXT("unfiltered: the editor-only object is saved"), UnfilteredTables->ExportMap.Num(), 2);
	TestFalse(TEXT("an editor build saves editor-only data unless filtered"),
		(UnfilteredTables->Summary.GetPackageFlags() & uint32(PKG_FilterEditorOnly)) != 0);
	TestTrue(TEXT("unfiltered: the editor-only properties"), NameTableHas(*UnfilteredTables, TEXT("EditorNote")));
	#else
	// This build has no editor-only properties, so every package it saves is filtered (D14).
	TestTrue(TEXT("a build without editor-only data always filters"),
		(UnfilteredTables->Summary.GetPackageFlags() & uint32(PKG_FilterEditorOnly)) != 0);
	#endif

	DestroyPackage(Unfiltered->GetName());
	DestroyPackage(Filtered->GetName());
	UPackageTestObject* LoadedWith = LoadObject<UPackageTestObject>(nullptr, TEXT("/PackageTest/EditorData.Asset"));
	UPackageTestObject* LoadedWithout = LoadObject<UPackageTestObject>(nullptr, TEXT("/PackageTest/CookedData.Asset"));
	if (!TestNotNull(TEXT("unfiltered loads"), LoadedWith) || !TestNotNull(TEXT("filtered loads"), LoadedWithout))
	{
		return false;
	}
	TestEqual(TEXT("unfiltered: the rest loads"), LoadedWith->AfterEditorOnly, 21);
	TestEqual(TEXT("filtered: the rest loads"), LoadedWithout->AfterEditorOnly, 22);
	TestNull(TEXT("filtered: the reference to the editor-only object is null"), LoadedWithout->ObjectRef);
	#if WITH_EDITORONLY_DATA
	TestTrue(TEXT("unfiltered: the editor-only object loads"),
		LoadedWith->ObjectRef != nullptr && LoadedWith->ObjectRef->IsA<UPackageTestEditorOnlyObject>());
	TestEqual(TEXT("unfiltered: editor-only values loaded"), LoadedWith->EditorNote, TEXT("Kept"));
	TestEqual(TEXT("unfiltered: editor-only count"), LoadedWith->EditorCount, 30);
	TestEqual(TEXT("filtered: editor-only values keep their defaults"), LoadedWithout->EditorNote, TEXT("Editor"));
	TestEqual(TEXT("filtered: editor-only count default"), LoadedWithout->EditorCount, 3);
	#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageNameTest, "System.CoreUObject.Package.PackageName",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageNameTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	TestEqual(TEXT("asset extension"), FPackageName::GetAssetPackageExtension(), TEXT(".lasset"));
	TestEqual(TEXT("map extension"), FPackageName::GetMapPackageExtension(), TEXT(".lmap"));
	TestTrue(TEXT("package extension"), FPackageName::IsPackageExtension(TEXT("lmap")));
	TestFalse(TEXT("not a package extension"), FPackageName::IsPackageExtension(TEXT(".uasset")));

	TestTrue(TEXT("/Game/ mount point"), FPackageName::MountPointExists(TEXT("/Game/")));
	TestTrue(TEXT("/Engine/ mount point"), FPackageName::MountPointExists(TEXT("Engine")));
	TestTrue(TEXT("registered mount point"), FPackageName::MountPointExists(TestRoot));
	TestFalse(TEXT("unknown mount point"), FPackageName::MountPointExists(TEXT("/Nowhere/")));

	TestTrue(TEXT("valid"), FPackageName::IsValidLongPackageName(TEXT("/Game/Maps/Arena")));
	TestTrue(TEXT("valid under a registered root"), FPackageName::IsValidLongPackageName(TEXT("/PackageTest/A/B")));
	TestFalse(TEXT("no root"), FPackageName::IsValidLongPackageName(TEXT("/Nowhere/Arena")));
	TestFalse(TEXT("relative"), FPackageName::IsValidLongPackageName(TEXT("Game/Arena")));
	TestFalse(TEXT("trailing slash"), FPackageName::IsValidLongPackageName(TEXT("/Game/Maps/")));
	TestFalse(TEXT("double slash"), FPackageName::IsValidLongPackageName(TEXT("/Game//Arena")));
	TestFalse(TEXT("a dot"), FPackageName::IsValidLongPackageName(TEXT("/Game/Arena.Arena")));
	TestFalse(TEXT("a space"), FPackageName::IsValidLongPackageName(TEXT("/Game/My Arena")));
	TestFalse(TEXT("the root alone"), FPackageName::IsValidLongPackageName(TEXT("/Game")));
	FText Reason;
	TestFalse(
		TEXT("/Script is read-only"), FPackageName::IsValidLongPackageName(TEXT("/Script/Engine"), false, &Reason));
	TestTrue(TEXT("reason given"), !Reason.ToString().IsEmpty());
	TestTrue(TEXT("/Script with read-only roots"), FPackageName::IsValidLongPackageName(TEXT("/Script/Engine"), true));
	TestTrue(TEXT("script package"), FPackageName::IsScriptPackage(TEXT("/Script/CoreUObject")));

	FString Filename;
	TestTrue(TEXT("to a file"),
		FPackageName::TryConvertLongPackageNameToFilename(TEXT("/Game/Maps/Arena"), Filename, TEXT(".lmap")));
	TestEqual(TEXT("under the project content"), Filename, FPaths::ProjectContentDir() + TEXT("Maps/Arena.lmap"));
	TestEqual(TEXT("engine content"),
		FPackageName::LongPackageNameToFilename(
			TEXT("/Engine/EngineMaterials/M_Default"), FPackageName::GetAssetPackageExtension()),
		FPaths::EngineContentDir() + TEXT("EngineMaterials/M_Default.lasset"));
	TestFalse(TEXT("/Script has no file"),
		FPackageName::TryConvertLongPackageNameToFilename(TEXT("/Script/Engine"), Filename));

	FString PackageName;
	TestTrue(TEXT("from a file"),
		FPackageName::TryConvertFilenameToLongPackageName(
			FPaths::ProjectContentDir() + TEXT("Maps/Arena.lmap"), PackageName));
	TestEqual(TEXT("the long name"), PackageName, TEXT("/Game/Maps/Arena"));
	TestEqual(TEXT("from a registered folder"),
		FPackageName::FilenameToLongPackageName(GetTestContentDir() + TEXT("Sub/Thing.lasset")),
		TEXT("/PackageTest/Sub/Thing"));
	TestTrue(TEXT("a long name converts to itself"),
		FPackageName::TryConvertFilenameToLongPackageName(TEXT("/Game/Maps/Arena"), PackageName) &&
			PackageName == TEXT("/Game/Maps/Arena"));
	FString Failure;
	TestFalse(TEXT("outside every mount point"),
		FPackageName::TryConvertFilenameToLongPackageName(TEXT("C:/Elsewhere/Thing.lasset"), PackageName, &Failure));
	TestFalse(TEXT("failure reason"), Failure.IsEmpty());

	TestEqual(TEXT("ObjectPathToPackageName"),
		FPackageName::ObjectPathToPackageName(TEXT("/Game/Maps/Arena.Arena:PersistentLevel")),
		TEXT("/Game/Maps/Arena"));
	TestEqual(TEXT("ObjectPathToObjectName"),
		FPackageName::ObjectPathToObjectName(TEXT("/Game/Maps/Arena.Arena:PersistentLevel")), TEXT("PersistentLevel"));
	TestEqual(TEXT("GetShortName"), FPackageName::GetShortName(TEXT("/Game/Maps/Arena")), TEXT("Arena"));
	TestEqual(
		TEXT("GetLongPackagePath"), FPackageName::GetLongPackagePath(TEXT("/Game/Maps/Arena")), TEXT("/Game/Maps"));
	FString Root;
	FString Path;
	FString Name;
	TestTrue(TEXT("SplitLongPackageName"),
		FPackageName::SplitLongPackageName(TEXT("/Game/Maps/Desert/Arena"), Root, Path, Name));
	TestEqual(TEXT("root"), Root, TEXT("/Game/"));
	TestEqual(TEXT("path"), Path, TEXT("Maps/Desert/"));
	TestEqual(TEXT("name"), Name, TEXT("Arena"));
	TestTrue(TEXT("SplitLongPackageName, root without slash"),
		FPackageName::SplitLongPackageName(TEXT("/Engine/Arena"), Root, Path, Name, true) && Root == TEXT("Engine/") &&
			Path.IsEmpty() && Name == TEXT("Arena"));
	TestEqual(TEXT("mount point"), FPackageName::GetPackageMountPoint(TEXT("/Game/Maps/Arena")), FName(TEXT("Game")));

	// Existence: /Script never has a file; a package registered in memory exists.
	TestFalse(TEXT("/Script does not exist on disk"), FPackageName::DoesPackageExist(TEXT("/Script/CoreUObject")));
	TestFalse(TEXT("missing"), FPackageName::DoesPackageExist(TEXT("/PackageTest/NotSaved")));
	UPackage* Package = Scope.NewPackage(TEXT("Exists"));
	NewObject<UPackageTestObject>(Package, TEXT("Asset"), RF_Public);
	if (!Scope.Save(*this, Package))
	{
		return false;
	}
	TestTrue(TEXT("in memory"), FPackageName::DoesPackageExist(TEXT("/PackageTest/Exists")));
	TestTrue(TEXT("LoadPackage of /Script is the compiled-in package"),
		LoadPackage(nullptr, TEXT("/Script/CoreUObject"), LOAD_None) ==
			FindPackage(nullptr, TEXT("/Script/CoreUObject")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackagePostLoadTest, "System.CoreUObject.Package.PostLoad",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackagePostLoadTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	UPackage* DepPackage = Scope.NewPackage(TEXT("PostLoadDep"));
	UPackage* Package = Scope.NewPackage(TEXT("PostLoadMain"));
	UPackageTestObject* Dep = NewObject<UPackageTestObject>(DepPackage, TEXT("Dep"), RF_Public);
	// Exports are serialized in export order (sorted by path): "A" before "B". A points at B, so its PostLoad
	// only sees B's loaded value if every export was serialized before the first PostLoad.
	UPackageTestObject* A = NewObject<UPackageTestObject>(Package, TEXT("A"), RF_Public);
	UPackageTestObject* B = NewObject<UPackageTestObject>(Package, TEXT("B"), RF_Public);
	Dep->IntValue = 30;
	A->IntValue = 10;
	B->IntValue = 20;
	A->ObjectRef = B;
	B->ObjectRef = Dep;
	if (!Scope.Save(*this, DepPackage) || !Scope.Save(*this, Package))
	{
		return false;
	}
	DestroyPackage(Package->GetName());
	DestroyPackage(DepPackage->GetName());
	UPackageTestObject::GetPostLoadLog().Reset();

	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/PostLoadMain"), LOAD_None);
	UPackageTestObject* LoadedA = Loaded ? FindObject<UPackageTestObject>(Loaded, TEXT("A")) : nullptr;
	UPackageTestObject* LoadedB = Loaded ? FindObject<UPackageTestObject>(Loaded, TEXT("B")) : nullptr;
	if (!TestNotNull(TEXT("A"), LoadedA) || !TestNotNull(TEXT("B"), LoadedB))
	{
		return false;
	}
	TestEqual(TEXT("A's PostLoad saw B loaded"), LoadedA->PostLoadSeenRefValue, 20);
	TestEqual(TEXT("B's PostLoad saw the dependency loaded"), LoadedB->PostLoadSeenRefValue, 30);
	TestEqual(TEXT("PostLoad once"), LoadedA->NumPostLoads, 1);
	const TArray<FString>& Log = UPackageTestObject::GetPostLoadLog();
	TestTrue(TEXT("the order: the dependency (loaded first), then the exports in order"),
		Log ==
			TArray<FString>({TEXT("/PackageTest/PostLoadDep.Dep"), TEXT("/PackageTest/PostLoadMain.A"),
				TEXT("/PackageTest/PostLoadMain.B")}));
	TestFalse(TEXT("no RF_NeedPostLoad left"),
		LoadedA->HasAnyFlags(RF_NeedPostLoad) || LoadedB->HasAnyFlags(RF_NeedPostLoad));
	TestFalse(TEXT("no load in progress"), IsLoading());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageSubobjectTest, "System.CoreUObject.Package.Subobjects",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageSubobjectTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	UPackage* Package = Scope.NewPackage(TEXT("Subobjects"));
	UPackageTestOwner* Changed = NewObject<UPackageTestOwner>(Package, TEXT("Changed"), RF_Public);
	UPackageTestOwner* Reset = NewObject<UPackageTestOwner>(Package, TEXT("Reset"), RF_Public);
	UPackageTestOwner* Untouched = NewObject<UPackageTestOwner>(Package, TEXT("Untouched"), RF_Public);
	Changed->Component->Value = 7;
	Changed->Component->Label = TEXT("Changed");
	// Back to the class default, which differs from what the owner's constructor sets: saved against the owner's
	// archetype's subobject, so it is not lost.
	Reset->Component->Value = 0;
	// A runtime inner object, not a default subobject: exported as an inner object of its owner.
	Changed->Extra = NewObject<UPackageTestSubobject>(Changed, TEXT("Extra"));
	Changed->Extra->Value = 99;
	TestEqual(TEXT("the constructor set the subobject up"), Untouched->Component->Value, 5);
	UPackageTestSubobject* const OriginalComponent = Changed->Component;
	TestTrue(TEXT("default subobject"), OriginalComponent->IsDefaultSubobject());
	TestTrue(TEXT("its archetype is the class default object's subobject"),
		OriginalComponent->GetArchetype() == GetDefault<UPackageTestOwner>()->Component);
	if (!Scope.Save(*this, Package))
	{
		return false;
	}
	DestroyPackage(Package->GetName());

	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/Subobjects"), LOAD_None);
	UPackageTestOwner* LoadedChanged = Loaded ? FindObject<UPackageTestOwner>(Loaded, TEXT("Changed")) : nullptr;
	UPackageTestOwner* LoadedReset = Loaded ? FindObject<UPackageTestOwner>(Loaded, TEXT("Reset")) : nullptr;
	UPackageTestOwner* LoadedUntouched = Loaded ? FindObject<UPackageTestOwner>(Loaded, TEXT("Untouched")) : nullptr;
	if (!TestNotNull(TEXT("Changed"), LoadedChanged) || !TestNotNull(TEXT("Reset"), LoadedReset) ||
		!TestNotNull(TEXT("Untouched"), LoadedUntouched))
	{
		return false;
	}
	UPackageTestSubobject* Component = LoadedChanged->Component;
	if (!TestNotNull(TEXT("the owner's constructor rebuilt the subobject (D12)"), Component))
	{
		return false;
	}
	TestTrue(TEXT("the pointer is the rebuilt subobject"),
		Component->GetOuter() == LoadedChanged && Component->GetFName() == FName(TEXT("Component")));
	TestTrue(TEXT("still a default subobject"), Component->IsDefaultSubobject());
	TestEqual(TEXT("its saved value applied"), Component->Value, 7);
	TestEqual(TEXT("its saved string applied"), Component->Label, TEXT("Changed"));
	TestEqual(TEXT("a value back at the class default survives"), LoadedReset->Component->Value, 0);
	TestEqual(TEXT("an untouched subobject keeps the constructor's value"), LoadedUntouched->Component->Value, 5);
	TestEqual(TEXT("and the constructor's string"), LoadedUntouched->Component->Label, TEXT("FromConstructor"));
	if (TestNotNull(TEXT("runtime inner object"), LoadedChanged->Extra))
	{
		TestEqual(TEXT("inner object value"), LoadedChanged->Extra->Value, 99);
		TestTrue(TEXT("inner object outer"), LoadedChanged->Extra->GetOuter() == LoadedChanged);
		TestFalse(TEXT("not a default subobject"), LoadedChanged->Extra->IsDefaultSubobject());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageErrorsTest, "System.CoreUObject.Package.Errors",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageErrorsTest::RunTest(const FString& Parameters)
{
	FPackageTestScope Scope;
	// A missing package is an error (LOAD_NoWarn: a log line; LOAD_Quiet: nothing).
	AddExpectedError(TEXT("/PackageTest/DoesNotExist does not exist"));
	TestNull(TEXT("missing package"), LoadPackage(nullptr, TEXT("/PackageTest/DoesNotExist"), LOAD_None));
	TestNull(TEXT("missing package, quiet"), LoadPackage(nullptr, TEXT("/PackageTest/DoesNotExist"), LOAD_Quiet));
	TestNull(TEXT("missing package, no warning"), LoadPackage(nullptr, TEXT("/PackageTest/DoesNotExist"), LOAD_NoWarn));

	// Damaged data fails cleanly.
	UPackage* Package = Scope.NewPackage(TEXT("Damaged"));
	NewObject<UPackageTestObject>(Package, TEXT("Asset"), RF_Public)->IntValue = 3;
	TArray<uint8> Bytes;
	if (!Scope.Save(*this, Package, &Bytes))
	{
		return false;
	}
	DestroyPackage(Package->GetName());
	TArray<uint8> Truncated = Bytes;
	Truncated.SetNum(Bytes.Num() - 10);
	Scope.Register(TEXT("/PackageTest/Damaged"), Truncated);
	AddExpectedError(TEXT("is truncated"));
	TestNull(TEXT("truncated package"), LoadPackage(nullptr, TEXT("/PackageTest/Damaged"), LOAD_None));
	DestroyPackage(TEXT("/PackageTest/Damaged"));

	TArray<uint8> NotAPackage = Bytes;
	NotAPackage[0] = 'X';
	Scope.Register(TEXT("/PackageTest/Damaged"), NotAPackage);
	AddExpectedError(TEXT("is not a package"));
	TestNull(TEXT("wrong tag"), LoadPackage(nullptr, TEXT("/PackageTest/Damaged"), LOAD_None));
	DestroyPackage(TEXT("/PackageTest/Damaged"));

	TArray<uint8> Newer = Bytes;
	Newer[4] = uint8(VER_LEON_LATEST + 1);
	Scope.Register(TEXT("/PackageTest/Damaged"), Newer);
	AddExpectedError(TEXT("is newer than this engine"));
	TestNull(TEXT("newer version"), LoadPackage(nullptr, TEXT("/PackageTest/Damaged"), LOAD_None));
	DestroyPackage(TEXT("/PackageTest/Damaged"));

	// A name count the file cannot hold (the summary's NameCount, after the tag, the two versions, the header size and
	// the flags) is refused before anything is reserved for it.
	TArray<uint8> HugeCount = Bytes;
	HugeCount[20] = 0xFF;
	HugeCount[21] = 0xFF;
	HugeCount[22] = 0xFF;
	HugeCount[23] = 0x7F;
	Scope.Register(TEXT("/PackageTest/Damaged"), HugeCount);
	AddExpectedError(TEXT("has invalid tables"));
	TestNull(TEXT("a corrupt count"), LoadPackage(nullptr, TEXT("/PackageTest/Damaged"), LOAD_None));
	DestroyPackage(TEXT("/PackageTest/Damaged"));

	// The intact bytes still load.
	Scope.Register(TEXT("/PackageTest/Damaged"), Bytes);
	UPackageTestObject* Asset = LoadObject<UPackageTestObject>(nullptr, TEXT("/PackageTest/Damaged.Asset"));
	TestTrue(TEXT("the intact package loads"), Asset && Asset->IntValue == 3);
	return true;
}

	#if PLATFORM_DESKTOP

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageFileTest, "System.CoreUObject.Package.Files",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageFileTest::RunTest(const FString& Parameters)
{
	// Real files through IFileManager, under <Project>/Intermediate/Tests/CoreUObjectPackage/ (desktop only: the PS2
	// platform file is read-only).
	FPackageTestScope Scope;
	const FString Dir = GetTestContentDir();
	IFileManager::Get().DeleteDirectory(*Dir, false, true);

	UPackage* Asset = Scope.NewPackage(TEXT("Files/Asset"));
	UPackage* Map = Scope.NewPackage(TEXT("Files/Map"));
	UPackageTestObject* AssetObject = NewObject<UPackageTestObject>(Asset, TEXT("Asset"), RF_Public);
	UPackageTestObject* MapObject = NewObject<UPackageTestObject>(Map, TEXT("Map"), RF_Public);
	AssetObject->IntValue = 12;
	MapObject->ObjectRef = AssetObject;
	FillPattern(AssetObject->BulkData, 128, 5);

	const FString AssetFile = FPackageName::LongPackageNameToFilename(
		TEXT("/PackageTest/Files/Asset"), FPackageName::GetAssetPackageExtension());
	const FString MapFile =
		FPackageName::LongPackageNameToFilename(TEXT("/PackageTest/Files/Map"), FPackageName::GetMapPackageExtension());
	TestTrue(TEXT("save .lasset"), UPackage::SavePackage(Asset, nullptr, RF_Public, *AssetFile));
	TestTrue(TEXT("save .lmap"), UPackage::SavePackage(Map, MapObject, RF_NoFlags, *MapFile));
	TestTrue(TEXT("the file exists"), IFileManager::Get().FileExists(*AssetFile));
	FString Found;
	TestTrue(TEXT("DoesPackageExist finds the .lasset"),
		FPackageName::DoesPackageExist(TEXT("/PackageTest/Files/Asset"), nullptr, &Found));
	TestTrue(TEXT("its file"), FPaths::IsSamePath(Found, AssetFile));
	TestTrue(TEXT("DoesPackageExist finds the .lmap"),
		FPackageName::DoesPackageExist(TEXT("/PackageTest/Files/Map"), nullptr, &Found));
	TestEqual(
		TEXT("the file's package"), FPackageName::FilenameToLongPackageName(MapFile), TEXT("/PackageTest/Files/Map"));

	DestroyPackage(TEXT("/PackageTest/Files/Map"));
	DestroyPackage(TEXT("/PackageTest/Files/Asset"));
	UPackage* LoadedMap = LoadPackage(nullptr, *MapFile, LOAD_None);
	if (!TestNotNull(TEXT("loaded from its file"), LoadedMap))
	{
		return false;
	}
	TestTrue(TEXT("a .lmap is a map"), LoadedMap->ContainsMap());
	TestTrue(TEXT("the file name is recorded"), FPaths::IsSamePath(LoadedMap->FileName.ToString(), MapFile));
	UPackageTestObject* LoadedMapObject = FindObject<UPackageTestObject>(LoadedMap, TEXT("Map"));
	UPackageTestObject* LoadedAsset = LoadedMapObject ? Cast<UPackageTestObject>(LoadedMapObject->ObjectRef) : nullptr;
	if (TestNotNull(TEXT("the import loaded its file"), LoadedAsset))
	{
		TestEqual(TEXT("its value"), LoadedAsset->IntValue, 12);
		TestTrue(TEXT("its bulk data"), HasPattern(LoadedAsset->BulkData, 128, 5));
		TestFalse(TEXT("an asset package is not a map"), LoadedAsset->GetOutermost()->ContainsMap());
	}
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}

	#endif // PLATFORM_DESKTOP

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPackageBudgetTest, "System.CoreUObject.Package.Budget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPackageBudgetTest::RunTest(const FString& Parameters)
{
	// The cost of one save / load round trip of NumObjects filled objects, for the platform budgets (PS2:
	// Budgets.md).
	constexpr int32 NumObjects = 50;
	FPackageTestScope Scope;
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
	const uint64 HeapBefore = FMemory::GetUsage().CurrentBytes;
	UPackage* Package = Scope.NewPackage(TEXT("Budget"));
	for (int32 Index = 0; Index < NumObjects; ++Index)
	{
		UPackageTestObject* Object =
			NewObject<UPackageTestObject>(Package, FName(TEXT("Object"), Index + 1), RF_Public);
		FillAllProperties(Object);
		FillPattern(Object->BulkData, 256, uint8(Index));
	}
	const uint64 HeapWithObjects = FMemory::GetUsage().CurrentBytes;

	TArray<uint8> Bytes;
	const double SaveStart = FPlatformTime::Seconds();
	if (!Scope.Save(*this, Package, &Bytes))
	{
		return false;
	}
	const double SaveSeconds = FPlatformTime::Seconds() - SaveStart;
	DestroyPackage(Package->GetName());
	const uint64 HeapAfterDestroy = FMemory::GetUsage().CurrentBytes;

	const double LoadStart = FPlatformTime::Seconds();
	UPackage* Loaded = LoadPackage(nullptr, TEXT("/PackageTest/Budget"), LOAD_None);
	const double LoadSeconds = FPlatformTime::Seconds() - LoadStart;
	const uint64 HeapLoaded = FMemory::GetUsage().CurrentBytes;
	if (!TestNotNull(TEXT("loaded"), Loaded))
	{
		return false;
	}
	TArray<UObject*> LoadedObjects;
	GetObjectsWithOuter(Loaded, LoadedObjects, false);
	TestEqual(TEXT("every object loaded"), LoadedObjects.Num(), NumObjects);

	UE_LOG(LogTemp, Display,
		TEXT("Package budget: %d objects, package %d bytes; save %.3f ms, load %.3f ms; heap %d KB -> %d KB with the "
			 "objects -> %d KB destroyed (the package bytes stay registered) -> %d KB loaded; the load also holds the "
			 "file bytes (%d KB) until it ends"),
		NumObjects, Bytes.Num(), SaveSeconds * 1000.0, LoadSeconds * 1000.0, int32(HeapBefore / 1024),
		int32(HeapWithObjects / 1024), int32(HeapAfterDestroy / 1024), int32(HeapLoaded / 1024),
		int32(Bytes.Num() / 1024));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
