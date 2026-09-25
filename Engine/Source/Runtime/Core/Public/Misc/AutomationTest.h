#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"

class FOutputDevice;

/** Test flags: application contexts and filters (UE: EAutomationTestFlags, the subset Leon uses). */
namespace EAutomationTestFlags
{
	enum Type : uint32
	{
		EditorContext = 0x00000001,
		ClientContext = 0x00000002,
		ServerContext = 0x00000004,
		CommandletContext = 0x00000008,
		ApplicationContextMask = EditorContext | ClientContext | ServerContext | CommandletContext,

		/** Needs a GPU / window (skipped by TestPAL and -nullrhi runs). */
		NonNullRHI = 0x00000100,
		Disabled = 0x00010000,

		SmokeFilter = 0x01000000,
		EngineFilter = 0x02000000,
		ProductFilter = 0x04000000,
		PerfFilter = 0x08000000,
		StressFilter = 0x10000000,
		NegativeFilter = 0x20000000,
		FilterMask = SmokeFilter | EngineFilter | ProductFilter | PerfFilter | StressFilter | NegativeFilter
	};
} // namespace EAutomationTestFlags

/**
 * Base of an automation test (UE: FAutomationTestBase). Declare tests with IMPLEMENT_SIMPLE_AUTOMATION_TEST and
 * report with TestEqual / TestTrue / AddError; errors logged while the test runs also fail it (UE behaviour).
 */
class CORE_API FAutomationTestBase
{
	friend class FAutomationTestFramework;

public:
	FAutomationTestBase(const FString& InName, const bool bInComplexTask);
	virtual ~FAutomationTestBase();

	FAutomationTestBase(const FAutomationTestBase&) = delete;
	FAutomationTestBase& operator=(const FAutomationTestBase&) = delete;

	virtual uint32 GetTestFlags() const = 0;

	/** Dotted display name: "System.Core.Containers.Array". */
	virtual FString GetBeautifiedTestName() const = 0;

	/** The class name the test was registered with. */
	const FString& GetTestName() const
	{
		return TestName;
	}

	virtual void AddError(const FString& InError, int32 StackOffset = 0);
	virtual void AddWarning(const FString& InWarning, int32 StackOffset = 0);
	virtual void AddInfo(const FString& InLogItem, int32 StackOffset = 0);

	bool HasAnyErrors() const
	{
		return Errors.Num() > 0;
	}

	const TArray<FString>& GetErrors() const
	{
		return Errors;
	}
	const TArray<FString>& GetWarnings() const
	{
		return Warnings;
	}

	/** Expects an error log line containing Pattern during the test; it then does not fail the test. */
	void AddExpectedError(const FString& Pattern, int32 Occurrences = 1);

	// Checks. Each returns the result so tests can early-out: if (!TestNotNull(...)) { return false; }

	bool TestTrue(const TCHAR* What, bool bValue);
	bool TestFalse(const TCHAR* What, bool bValue);

	bool TestEqual(const TCHAR* What, int32 Actual, int32 Expected);
	bool TestEqual(const TCHAR* What, int64 Actual, int64 Expected);
	bool TestEqual(const TCHAR* What, uint32 Actual, uint32 Expected);
	bool TestEqual(const TCHAR* What, uint64 Actual, uint64 Expected);
	bool TestEqual(const TCHAR* What, float Actual, float Expected, float Tolerance = 1.e-4f);
	bool TestEqual(const TCHAR* What, double Actual, double Expected, double Tolerance = 1.e-4);
	bool TestEqual(const TCHAR* What, const FString& Actual, const FString& Expected);
	bool TestEqual(const TCHAR* What, const FString& Actual, const TCHAR* Expected);
	bool TestEqual(const TCHAR* What, const TCHAR* Actual, const TCHAR* Expected);
	bool TestEqual(const TCHAR* What, FName Actual, FName Expected);

	/** Case-insensitive string comparison. */
	bool TestEqualInsensitive(const TCHAR* What, const TCHAR* Actual, const TCHAR* Expected);

	/** Any type with operator==. */
	template <typename ValueType>
	bool TestEqual(const TCHAR* What, const ValueType& Actual, const ValueType& Expected)
	{
		if (!(Actual == Expected))
		{
			AddError(FString::Printf("%s: The two values are not equal.", What), 1);
			return false;
		}
		return true;
	}

	template <typename ValueType>
	bool TestNotEqual(const TCHAR* What, const ValueType& Actual, const ValueType& Expected)
	{
		if (Actual == Expected)
		{
			AddError(FString::Printf("%s: The two values are equal.", What), 1);
			return false;
		}
		return true;
	}

	template <typename ValueType>
	bool TestSame(const TCHAR* What, const ValueType& Actual, const ValueType& Expected)
	{
		if (&Actual != &Expected)
		{
			AddError(FString::Printf("%s: The two values are not the same.", What), 1);
			return false;
		}
		return true;
	}

	template <typename ValueType>
	bool TestNull(const TCHAR* What, const ValueType* Pointer)
	{
		if (Pointer != nullptr)
		{
			AddError(FString::Printf("Expected '%s' to be null.", What), 1);
			return false;
		}
		return true;
	}

	template <typename ValueType>
	bool TestNotNull(const TCHAR* What, const ValueType* Pointer)
	{
		if (Pointer == nullptr)
		{
			AddError(FString::Printf("Expected '%s' to be not null.", What), 1);
			return false;
		}
		return true;
	}

	template <typename ValueType>
	bool TestValid(const TCHAR* What, const ValueType& Value)
	{
		if (!Value.IsValid())
		{
			AddError(FString::Printf("Expected '%s' to be valid.", What), 1);
			return false;
		}
		return true;
	}

	template <typename ValueType>
	bool TestInvalid(const TCHAR* What, const ValueType& Value)
	{
		if (Value.IsValid())
		{
			AddError(FString::Printf("Expected '%s' to be invalid.", What), 1);
			return false;
		}
		return true;
	}

protected:
	/** The test body; return false (or add errors) to fail. */
	virtual bool RunTest(const FString& Parameters) = 0;

private:
	/** Called by the framework for each error log line; true when it matched an expected error. */
	bool ConsumeExpectedError(const TCHAR* Message);

	struct FExpectedError
	{
		FString Pattern;
		int32 Remaining;
	};

	FString TestName;
	bool bComplexTask;
	TArray<FString> Errors;
	TArray<FString> Warnings;
	TArray<FString> Infos;
	TArray<FExpectedError> ExpectedErrors;
};

/** Registry and runner of the automation tests linked into the executable (UE: FAutomationTestFramework). */
class CORE_API FAutomationTestFramework
{
public:
	static FAutomationTestFramework& Get();

	bool RegisterAutomationTest(const FString& InTestNameToRegister, FAutomationTestBase* InTestToRegister);
	bool UnregisterAutomationTest(const FString& InTestNameToUnregister);

	/** Beautified names of the registered tests, sorted. */
	void GetTestNames(TArray<FString>& OutNames) const;

	/**
	 * Runs the tests whose beautified name contains Filter (all when empty), skipping disabled tests and the ones
	 * whose flags intersect ExcludeFlags. Prints one line per test and a summary to GLog; returns the failures.
	 */
	int32 RunTests(const TCHAR* Filter = TEXT(""), uint32 ExcludeFlags = EAutomationTestFlags::Disabled,
		int32* OutNumRun = nullptr);

	/** Runs one test; true when it passed. */
	bool RunTest(FAutomationTestBase& Test);

	/** The test being run, or nullptr. */
	FAutomationTestBase* GetCurrentTest() const
	{
		return CurrentTest;
	}

private:
	FAutomationTestFramework() = default;

	TMap<FString, FAutomationTestBase*> Tests;
	FAutomationTestBase* CurrentTest = nullptr;
};

/** Declares a test class and registers an instance (UE: IMPLEMENT_SIMPLE_AUTOMATION_TEST). Define TClass::RunTest. */
#define IMPLEMENT_SIMPLE_AUTOMATION_TEST(TClass, PrettyName, TFlags)                                                   \
	class TClass : public FAutomationTestBase                                                                          \
	{                                                                                                                  \
	public:                                                                                                            \
		TClass(const FString& InName)                                                                                  \
			: FAutomationTestBase(InName, false)                                                                       \
		{                                                                                                              \
		}                                                                                                              \
		virtual uint32 GetTestFlags() const override                                                                   \
		{                                                                                                              \
			return TFlags;                                                                                             \
		}                                                                                                              \
		virtual FString GetBeautifiedTestName() const override                                                         \
		{                                                                                                              \
			return PrettyName;                                                                                         \
		}                                                                                                              \
                                                                                                                       \
	protected:                                                                                                         \
		virtual bool RunTest(const FString& Parameters) override;                                                      \
	};                                                                                                                 \
	namespace                                                                                                          \
	{                                                                                                                  \
		TClass TClass##AutomationTestInstance(TEXT(#TClass));                                                          \
	}

// Test bodies rarely use RunTest's Parameters; UE compiles with the unused-parameter warning off. Only test sources
// include this header.
#if defined(_MSC_VER)
	#pragma warning(disable : 4100)
#elif defined(__GNUC__)
	#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
