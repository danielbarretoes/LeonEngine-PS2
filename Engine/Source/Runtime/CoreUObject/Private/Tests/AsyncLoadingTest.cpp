#include "Async/AsyncIOSystem.h"
#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Tests/PackageTestTypes.h"
#include "UObject/GarbageCollection.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

#if WITH_DEV_AUTOMATION_TESTS

// LoadPackageAsync (Docs/PLANS/ps2-shipping.md N24): in-memory packages on every platform (TestPAL on the EE), package
// files through the asynchronous reads on the desktop.

namespace
{
	const TCHAR* const AsyncTestRoot = TEXT("/AsyncLoadTest/");

	FString GetAsyncTestContentDir()
	{
		return FPaths::ProjectIntermediateDir() + TEXT("Tests/CoreUObjectAsyncLoad/");
	}

	void DestroyAsyncTestPackage(const FString& PackageName)
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

	/** The mount point for a test; its packages go at the end. */
	class FAsyncLoadTestScope
	{
	public:
		FAsyncLoadTestScope()
		{
			FPackageName::RegisterMountPoint(AsyncTestRoot, GetAsyncTestContentDir());
		}
		~FAsyncLoadTestScope()
		{
			FlushAsyncLoading();
			for (const FString& PackageName : PackageNames)
			{
				FLinkerLoad::UnregisterInMemoryPackage(PackageName);
				DestroyAsyncTestPackage(PackageName);
			}
			FPackageName::UnRegisterMountPoint(AsyncTestRoot, GetAsyncTestContentDir());
		}

		UPackage* NewPackage(const TCHAR* ShortName)
		{
			const FString PackageName = FString(AsyncTestRoot) + ShortName;
			PackageNames.AddUnique(PackageName);
			return CreatePackage(*PackageName);
		}

		/** Saves to memory (bFile false) or to its .lasset (the desktop). */
		bool Save(FAutomationTestBase& Test, UPackage* Package, bool bFile)
		{
			if (bFile)
			{
				const FString File = FPackageName::LongPackageNameToFilename(
					Package->GetName(), FPackageName::GetAssetPackageExtension());
				return Test.TestTrue(*FString::Printf(TEXT("%s saves"), *Package->GetName()),
					UPackage::SavePackage(Package, nullptr, RF_Public, *File));
			}
			TArray<uint8> Bytes;
			if (!Test.TestTrue(*FString::Printf(TEXT("%s saves"), *Package->GetName()),
					UPackage::SaveToMemory(Package, nullptr, RF_Public, Bytes).IsSuccessful()))
			{
				return false;
			}
			FLinkerLoad::RegisterInMemoryPackage(Package->GetName(), Bytes);
			return true;
		}

		TArray<FString> PackageNames;
	};

	/** What a request's delegate was told. */
	struct FAsyncResult
	{
		int32 Calls = 0;
		UPackage* Package = nullptr;
		EAsyncLoadingResult::Type Result = EAsyncLoadingResult::Failed;

		FLoadPackageAsyncDelegate MakeDelegate()
		{
			return FLoadPackageAsyncDelegate::CreateLambda(
				[this](const FName&, UPackage* InPackage, EAsyncLoadingResult::Type InResult)
				{
					++Calls;
					Package = InPackage;
					Result = InResult;
				});
		}
	};

	/** Main -> Middle -> Leaf, and Leaf back to Main; saved, then destroyed as if the process had started again. */
	bool MakeChain(FAutomationTestBase& Test, FAsyncLoadTestScope& Scope, bool bFile)
	{
		UPackage* LeafPackage = Scope.NewPackage(TEXT("Leaf"));
		UPackage* MiddlePackage = Scope.NewPackage(TEXT("Middle"));
		UPackage* MainPackage = Scope.NewPackage(TEXT("Main"));
		UPackageTestObject* Leaf = NewObject<UPackageTestObject>(LeafPackage, TEXT("Leaf"), RF_Public);
		UPackageTestObject* Middle = NewObject<UPackageTestObject>(MiddlePackage, TEXT("Middle"), RF_Public);
		UPackageTestObject* Main = NewObject<UPackageTestObject>(MainPackage, TEXT("Main"), RF_Public);
		Leaf->IntValue = 3;
		Middle->IntValue = 2;
		Main->IntValue = 1;
		Main->ObjectRef = Middle;
		Middle->ObjectRef = Leaf;
		Leaf->ObjectArray.Add(Main);
		const bool bSaved = Scope.Save(Test, LeafPackage, bFile) && Scope.Save(Test, MiddlePackage, bFile) &&
			Scope.Save(Test, MainPackage, bFile);
		for (const FString& PackageName : Scope.PackageNames)
		{
			DestroyAsyncTestPackage(PackageName);
		}
		return bSaved;
	}

	/** The chain came in whole. */
	void CheckChain(FAutomationTestBase& Test, UPackage* Loaded)
	{
		const UPackageTestObject* Main = Loaded ? FindObject<UPackageTestObject>(Loaded, TEXT("Main")) : nullptr;
		const UPackageTestObject* Middle = Main ? Cast<UPackageTestObject>(Main->ObjectRef) : nullptr;
		const UPackageTestObject* Leaf = Middle ? Cast<UPackageTestObject>(Middle->ObjectRef) : nullptr;
		if (Test.TestNotNull(TEXT("Main -> Middle -> Leaf"), Leaf))
		{
			Test.TestEqual(TEXT("Main's value"), Main->IntValue, 1);
			Test.TestEqual(TEXT("Leaf's value"), Leaf->IntValue, 3);
			Test.TestTrue(TEXT("Leaf -> Main"), Leaf->ObjectArray.Num() == 1 && Leaf->ObjectArray[0] == Main);
			Test.TestTrue(TEXT("Fully loaded"), Loaded->IsFullyLoaded());
		}
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncLoadingInMemoryTest, "System.CoreUObject.AsyncLoading.Delegates",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAsyncLoadingInMemoryTest::RunTest(const FString& Parameters)
{
	// A package and its imports (a circular one too) load in the background: nothing happens in LoadPackageAsync
	// itself, the delegate runs once from ProcessAsyncLoading or a flush with the package; a loaded package and a
	// missing one complete at the next ProcessAsyncLoading (UE).
	FAsyncLoadTestScope Scope;
	if (!MakeChain(*this, Scope, false))
	{
		return false;
	}
	FAsyncResult Main;
	const int32 Id = LoadPackageAsync(TEXT("/AsyncLoadTest/Main"), Main.MakeDelegate());
	TestTrue("A request id", Id > 0);
	TestEqual("Not in LoadPackageAsync", Main.Calls, 0);
	TestTrue("Loading", IsAsyncLoading() && GetNumAsyncPackages() >= 1);
	TestNull("Not loaded yet", FindPackage(nullptr, TEXT("/AsyncLoadTest/Main")));
	FlushAsyncLoading(Id);
	TestEqual("Called once", Main.Calls, 1);
	TestTrue("Succeeded", Main.Result == EAsyncLoadingResult::Succeeded);
	TestTrue("With the package", Main.Package == FindPackage(nullptr, TEXT("/AsyncLoadTest/Main")));
	CheckChain(*this, Main.Package);
	TestFalse("Nothing left", IsAsyncLoading());

	FAsyncResult Again;
	FAsyncResult Missing;
	(void)LoadPackageAsync(TEXT("/AsyncLoadTest/Main"), Again.MakeDelegate());
	(void)LoadPackageAsync(TEXT("/AsyncLoadTest/NotThere"), Missing.MakeDelegate());
	TestEqual("A loaded package waits for the next ProcessAsyncLoading", Again.Calls, 0);
	TestTrue("Complete", ProcessAsyncLoading(false, false, 0.0f) == EAsyncPackageState::Complete);
	TestTrue("A loaded package succeeds",
		Again.Calls == 1 && Again.Result == EAsyncLoadingResult::Succeeded && Again.Package == Main.Package);
	TestTrue("A missing one fails",
		Missing.Calls == 1 && Missing.Result == EAsyncLoadingResult::Failed && Missing.Package == nullptr);

	// Cancelled: its delegate says so and nothing loads.
	DestroyAsyncTestPackage(TEXT("/AsyncLoadTest/Main"));
	DestroyAsyncTestPackage(TEXT("/AsyncLoadTest/Middle"));
	DestroyAsyncTestPackage(TEXT("/AsyncLoadTest/Leaf"));
	FAsyncResult Cancelled;
	(void)LoadPackageAsync(TEXT("/AsyncLoadTest/Main"), Cancelled.MakeDelegate());
	CancelAsyncLoading();
	TestTrue("Cancelled", Cancelled.Calls == 1 && Cancelled.Result == EAsyncLoadingResult::Canceled);
	TestNull("Nothing loaded", FindPackage(nullptr, TEXT("/AsyncLoadTest/Main")));
	TestFalse("Nothing left", IsAsyncLoading());
	return true;
}

	#if PLATFORM_DESKTOP

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncLoadingFilesTest, "System.CoreUObject.AsyncLoading.Files",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAsyncLoadingFilesTest::RunTest(const FString& Parameters)
{
	// Package files: their bytes come through the asynchronous reads (Win64 reads them in ProcessAsyncLoading, the
	// PS2's IO thread in the background), the imports' too, and one ProcessAsyncLoading loads the lot. A LoadPackage
	// of a package on its way takes the bytes read for it: each file is read once.
	FAsyncLoadTestScope Scope;
	IFileManager::Get().DeleteDirectory(*GetAsyncTestContentDir(), false, true);
	if (!MakeChain(*this, Scope, true))
	{
		return false;
	}
	FAsyncResult Main;
	const uint64 BytesBefore = FAsyncIOSystem::Get().GetBytesRead();
	(void)LoadPackageAsync(TEXT("/AsyncLoadTest/Main"), Main.MakeDelegate());
	TestTrue("One call loads the package and its imports",
		ProcessAsyncLoading(false, false, 0.0f) == EAsyncPackageState::Complete);
	TestTrue("Succeeded", Main.Calls == 1 && Main.Result == EAsyncLoadingResult::Succeeded);
	CheckChain(*this, Main.Package);
	int64 FilesSize = 0;
	for (const TCHAR* Name : {TEXT("Main"), TEXT("Middle"), TEXT("Leaf")})
	{
		FilesSize += IFileManager::Get().FileSize(*FPackageName::LongPackageNameToFilename(
			FString(AsyncTestRoot) + Name, FPackageName::GetAssetPackageExtension()));
	}
	TestEqual(
		"Every file read once, asynchronously", int64(FAsyncIOSystem::Get().GetBytesRead() - BytesBefore), FilesSize);

	// The flush: a synchronous load of a package on its way uses its bytes.
	DestroyAsyncTestPackage(TEXT("/AsyncLoadTest/Main"));
	DestroyAsyncTestPackage(TEXT("/AsyncLoadTest/Middle"));
	DestroyAsyncTestPackage(TEXT("/AsyncLoadTest/Leaf"));
	FAsyncResult Leaf;
	const uint64 BytesBeforeLeaf = FAsyncIOSystem::Get().GetBytesRead();
	(void)LoadPackageAsync(TEXT("/AsyncLoadTest/Leaf"), Leaf.MakeDelegate());
	UPackage* SyncLeaf = LoadPackage(nullptr, TEXT("/AsyncLoadTest/Leaf"), LOAD_None);
	TestNotNull("LoadPackage of a package on its way", SyncLeaf);
	TestEqual("The delegate has not run yet", Leaf.Calls, 0);
	(void)ProcessAsyncLoading(false, false, 0.0f);
	TestTrue("It runs with the package LoadPackage made",
		Leaf.Calls == 1 && Leaf.Package == SyncLeaf && Leaf.Result == EAsyncLoadingResult::Succeeded);
	const int64 LeafSize = IFileManager::Get().FileSize(*FPackageName::LongPackageNameToFilename(
		TEXT("/AsyncLoadTest/Leaf"), FPackageName::GetAssetPackageExtension()));
	TestTrue("The leaf's file was read once, asynchronously",
		int64(FAsyncIOSystem::Get().GetBytesRead() - BytesBeforeLeaf) == LeafSize);
	IFileManager::Get().DeleteDirectory(*GetAsyncTestContentDir(), false, true);
	return true;
}

	#endif // PLATFORM_DESKTOP

#endif // WITH_DEV_AUTOMATION_TESTS
