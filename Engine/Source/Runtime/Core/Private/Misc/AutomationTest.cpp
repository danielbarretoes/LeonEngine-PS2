#include "Misc/AutomationTest.h"

#include "CoreGlobals.h"
#include "HAL/PlatformTime.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Templates/Function.h"

#include <cmath>

DEFINE_LOG_CATEGORY_STATIC(LogAutomationTest, Log, All);

namespace
{
	/** Turns error / warning log lines written during a test into test errors / warnings (UE behaviour). */
	class FAutomationLogCapture final : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			const ELogVerbosity::Type Level = ELogVerbosity::Type(Verbosity & ELogVerbosity::VerbosityMask);
			if (Category == LogAutomationTest.GetCategoryName())
			{
				return;
			}
			if (Level <= ELogVerbosity::Error)
			{
				Callback(V, true);
			}
			else if (Level == ELogVerbosity::Warning)
			{
				Callback(V, false);
			}
		}

		TFunction<void(const TCHAR*, bool)> Callback;
	};
} // namespace

FAutomationTestBase::FAutomationTestBase(const FString& InName, const bool bInComplexTask)
	: TestName(InName)
	, bComplexTask(bInComplexTask)
{
	FAutomationTestFramework::Get().RegisterAutomationTest(InName, this);
}

FAutomationTestBase::~FAutomationTestBase()
{
	FAutomationTestFramework::Get().UnregisterAutomationTest(TestName);
}

void FAutomationTestBase::AddError(const FString& InError, int32 /*StackOffset*/)
{
	Errors.Add(InError);
}

void FAutomationTestBase::AddWarning(const FString& InWarning, int32 /*StackOffset*/)
{
	Warnings.Add(InWarning);
}

void FAutomationTestBase::AddInfo(const FString& InLogItem, int32 /*StackOffset*/)
{
	Infos.Add(InLogItem);
}

void FAutomationTestBase::AddExpectedError(const FString& Pattern, int32 Occurrences)
{
	ExpectedErrors.Add({Pattern, Occurrences});
}

bool FAutomationTestBase::ConsumeExpectedError(const TCHAR* Message)
{
	const FString Text(Message);
	for (FExpectedError& Expected : ExpectedErrors)
	{
		if (Expected.Remaining > 0 && Text.Contains(Expected.Pattern))
		{
			--Expected.Remaining;
			return true;
		}
	}
	return false;
}

bool FAutomationTestBase::TestTrue(const TCHAR* What, bool bValue)
{
	if (!bValue)
	{
		AddError(FString::Printf("%s: Expected to be true.", What), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestFalse(const TCHAR* What, bool bValue)
{
	if (bValue)
	{
		AddError(FString::Printf("%s: Expected to be false.", What), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, int32 Actual, int32 Expected)
{
	if (Actual != Expected)
	{
		AddError(FString::Printf("Expected '%s' to be %d, but it was %d.", What, Expected, Actual), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, int64 Actual, int64 Expected)
{
	if (Actual != Expected)
	{
		AddError(
			FString::Printf("Expected '%s' to be %lld, but it was %lld.", What, (long long)Expected, (long long)Actual),
			1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, uint32 Actual, uint32 Expected)
{
	if (Actual != Expected)
	{
		AddError(FString::Printf("Expected '%s' to be %u, but it was %u.", What, Expected, Actual), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, uint64 Actual, uint64 Expected)
{
	if (Actual != Expected)
	{
		AddError(FString::Printf("Expected '%s' to be %llu, but it was %llu.", What, (unsigned long long)Expected,
					 (unsigned long long)Actual),
			1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, float Actual, float Expected, float Tolerance)
{
	if (!(std::fabs(Actual - Expected) <= Tolerance))
	{
		AddError(FString::Printf("Expected '%s' to be %f, but it was %f within tolerance %f.", What, double(Expected),
					 double(Actual), double(Tolerance)),
			1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, double Actual, double Expected, double Tolerance)
{
	if (!(std::fabs(Actual - Expected) <= Tolerance))
	{
		AddError(FString::Printf(
					 "Expected '%s' to be %f, but it was %f within tolerance %f.", What, Expected, Actual, Tolerance),
			1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const FString& Actual, const FString& Expected)
{
	if (!Actual.Equals(Expected, ESearchCase::CaseSensitive))
	{
		AddError(FString::Printf("Expected '%s' to be \"%s\", but it was \"%s\".", What, *Expected, *Actual), 1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const FString& Actual, const TCHAR* Expected)
{
	return TestEqual(What, Actual, FString(Expected));
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, const TCHAR* Actual, const TCHAR* Expected)
{
	return TestEqual(What, FString(Actual), FString(Expected));
}

bool FAutomationTestBase::TestEqual(const TCHAR* What, FName Actual, FName Expected)
{
	if (Actual != Expected)
	{
		AddError(FString::Printf(
					 "Expected '%s' to be \"%s\", but it was \"%s\".", What, *Expected.ToString(), *Actual.ToString()),
			1);
		return false;
	}
	return true;
}

bool FAutomationTestBase::TestEqualInsensitive(const TCHAR* What, const TCHAR* Actual, const TCHAR* Expected)
{
	if (FCString::Stricmp(Actual, Expected) != 0)
	{
		AddError(FString::Printf("Expected '%s' to be \"%s\", but it was \"%s\".", What, Expected, Actual), 1);
		return false;
	}
	return true;
}

FAutomationTestFramework& FAutomationTestFramework::Get()
{
	static FAutomationTestFramework Framework;
	return Framework;
}

bool FAutomationTestFramework::RegisterAutomationTest(
	const FString& InTestNameToRegister, FAutomationTestBase* InTestToRegister)
{
	if (Tests.Contains(InTestNameToRegister))
	{
		return false;
	}
	Tests.Add(InTestNameToRegister, InTestToRegister);
	return true;
}

bool FAutomationTestFramework::UnregisterAutomationTest(const FString& InTestNameToUnregister)
{
	return Tests.Remove(InTestNameToUnregister) > 0;
}

void FAutomationTestFramework::GetTestNames(TArray<FString>& OutNames) const
{
	OutNames.Reset();
	for (const auto& Pair : Tests)
	{
		OutNames.Add(Pair.Value->GetBeautifiedTestName());
	}
	OutNames.Sort();
}

bool FAutomationTestFramework::RunTest(FAutomationTestBase& Test)
{
	Test.Errors.Reset();
	Test.Warnings.Reset();
	Test.Infos.Reset();
	Test.ExpectedErrors.Reset();

	FAutomationLogCapture Capture;
	Capture.Callback = [&Test](const TCHAR* Message, bool bError)
	{
		if (!bError)
		{
			Test.AddWarning(Message);
		}
		else if (!Test.ConsumeExpectedError(Message))
		{
			Test.AddError(Message);
		}
	};

	CurrentTest = &Test;
	GLog->AddOutputDevice(&Capture);
	const bool bReturned = Test.RunTest(FString());
	GLog->RemoveOutputDevice(&Capture);
	CurrentTest = nullptr;

	for (const FAutomationTestBase::FExpectedError& Expected : Test.ExpectedErrors)
	{
		if (Expected.Remaining > 0)
		{
			Test.AddError(
				FString::Printf("Expected error not seen %d more time(s): %s", Expected.Remaining, *Expected.Pattern));
		}
	}
	if (!bReturned && !Test.HasAnyErrors())
	{
		Test.AddError("RunTest returned false");
	}
	return !Test.HasAnyErrors();
}

int32 FAutomationTestFramework::RunTests(const TCHAR* Filter, uint32 ExcludeFlags, int32* OutNumRun)
{
	TArray<FAutomationTestBase*> Selected;
	for (const auto& Pair : Tests)
	{
		FAutomationTestBase* Test = Pair.Value;
		if ((Test->GetTestFlags() & ExcludeFlags) != 0)
		{
			continue;
		}
		if (Filter && *Filter && !Test->GetBeautifiedTestName().Contains(Filter))
		{
			continue;
		}
		Selected.Add(Test);
	}
	Selected.Sort([](const FAutomationTestBase& A, const FAutomationTestBase& B)
		{ return A.GetBeautifiedTestName().Compare(B.GetBeautifiedTestName()) < 0; });

	int32 NumFailed = 0;
	const double StartTime = FPlatformTime::Seconds();
	for (FAutomationTestBase* Test : Selected)
	{
		const bool bPassed = RunTest(*Test);
		UE_LOG(LogAutomationTest, Display, TEXT("Test Completed. Result={%s} Name={%s}"), bPassed ? "Passed" : "Failed",
			*Test->GetBeautifiedTestName());
		for (const FString& Error : Test->GetErrors())
		{
			UE_LOG(LogAutomationTest, Display, TEXT("  Error: %s"), *Error);
		}
		if (!bPassed)
		{
			++NumFailed;
		}
	}

	UE_LOG(LogAutomationTest, Display, TEXT("Automation: %d test(s), %d passed, %d failed (%.2f s)"), Selected.Num(),
		Selected.Num() - NumFailed, NumFailed, FPlatformTime::Seconds() - StartTime);
	if (OutNumRun)
	{
		*OutNumRun = Selected.Num();
	}
	return NumFailed;
}
