#pragma once

#include "CoreMinimal.h"

class UObject;

/** How a reimport ended (UE: EReimportResult). */
namespace EReimportResult
{
	enum Type
	{
		Failed,
		Succeeded,
		Cancelled,
	};
} // namespace EReimportResult

/**
 * Something that can reimport assets from their source files (UE: FReimportHandler, EditorReimportHandler.h). A handler
 * registers itself with FReimportManager when it is constructed and leaves when it is destroyed; Leon's factories are
 * handlers (their class default objects answer), where UE has separate UReimport*Factory classes.
 */
class LEONED_API FReimportHandler
{
public:
	FReimportHandler();
	virtual ~FReimportHandler();

	FReimportHandler(const FReimportHandler&) = delete;
	FReimportHandler& operator=(const FReimportHandler&) = delete;

	/** True when the handler can reimport Obj; OutFilenames gets its source files (absolute) (UE: CanReimport). */
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) = 0;

	/** Points Obj at other source files (UE: SetReimportPaths). */
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) = 0;

	/** Reads Obj's source again into Obj (UE: Reimport). */
	virtual EReimportResult::Type Reimport(UObject* Obj) = 0;

	/**
	 * The assets the last Reimport made or changed besides the reimported one (a new material of a mesh): their
	 * packages must be saved too (Leon).
	 */
	virtual void GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
	{
		(void)OutObjects;
	}

	/** Handlers with a higher priority are asked first (UE: GetPriority). */
	virtual int32 GetPriority() const
	{
		return 0;
	}
};

/**
 * The registered reimport handlers (UE: FReimportManager). Reimport asks them in priority order and lets the first
 * that can reimport the object do it. Leon keeps only the automated path: no dialogs, no new-file prompts, no
 * notifications.
 */
class LEONED_API FReimportManager
{
public:
	/** The manager (UE: Instance). It lives until the process ends, so handlers can leave it at any time. */
	static FReimportManager* Instance();

	void RegisterHandler(FReimportHandler& InHandler);
	void UnregisterHandler(FReimportHandler& InHandler);

	/** True when a handler can reimport Obj; its source files go to ReimportSourceFilenames (UE: CanReimport). */
	bool CanReimport(UObject* Obj, TArray<FString>* ReimportSourceFilenames = nullptr) const;

	/**
	 * Reimports Obj with the first handler that can (UE: Reimport, automated). False when none can, or when the
	 * source file is missing or the import fails (logged). The caller saves Obj's package, and the packages of the
	 * objects OutAdditionalObjects gets (GetAdditionalReimportedObjects).
	 */
	bool Reimport(UObject* Obj, TArray<UObject*>* OutAdditionalObjects = nullptr);

private:
	FReimportManager() = default;

	/** The handlers by descending priority, then registration order. */
	TArray<FReimportHandler*> Handlers;
};
