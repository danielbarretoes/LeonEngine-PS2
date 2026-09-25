#include "Logging/LogCategory.h"

#include "Misc/CString.h"

FLogCategoryBase*& FLogCategoryBase::GetFirstCategory()
{
	static FLogCategoryBase* First = nullptr;
	return First;
}

FLogCategoryBase::FLogCategoryBase(
	const TCHAR* InCategoryName, ELogVerbosity::Type InDefaultVerbosity, ELogVerbosity::Type InCompileTimeVerbosity)
	: DefaultVerbosity(InDefaultVerbosity)
	, CompileTimeVerbosity(InCompileTimeVerbosity)
	, CategoryName(InCategoryName)
{
	ResetToDefault();
	NextCategory = GetFirstCategory();
	GetFirstCategory() = this;
}

FLogCategoryBase::~FLogCategoryBase()
{
	for (FLogCategoryBase** Link = &GetFirstCategory(); *Link; Link = &(*Link)->NextCategory)
	{
		if (*Link == this)
		{
			*Link = NextCategory;
			break;
		}
	}
}

void FLogCategoryBase::SetVerbosity(ELogVerbosity::Type NewVerbosity)
{
	const ELogVerbosity::Type Level = ELogVerbosity::Type(NewVerbosity & ELogVerbosity::VerbosityMask);
	Verbosity = Level < CompileTimeVerbosity ? Level : CompileTimeVerbosity;
}

void FLogCategoryBase::ResetToDefault()
{
	SetVerbosity(DefaultVerbosity);
}

FLogCategoryBase* FLogCategoryBase::FindCategory(const FName& Name)
{
	for (FLogCategoryBase* Category = GetFirstCategory(); Category; Category = Category->NextCategory)
	{
		if (Category->CategoryName == Name)
		{
			return Category;
		}
	}
	return nullptr;
}

const TCHAR* ToString(ELogVerbosity::Type Verbosity)
{
	switch (Verbosity & ELogVerbosity::VerbosityMask)
	{
		case ELogVerbosity::NoLogging:
			return TEXT("NoLogging");
		case ELogVerbosity::Fatal:
			return TEXT("Fatal");
		case ELogVerbosity::Error:
			return TEXT("Error");
		case ELogVerbosity::Warning:
			return TEXT("Warning");
		case ELogVerbosity::Display:
			return TEXT("Display");
		case ELogVerbosity::Log:
			return TEXT("Log");
		case ELogVerbosity::Verbose:
			return TEXT("Verbose");
		case ELogVerbosity::VeryVerbose:
			return TEXT("VeryVerbose");
		default:
			return TEXT("UknownVerbosity");
	}
}

ELogVerbosity::Type ParseLogVerbosityFromString(const TCHAR* VerbosityString)
{
	static const struct
	{
		const TCHAR* Name;
		ELogVerbosity::Type Verbosity;
	} Table[] = {
		{TEXT("NoLogging"), ELogVerbosity::NoLogging},
		{TEXT("Fatal"), ELogVerbosity::Fatal},
		{TEXT("Error"), ELogVerbosity::Error},
		{TEXT("Warning"), ELogVerbosity::Warning},
		{TEXT("Display"), ELogVerbosity::Display},
		{TEXT("Log"), ELogVerbosity::Log},
		{TEXT("Verbose"), ELogVerbosity::Verbose},
		{TEXT("VeryVerbose"), ELogVerbosity::VeryVerbose},
		{TEXT("All"), ELogVerbosity::All},
	};
	for (const auto& Entry : Table)
	{
		if (FCString::Stricmp(VerbosityString, Entry.Name) == 0)
		{
			return Entry.Verbosity;
		}
	}
	return ELogVerbosity::NoLogging;
}
