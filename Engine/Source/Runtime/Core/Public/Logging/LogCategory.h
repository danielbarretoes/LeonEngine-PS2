#pragma once

#include "CoreTypes.h"
#include "Logging/LogVerbosity.h"
#include "UObject/NameTypes.h"

/**
 * Runtime state of a log category (UE: FLogCategoryBase). Every category registers itself so config / command-line
 * settings can change its verbosity later (FindCategory, SetVerbosity).
 */
struct CORE_API FLogCategoryBase
{
	FLogCategoryBase(
		const TCHAR* CategoryName, ELogVerbosity::Type InDefaultVerbosity, ELogVerbosity::Type InCompileTimeVerbosity);
	~FLogCategoryBase();

	FLogCategoryBase(const FLogCategoryBase&) = delete;
	FLogCategoryBase& operator=(const FLogCategoryBase&) = delete;

	/** True when a message of this verbosity is filtered out at run time. */
	FORCEINLINE bool IsSuppressed(ELogVerbosity::Type VerbosityLevel) const
	{
		return !((VerbosityLevel & ELogVerbosity::VerbosityMask) <= Verbosity);
	}

	FORCEINLINE const FName& GetCategoryName() const
	{
		return CategoryName;
	}

	FORCEINLINE ELogVerbosity::Type GetVerbosity() const
	{
		return (ELogVerbosity::Type)Verbosity;
	}

	/** Sets the run-time verbosity (clamped to the compile-time verbosity). */
	void SetVerbosity(ELogVerbosity::Type Verbosity);

	/** Restores the default verbosity. */
	void ResetToDefault();

	FORCEINLINE ELogVerbosity::Type GetCompileTimeVerbosity() const
	{
		return CompileTimeVerbosity;
	}

	/** The registered category with this name, or nullptr. */
	static FLogCategoryBase* FindCategory(const FName& Name);

	/** Calls Visitor for every registered category. */
	template <typename VisitorType>
	static void ForEachCategory(VisitorType&& Visitor)
	{
		for (FLogCategoryBase* Category = GetFirstCategory(); Category; Category = Category->NextCategory)
		{
			Visitor(*Category);
		}
	}

private:
	static FLogCategoryBase*& GetFirstCategory();

	ELogVerbosity::Type Verbosity;
	ELogVerbosity::Type DefaultVerbosity;
	ELogVerbosity::Type CompileTimeVerbosity;
	FName CategoryName;
	FLogCategoryBase* NextCategory = nullptr;
};

/** A log category with its compile-time verbosity (UE: FLogCategory). */
template <uint8 InDefaultVerbosity, uint8 InCompileTimeVerbosity>
struct FLogCategory : public FLogCategoryBase
{
	static_assert(
		(InDefaultVerbosity & ELogVerbosity::VerbosityMask) < ELogVerbosity::NumVerbosity, "Bogus default verbosity.");
	static_assert(InCompileTimeVerbosity < ELogVerbosity::NumVerbosity, "Bogus compile time verbosity.");

	enum
	{
		CompileTimeVerbosity = InCompileTimeVerbosity
	};

	FORCEINLINE FLogCategory(const TCHAR* InCategoryName)
		: FLogCategoryBase(
			  InCategoryName, ELogVerbosity::Type(InDefaultVerbosity), ELogVerbosity::Type(CompileTimeVerbosity))
	{
	}
};
