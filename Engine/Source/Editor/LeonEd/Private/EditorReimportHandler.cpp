#include "EditorReimportHandler.h"

#include "LeonEdLog.h"
#include "UObject/Object.h"

FReimportHandler::FReimportHandler()
{
	FReimportManager::Instance()->RegisterHandler(*this);
}

FReimportHandler::~FReimportHandler()
{
	FReimportManager::Instance()->UnregisterHandler(*this);
}

FReimportManager* FReimportManager::Instance()
{
	// Never destroyed: class default objects (the handlers) may be destroyed after static destructors run.
	static FReimportManager* Manager = new FReimportManager();
	return Manager;
}

void FReimportManager::RegisterHandler(FReimportHandler& InHandler)
{
	// Registration happens in FReimportHandler's constructor, before the subclass's GetPriority is callable: keep the
	// registration order here and sort by priority when asking.
	Handlers.AddUnique(&InHandler);
}

void FReimportManager::UnregisterHandler(FReimportHandler& InHandler)
{
	Handlers.Remove(&InHandler);
}

bool FReimportManager::CanReimport(UObject* Obj, TArray<FString>* ReimportSourceFilenames) const
{
	for (FReimportHandler* Handler : Handlers)
	{
		TArray<FString> Filenames;
		if (Handler->CanReimport(Obj, Filenames))
		{
			if (ReimportSourceFilenames != nullptr)
			{
				*ReimportSourceFilenames = Filenames;
			}
			return true;
		}
	}
	return false;
}

bool FReimportManager::Reimport(UObject* Obj, TArray<UObject*>* OutAdditionalObjects)
{
	if (Obj == nullptr)
	{
		return false;
	}
	// The highest priority first, then registration order (a stable sort of a copy: a reimport may create factories,
	// which register themselves).
	TArray<FReimportHandler*> Sorted = Handlers;
	Sorted.StableSort(
		[](const FReimportHandler& A, const FReimportHandler& B) { return A.GetPriority() > B.GetPriority(); });
	for (FReimportHandler* Handler : Sorted)
	{
		TArray<FString> Filenames;
		if (!Handler->CanReimport(Obj, Filenames))
		{
			continue;
		}
		const EReimportResult::Type Result = Handler->Reimport(Obj);
		if (Result == EReimportResult::Succeeded && OutAdditionalObjects != nullptr)
		{
			Handler->GetAdditionalReimportedObjects(*OutAdditionalObjects);
		}
		return Result == EReimportResult::Succeeded;
	}
	UE_LOG(LogLeonEd, Warning, "No reimport handler for %s", *Obj->GetPathName());
	return false;
}
