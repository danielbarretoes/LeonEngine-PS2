#pragma once

// The result of UPackage::Save (UE: UObject/SavePackage.h). The save itself is UPackage::Save / SavePackage.

#include "CoreMinimal.h"

/** How a save ended (UE: ESavePackageResult; the subset Leon uses). */
enum class ESavePackageResult
{
	/** The package was written. */
	Success,
	/** Nothing was written (UE). */
	Canceled,
	/** The save failed: see the log. */
	Error,
};

/** What UPackage::Save reports (UE: FSavePackageResultStruct). */
struct FSavePackageResultStruct
{
	ESavePackageResult Result = ESavePackageResult::Error;

	/** Size of the package in bytes. */
	int64 TotalFileSize = 0;

	FSavePackageResultStruct() = default;

	FSavePackageResultStruct(ESavePackageResult InResult, int64 InTotalFileSize = 0)
		: Result(InResult)
		, TotalFileSize(InTotalFileSize)
	{
	}

	FORCEINLINE bool IsSuccessful() const
	{
		return Result == ESavePackageResult::Success;
	}

	FORCEINLINE bool operator==(ESavePackageResult InResult) const
	{
		return Result == InResult;
	}

	FORCEINLINE bool operator!=(ESavePackageResult InResult) const
	{
		return Result != InResult;
	}
};
