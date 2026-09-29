#include "Async/AsyncFileHandle.h"
#include "Async/AsyncIOSystem.h"
#include "HAL/FileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformTime.h"
#include "HAL/UnrealMemory.h"
#include "Misc/PackageName.h"
#include "Serialization/AsyncLoadingPrivate.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

// LoadPackageAsync (UE: Serialization/AsyncLoading.cpp, FAsyncLoadingThread and FAsyncPackage; Docs/PLANS/
// ps2-shipping.md N24). A package's file, and the files of the packages it imports, are read by the IO thread
// (FAsyncIOSystem); the game thread serializes a package once its bytes and its imports' are in memory, through the
// synchronous loader (LoadPackage finds the bytes with TakeAsyncPackageBytes), so the objects, their order and their
// PostLoad are the synchronous ones. Only the reads are asynchronous: Leon's packages are small, the disc is slow.

DEFINE_LOG_CATEGORY_STATIC(LogStreaming, Log, All);

namespace
{
	/** A package on its way (UE: FAsyncPackage). */
	struct FAsyncPackage
	{
		enum class EState : uint8
		{
			/** Waiting for room: the bytes in flight are at MaxBytesInFlight. */
			Queued,
			/** Its file's bytes are being read. */
			Reading,
			/** Its bytes and its tables are in: it loads once its imports are in too. */
			Ready,
			/** Loaded (by this loader or a synchronous load). */
			Loaded,
			Failed,
			Canceled,
		};

		FName Name;
		FString Filename;
		EAsyncIOPriorityAndFlags Priority = AIOP_Normal;
		EState State = EState::Reading;
		/** The requests (their ids and delegates) waiting for it; none for an import no one asked for. */
		TArray<TPair<int32, FLoadPackageAsyncDelegate>> Requests;
		IAsyncReadFileHandle* Handle = nullptr;
		IAsyncReadRequest* Read = nullptr;
		/** The file's bytes until the loader takes them (FMemory, LoadMapMisc). */
		uint8* Bytes = nullptr;
		int64 Size = 0;
		/** Registered with FLinkerLoad::RegisterInMemoryPackage: nothing to read. */
		bool bInMemory = false;
		/** Another package on its way imports it: it is read before the queued requests (they wait on it). */
		bool bImport = false;
		/** The packages its imports name. */
		TArray<FName> Imports;
	};

	struct FAsyncLoader
	{
		/**
		 * The package bytes read ahead at most (Leon): the rest waits (Queued), so a preload of many packages holds its
		 * bytes a few at a time (LoadMapMisc's budget on the PS2); an import passes when nothing is being read, so a
		 * package that waits for it always gets it.
		 */
		static constexpr int64 MaxBytesInFlight = 640 * 1024;

		/** The packages on their way, in request order. */
		TArray<FAsyncPackage*> Packages;
		int32 NextRequestId = 1;
		/** The game thread's cost (for the flush's log): packages serialized and the cycles in them. */
		int32 NumSerialized = 0;
		uint64 SerializeCycles = 0;

		~FAsyncLoader()
		{
			for (FAsyncPackage* Package : Packages)
			{
				ReleaseRead(*Package);
				FMemory::Free(Package->Bytes);
				delete Package;
			}
		}

		FAsyncPackage* Find(FName Name) const
		{
			for (FAsyncPackage* Package : Packages)
			{
				if (Package->Name == Name)
				{
					return Package;
				}
			}
			return nullptr;
		}

		static void ReleaseRead(FAsyncPackage& Package)
		{
			if (Package.Read != nullptr)
			{
				// Deleting a request the IO thread reads waits for it.
				delete Package.Read;
				Package.Read = nullptr;
			}
			delete Package.Handle;
			Package.Handle = nullptr;
		}

		static bool IsLoaded(FName Name)
		{
			const UPackage* Package = FindPackage(nullptr, *Name.ToString());
			return Package != nullptr && Package->IsFullyLoaded() && Package->LinkerLoad == nullptr;
		}

		/** A package to load: known, read or found loaded. */
		FAsyncPackage* Add(FName Name, EAsyncIOPriorityAndFlags Priority, bool bImport = false)
		{
			FAsyncPackage* Package = new FAsyncPackage();
			Package->Name = Name;
			Package->Priority = Priority;
			Packages.Add(Package);
			const FString NameString = Name.ToString();
			if (FPackageName::IsScriptPackage(NameString) || IsLoaded(Name))
			{
				Package->State = FAsyncPackage::EState::Loaded;
				return Package;
			}
			if (const TArray<uint8>* InMemory = FLinkerLoad::FindInMemoryPackage(NameString))
			{
				Package->bInMemory = true;
				ParseImports(*Package, InMemory->GetData(), InMemory->Num());
				return Package;
			}
			if (!FPackageName::DoesPackageExist(NameString, nullptr, &Package->Filename))
			{
				UE_LOG(LogStreaming, Warning, "LoadPackageAsync: %s does not exist", *NameString);
				Package->State = FAsyncPackage::EState::Failed;
				return Package;
			}
			const int64 Size = IFileManager::Get().FileSize(*Package->Filename);
			if (Size <= 0)
			{
				UE_LOG(LogStreaming, Warning, "LoadPackageAsync: %s cannot be read", *Package->Filename);
				Package->State = FAsyncPackage::EState::Failed;
				return Package;
			}
			Package->Size = Size;
			Package->bImport = bImport;
			Package->State = FAsyncPackage::EState::Queued;
			(void)IssueReads();
			return Package;
		}

		/** The bytes the packages hold (read or being read, not yet taken by the loader). */
		[[nodiscard]] int64 GetBytesHeld() const
		{
			int64 Held = 0;
			for (const FAsyncPackage* Package : Packages)
			{
				Held += Package->Bytes != nullptr ? Package->Size : 0;
			}
			return Held;
		}

		[[nodiscard]] bool IsAnyReading() const
		{
			for (const FAsyncPackage* Package : Packages)
			{
				if (Package->State == FAsyncPackage::EState::Reading)
				{
					return true;
				}
			}
			return false;
		}

		/** Starts a queued package's read. */
		static void StartRead(FAsyncPackage& Package)
		{
			{
				LLM_SCOPE(ELLMTag::LoadMapMisc);
				Package.Bytes = static_cast<uint8*>(FMemory::Malloc(SIZE_T(Package.Size), 16));
			}
			Package.State = FAsyncPackage::EState::Reading;
			Package.Handle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*Package.Filename);
			Package.Read = Package.Handle->ReadRequest(0, Package.Size, Package.Priority, nullptr, Package.Bytes);
		}

		/**
		 * Starts the queued reads there is room for, the imports first (what a read package waits for); an import
		 * starts anyway when nothing is being read. True when one started.
		 */
		bool IssueReads()
		{
			bool bIssued = false;
			int64 Held = GetBytesHeld();
			for (const bool bImports : {true, false})
			{
				for (FAsyncPackage* Package : Packages)
				{
					if (Package->State != FAsyncPackage::EState::Queued || Package->bImport != bImports)
					{
						continue;
					}
					if (Held + Package->Size > MaxBytesInFlight && Held > 0 && !(bImports && !IsAnyReading()))
					{
						continue;
					}
					if (IsLoaded(Package->Name))
					{
						Package->State = FAsyncPackage::EState::Loaded;
						continue;
					}
					StartRead(*Package);
					Held += Package->Size;
					bIssued = true;
				}
			}
			return bIssued;
		}

		/** Reads the tables' imports and asks for the packages they name (Ready, or Failed). */
		void ParseImports(FAsyncPackage& Package, const uint8* Bytes, int64 Size)
		{
			const FString& Label = Package.bInMemory ? Package.Name.ToString() : Package.Filename;
			if (!FLinkerLoad::GetImportedPackageNames(*Label, Bytes, Size, Package.Imports))
			{
				Package.State = FAsyncPackage::EState::Failed;
				return;
			}
			Package.State = FAsyncPackage::EState::Ready;
			for (const FName& Import : Package.Imports)
			{
				FAsyncPackage* Existing = Find(Import);
				if (Existing == nullptr && !IsLoaded(Import))
				{
					(void)Add(Import, Package.Priority, true);
				}
				else if (Existing != nullptr)
				{
					Existing->bImport = true;
				}
			}
		}

		/** Moves the packages whose reads ended on; true when one did. */
		bool UpdateReads()
		{
			bool bProgress = false;
			// Add appends while this walks: by index.
			for (int32 Index = 0; Index < Packages.Num(); ++Index)
			{
				FAsyncPackage& Package = *Packages[Index];
				if (Package.State != FAsyncPackage::EState::Reading || Package.Read == nullptr ||
					!Package.Read->PollCompletion())
				{
					continue;
				}
				const bool bRead = Package.Read->GetReadResults() != nullptr;
				ReleaseRead(Package);
				bProgress = true;
				if (!bRead)
				{
					UE_LOG(LogStreaming, Warning, "LoadPackageAsync: %s could not be read", *Package.Filename);
					Package.State = FAsyncPackage::EState::Failed;
					continue;
				}
				if (IsLoaded(Package.Name))
				{
					Package.State = FAsyncPackage::EState::Loaded;
					continue;
				}
				ParseImports(Package, Package.Bytes, Package.Size);
			}
			return bProgress;
		}

		/** Whether the package and every package it imports, as far as this loader knows them, are in memory. */
		bool IsClosureReady(const FAsyncPackage& Package, TArray<const FAsyncPackage*>& Visited) const
		{
			if (Visited.Contains(&Package))
			{
				return true;
			}
			Visited.Add(&Package);
			if (Package.State == FAsyncPackage::EState::Reading || Package.State == FAsyncPackage::EState::Queued)
			{
				return false;
			}
			for (const FName& Import : Package.Imports)
			{
				const FAsyncPackage* ImportPackage = Find(Import);
				if (ImportPackage != nullptr && !IsClosureReady(*ImportPackage, Visited))
				{
					return false;
				}
			}
			return true;
		}

		/** Loads one package (and what it imports) through LoadPackage; marks every package that is now loaded. */
		void LoadNow(FAsyncPackage& Package)
		{
			const uint64 StartCycles = FPlatformTime::Cycles64();
			const UPackage* Loaded = LoadPackage(nullptr, *Package.Name.ToString(), LOAD_None);
			SerializeCycles += FPlatformTime::Cycles64() - StartCycles;
			++NumSerialized;
			if (Loaded == nullptr)
			{
				Package.State = FAsyncPackage::EState::Failed;
			}
			for (FAsyncPackage* Other : Packages)
			{
				if ((Other->State == FAsyncPackage::EState::Ready || Other->State == FAsyncPackage::EState::Reading ||
						Other->State == FAsyncPackage::EState::Queued) &&
					IsLoaded(Other->Name))
				{
					ReleaseRead(*Other);
					Other->State = FAsyncPackage::EState::Loaded;
				}
			}
		}

		/** Loads the packages that can load; stops at the deadline (0: none). True when one loaded. */
		bool LoadReady(uint64 StartCycles, uint64 LimitMicroseconds, bool& bOutTimedOut)
		{
			bool bProgress = false;
			for (int32 Index = 0; Index < Packages.Num(); ++Index)
			{
				FAsyncPackage& Package = *Packages[Index];
				TArray<const FAsyncPackage*> Visited;
				if (Package.State != FAsyncPackage::EState::Ready || !IsClosureReady(Package, Visited))
				{
					continue;
				}
				LoadNow(Package);
				bProgress = true;
				if (LimitMicroseconds > 0 &&
					FPlatformTime::CyclesToMicroseconds(FPlatformTime::Cycles64() - StartCycles) >= LimitMicroseconds)
				{
					bOutTimedOut = true;
					return true;
				}
			}
			return bProgress;
		}

		/** Takes the finished packages out and calls their requests' delegates, in request order. */
		void CompleteFinished()
		{
			TArray<FAsyncPackage*> Finished;
			for (int32 Index = 0; Index < Packages.Num();)
			{
				FAsyncPackage* Package = Packages[Index];
				const bool bFinished = Package->State == FAsyncPackage::EState::Loaded ||
					Package->State == FAsyncPackage::EState::Failed ||
					Package->State == FAsyncPackage::EState::Canceled;
				// A finished import goes too: an importer's closure only waits for what is still on its way, and its
				// own load reports a missing import.
				if (bFinished)
				{
					Packages.RemoveAt(Index);
					Finished.Add(Package);
					continue;
				}
				++Index;
			}
			// The delegates may ask for more packages: the list is already consistent.
			for (FAsyncPackage* Package : Finished)
			{
				ReleaseRead(*Package);
				FMemory::Free(Package->Bytes);
				Package->Bytes = nullptr;
				UPackage* Loaded = Package->State == FAsyncPackage::EState::Loaded
					? FindPackage(nullptr, *Package->Name.ToString())
					: nullptr;
				const EAsyncLoadingResult::Type Result = Package->State == FAsyncPackage::EState::Canceled
					? EAsyncLoadingResult::Canceled
					: Loaded != nullptr ? EAsyncLoadingResult::Succeeded
										: EAsyncLoadingResult::Failed;
				if (Package->Requests.Num() > 0)
				{
					UE_LOG(LogStreaming, Log, "LoadPackageAsync: %s %s", *Package->Name.ToString(),
						Result == EAsyncLoadingResult::Succeeded      ? TEXT("loaded")
							: Result == EAsyncLoadingResult::Canceled ? TEXT("canceled")
																	  : TEXT("failed"));
				}
				for (TPair<int32, FLoadPackageAsyncDelegate>& Request : Package->Requests)
				{
					(void)Request.Value.ExecuteIfBound(Package->Name, Loaded, Result);
				}
				delete Package;
			}
		}

		[[nodiscard]] bool HasRequest(int32 RequestId) const
		{
			for (const FAsyncPackage* Package : Packages)
			{
				for (const TPair<int32, FLoadPackageAsyncDelegate>& Request : Package->Requests)
				{
					if (RequestId == INDEX_NONE || Request.Key == RequestId)
					{
						return true;
					}
				}
			}
			return false;
		}
	};

	FAsyncLoader& GetAsyncLoader()
	{
		static FAsyncLoader Loader;
		return Loader;
	}

	EAsyncIOPriorityAndFlags ToIOPriority(int32 PackagePriority)
	{
		return PackagePriority > 0 ? AIOP_High : PackagePriority < 0 ? AIOP_Low : AIOP_Normal;
	}
} // namespace

bool TakeAsyncPackageBytes(const TCHAR* Filename, uint8*& OutBytes, int64& OutSize)
{
	FAsyncLoader& Loader = GetAsyncLoader();
	for (FAsyncPackage* Package : Loader.Packages)
	{
		if (Package->bInMemory || Package->Filename != Filename)
		{
			continue;
		}
		if (Package->Bytes == nullptr && Package->State == FAsyncPackage::EState::Queued)
		{
			// Not started (MaxBytesInFlight): its read starts now, ahead of the others, and the load waits for it (the
			// IO thread's queue keeps the disc's reads in one sorted stream).
			Package->Priority = AIOP_CriticalPath;
			FAsyncLoader::StartRead(*Package);
		}
		if (Package->Bytes == nullptr)
		{
			continue;
		}
		if (Package->State == FAsyncPackage::EState::Reading)
		{
			// A synchronous load of a package on its way waits for its read (UE: the flush), which goes first.
			const uint64 WaitStart = FPlatformTime::Cycles64();
			if (Package->Read != nullptr)
			{
				FAsyncIOSystem::Get().RaisePriority(Package->Read, AIOP_CriticalPath);
			}
			if (Package->Read != nullptr &&
				(!Package->Read->WaitCompletion() || Package->Read->GetReadResults() == nullptr))
			{
				FAsyncLoader::ReleaseRead(*Package);
				return false;
			}
			const uint64 WaitedMs = FPlatformTime::CyclesToMicroseconds(FPlatformTime::Cycles64() - WaitStart) / 1000;
			if (WaitedMs > 0)
			{
				UE_LOG(LogStreaming, Display, "LoadPackage of %s waited %llu ms for its asynchronous read",
					*Package->Name.ToString(), WaitedMs);
			}
			FAsyncLoader::ReleaseRead(*Package);
			Package->State = FAsyncPackage::EState::Ready;
		}
		if (Package->State != FAsyncPackage::EState::Ready)
		{
			return false;
		}
		OutBytes = Package->Bytes;
		OutSize = Package->Size;
		Package->Bytes = nullptr;
		return true;
	}
	return false;
}

int32 LoadPackageAsync(const FString& InName, FLoadPackageAsyncDelegate InCompletionDelegate, int32 InPackagePriority)
{
	FAsyncLoader& Loader = GetAsyncLoader();
	const int32 RequestId = Loader.NextRequestId++;
	FString PackageName = InName;
	if (!FPackageName::IsValidLongPackageName(PackageName, true) && !FLinkerLoad::FindInMemoryPackage(PackageName) &&
		!FPackageName::TryConvertFilenameToLongPackageName(InName, PackageName))
	{
		UE_LOG(
			LogStreaming, Warning, "LoadPackageAsync: '%s' is neither a long package name nor a package file", *InName);
		PackageName = InName;
	}
	const FName Name(*PackageName);
	FAsyncPackage* Package = Loader.Find(Name);
	if (Package == nullptr)
	{
		Package = Loader.Add(Name, ToIOPriority(InPackagePriority));
	}
	Package->Requests.Add(TPair<int32, FLoadPackageAsyncDelegate>(RequestId, MoveTemp(InCompletionDelegate)));
	return RequestId;
}

EAsyncPackageState::Type ProcessAsyncLoading(bool bUseTimeLimit, bool /*bUseFullTimeLimit*/, float TimeLimit)
{
	FAsyncLoader& Loader = GetAsyncLoader();
	const uint64 StartCycles = FPlatformTime::Cycles64();
	const uint64 LimitMicroseconds = bUseTimeLimit && TimeLimit > 0.0f ? uint64(TimeLimit * 1000000.0f) : 0;
	bool bTimedOut = false;
	bool bProgress = true;
	while (bProgress && !bTimedOut)
	{
		// Without an IO thread (Win64) the reads the last pass asked for are made here, so a package and its imports
		// load in one call.
		FAsyncIOSystem::Get().Tick();
		bProgress = Loader.UpdateReads();
		bProgress |= Loader.IssueReads();
		bProgress |= Loader.LoadReady(StartCycles, LimitMicroseconds, bTimedOut);
		Loader.CompleteFinished();
	}
	if (Loader.Packages.Num() == 0)
	{
		return EAsyncPackageState::Complete;
	}
	return bTimedOut ? EAsyncPackageState::TimeOut : EAsyncPackageState::PendingImports;
}

void FlushAsyncLoading(int32 PackageID)
{
	FAsyncLoader& Loader = GetAsyncLoader();
	if (!Loader.HasRequest(PackageID))
	{
		return;
	}
	FAsyncIOSystem& IO = FAsyncIOSystem::Get();
	const uint64 StartCycles = FPlatformTime::Cycles64();
	const uint64 StartBytes = IO.GetBytesRead();
	const uint64 StartReadCycles = IO.GetReadCycles();
	const uint32 StartReads = IO.GetNumReads();
	const uint32 StartChunks = IO.GetNumChunks();
	const uint64 StartSerializeCycles = Loader.SerializeCycles;
	const int32 StartSerialized = Loader.NumSerialized;
	uint64 WaitCycles = 0;
	while (Loader.HasRequest(PackageID))
	{
		const int32 Before = Loader.Packages.Num();
		(void)ProcessAsyncLoading(false, false, 0.0f);
		if (!Loader.HasRequest(PackageID))
		{
			break;
		}
		if (IO.GetNumPendingRequests() > 0)
		{
			const uint64 WaitStart = FPlatformTime::Cycles64();
			IO.WaitForAnyCompletion();
			WaitCycles += FPlatformTime::Cycles64() - WaitStart;
		}
		else if (Loader.Packages.Num() == Before)
		{
			// Nothing to wait for and nothing moved: load what is left as the synchronous loader would.
			for (int32 Index = 0; Index < Loader.Packages.Num(); ++Index)
			{
				FAsyncPackage& Package = *Loader.Packages[Index];
				if (Package.State == FAsyncPackage::EState::Ready)
				{
					Loader.LoadNow(Package);
				}
			}
			Loader.CompleteFinished();
		}
	}
	const auto Ms = [](uint64 Cycles) { return FPlatformTime::CyclesToMicroseconds(Cycles) / 1000; };
	UE_LOG(LogStreaming, Display,
		"FlushAsyncLoading: %d package(s) serialized in %llu ms; %u read(s), %u chunk(s), %llu KB, the reads %llu ms; "
		"waited %llu ms; %llu ms in all",
		Loader.NumSerialized - StartSerialized, Ms(Loader.SerializeCycles - StartSerializeCycles),
		IO.GetNumReads() - StartReads, IO.GetNumChunks() - StartChunks, (IO.GetBytesRead() - StartBytes) / 1024,
		Ms(IO.GetReadCycles() - StartReadCycles), Ms(WaitCycles), Ms(FPlatformTime::Cycles64() - StartCycles));
}

void CancelAsyncLoading()
{
	FAsyncLoader& Loader = GetAsyncLoader();
	for (FAsyncPackage* Package : Loader.Packages)
	{
		if (Package->Read != nullptr)
		{
			Package->Read->Cancel();
		}
		FAsyncLoader::ReleaseRead(*Package);
		Package->State = FAsyncPackage::EState::Canceled;
	}
	Loader.CompleteFinished();
}

bool IsAsyncLoading()
{
	return GetAsyncLoader().Packages.Num() > 0;
}

int32 GetNumAsyncPackages()
{
	return GetAsyncLoader().Packages.Num();
}
