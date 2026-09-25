#include "Tests/LegacyGolden.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY(LogGolden);

namespace
{

	/** Values per logged row, so a pasted table stays within the line length. */
	constexpr int32 VectorsPerRow = 2;
	constexpr int32 ScalarsPerRow = 6;
	constexpr int32 FlagsPerRow = 8;

	/** A float as a C++ literal that reads back to the same float (%.9g plus ".0" when needed, and "f"). */
	FString FloatLiteral(float Value)
	{
		FString Text = FString::Printf("%.9g", static_cast<double>(Value));
		if (!Text.Contains(".") && !Text.Contains("e"))
		{
			Text += ".0";
		}
		Text += "f";
		return Text;
	}

	FString VectorLiteral(const FVector& Value)
	{
		return FString::Printf(
			"FVector(%s, %s, %s)", *FloatLiteral(Value.X), *FloatLiteral(Value.Y), *FloatLiteral(Value.Z));
	}

	/** Logs Items as rows of PerRow values, "<test> | <table> | <values>", with a comma after all but the last. */
	void LogRows(const FAutomationTestBase& Test, const TCHAR* What, const TArray<FString>& Items, int32 PerRow)
	{
		const FString TestName = Test.GetBeautifiedTestName();
		if (Items.Num() == 0)
		{
			UE_LOG(LogGolden, Display, "%s | %s | (no values)", *TestName, What);
			return;
		}
		for (int32 First = 0; First < Items.Num(); First += PerRow)
		{
			FString Row;
			const int32 Last = FMath::Min(First + PerRow, Items.Num());
			for (int32 Index = First; Index < Last; ++Index)
			{
				if (Index > First)
				{
					Row += " ";
				}
				Row += Items[Index];
				if (Index + 1 < Items.Num())
				{
					Row += ",";
				}
			}
			UE_LOG(LogGolden, Display, "%s | %s | %s", *TestName, What, *Row);
		}
	}

	bool CheckCount(FAutomationTestBase& Test, const TCHAR* What, int32 ActualCount, int32 ExpectedCount)
	{
		if (ActualCount == ExpectedCount)
		{
			return true;
		}
		Test.AddError(FString::Printf("%s: %d values but the golden table has %d", What, ActualCount, ExpectedCount));
		return false;
	}

	/** NaN-safe |A - B| <= Tolerance. */
	bool IsNear(float A, float B, float Tolerance)
	{
		return FMath::Abs(A - B) <= Tolerance;
	}

	bool IsNear(const FVector& A, const FVector& B, float Tolerance)
	{
		return IsNear(A.X, B.X, Tolerance) && IsNear(A.Y, B.Y, Tolerance) && IsNear(A.Z, B.Z, Tolerance);
	}

	/** Shared body of the vector checks: ToWorld converts a table entry, ToLegacy converts a result for recording. */
	bool CheckVectors(FAutomationTestBase& Test, const TCHAR* What, const TArray<FVector>& Actual,
		const FVector* Expected, int32 Count, float WorldTolerance, FVector (*ToWorld)(const FVector&),
		FVector (*ToLegacy)(const FVector&))
	{
		if (LegacyGolden::IsRecording())
		{
			TArray<FString> Items;
			for (const FVector& Value : Actual)
			{
				Items.Add(VectorLiteral(ToLegacy(Value)));
			}
			LogRows(Test, What, Items, VectorsPerRow);
			return true;
		}
		if (!CheckCount(Test, What, Actual.Num(), Count))
		{
			return false;
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Want = ToWorld(Expected[Index]);
			if (!IsNear(Actual[Index], Want, WorldTolerance))
			{
				Test.AddError(FString::Printf("%s[%d]: (%.6f, %.6f, %.6f), expected (%.6f, %.6f, %.6f) +/- %g", What,
					Index, Actual[Index].X, Actual[Index].Y, Actual[Index].Z, Want.X, Want.Y, Want.Z,
					static_cast<double>(WorldTolerance)));
				return false;
			}
		}
		return true;
	}

	FVector Unchanged(const FVector& Value)
	{
		return Value;
	}

	float ScalarToWorld(float Value, LegacyGolden::EUnit Unit)
	{
		switch (Unit)
		{
			case LegacyGolden::EUnit::Length:
				return LegacyGolden::ToWorldLength(Value);
			case LegacyGolden::EUnit::Speed:
				return LegacyGolden::ToWorldSpeed(Value);
			case LegacyGolden::EUnit::Unitless:
			default:
				return Value;
		}
	}

	float ScalarToLegacy(float Value, LegacyGolden::EUnit Unit)
	{
		// Speeds scale like lengths (the time unit does not change).
		return Unit == LegacyGolden::EUnit::Unitless ? Value : LegacyGolden::ToLegacyLength(Value);
	}

} // namespace

namespace LegacyGolden
{
	bool IsRecording()
	{
		return FParse::Param(FCommandLine::Get(), "GoldenRecord");
	}

	bool CheckPositions(FAutomationTestBase& Test, const TCHAR* What, const TArray<FVector>& ActualWorld,
		const FVector* ExpectedLegacy, int32 Count, float TolLegacyMetres)
	{
		return CheckVectors(Test, What, ActualWorld, ExpectedLegacy, Count, ToWorldLength(TolLegacyMetres),
			&ToWorldPosition, &ToLegacyPosition);
	}

	bool CheckDirections(FAutomationTestBase& Test, const TCHAR* What, const TArray<FVector>& ActualWorld,
		const FVector* ExpectedLegacy, int32 Count, float Tolerance)
	{
		return CheckVectors(
			Test, What, ActualWorld, ExpectedLegacy, Count, Tolerance, &ToWorldDirection, &ToLegacyDirection);
	}

	bool CheckUnitlessVectors(FAutomationTestBase& Test, const TCHAR* What, const TArray<FVector>& Actual,
		const FVector* Expected, int32 Count, float Tolerance)
	{
		return CheckVectors(Test, What, Actual, Expected, Count, Tolerance, &Unchanged, &Unchanged);
	}

	bool CheckScalars(FAutomationTestBase& Test, const TCHAR* What, const TArray<float>& Actual, const float* Expected,
		int32 Count, float Tolerance, EUnit Unit)
	{
		if (IsRecording())
		{
			TArray<FString> Items;
			for (const float Value : Actual)
			{
				Items.Add(FloatLiteral(ScalarToLegacy(Value, Unit)));
			}
			LogRows(Test, What, Items, ScalarsPerRow);
			return true;
		}
		if (!CheckCount(Test, What, Actual.Num(), Count))
		{
			return false;
		}
		const float WorldTolerance = ScalarToWorld(Tolerance, Unit);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Want = ScalarToWorld(Expected[Index], Unit);
			if (!IsNear(Actual[Index], Want, WorldTolerance))
			{
				Test.AddError(FString::Printf("%s[%d]: %.6f, expected %.6f +/- %g", What, Index,
					static_cast<double>(Actual[Index]), static_cast<double>(Want),
					static_cast<double>(WorldTolerance)));
				return false;
			}
		}
		return true;
	}

	bool CheckBools(
		FAutomationTestBase& Test, const TCHAR* What, const TArray<bool>& Actual, const bool* Expected, int32 Count)
	{
		if (IsRecording())
		{
			TArray<FString> Items;
			for (const bool bValue : Actual)
			{
				Items.Add(bValue ? "true" : "false");
			}
			LogRows(Test, What, Items, FlagsPerRow);
			return true;
		}
		if (!CheckCount(Test, What, Actual.Num(), Count))
		{
			return false;
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Actual[Index] != Expected[Index])
			{
				Test.AddError(FString::Printf("%s[%d]: %s, expected %s", What, Index, Actual[Index] ? "true" : "false",
					Expected[Index] ? "true" : "false"));
				return false;
			}
		}
		return true;
	}

	bool CheckInts(
		FAutomationTestBase& Test, const TCHAR* What, const TArray<int32>& Actual, const int32* Expected, int32 Count)
	{
		if (IsRecording())
		{
			TArray<FString> Items;
			for (const int32 Value : Actual)
			{
				Items.Add(FString::Printf("%d", Value));
			}
			LogRows(Test, What, Items, FlagsPerRow);
			return true;
		}
		if (!CheckCount(Test, What, Actual.Num(), Count))
		{
			return false;
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Actual[Index] != Expected[Index])
			{
				Test.AddError(FString::Printf("%s[%d]: %d, expected %d", What, Index, Actual[Index], Expected[Index]));
				return false;
			}
		}
		return true;
	}

	bool CheckHash(FAutomationTestBase& Test, const TCHAR* What, uint32 Actual, uint32 Expected)
	{
		if (IsRecording())
		{
			UE_LOG(LogGolden, Display, "%s | %s | 0x%08Xu", *Test.GetBeautifiedTestName(), What, Actual);
			return true;
		}
		if (Actual != Expected)
		{
			Test.AddError(FString::Printf("%s: hash 0x%08X, expected 0x%08X", What, Actual, Expected));
			return false;
		}
		return true;
	}
} // namespace LegacyGolden

#endif // WITH_DEV_AUTOMATION_TESTS
