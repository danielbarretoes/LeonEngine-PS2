#include "Dom/JsonValue.h"

#include "Dom/JsonObject.h"
#include "Math/NumericLimits.h"
#include "Misc/CString.h"

namespace
{
	const TArray<TSharedPtr<FJsonValue>> EmptyArray;
	const TSharedPtr<FJsonObject> EmptyObject;

	/** Double to integer with range checks, like UE's TryConvertNumber. */
	template <typename T>
	bool TryConvertNumber(const FJsonValue& InValue, T& OutNumber)
	{
		double Double;
		if (!InValue.TryGetNumber(Double))
		{
			return false;
		}
		if (Double < double(TNumericLimits<T>::Min()) || Double > double(TNumericLimits<T>::Max()))
		{
			return false;
		}
		// Round half away from zero (UE: RoundHalfFromZero).
		OutNumber = T(Double < 0.0 ? Double - 0.5 : Double + 0.5);
		return true;
	}
} // namespace

double FJsonValue::AsNumber() const
{
	double Number = 0.0;
	if (!TryGetNumber(Number))
	{
		ErrorMessage("Number");
	}
	return Number;
}

FString FJsonValue::AsString() const
{
	FString String;
	if (!TryGetString(String))
	{
		ErrorMessage("String");
	}
	return String;
}

bool FJsonValue::AsBool() const
{
	bool Bool = false;
	if (!TryGetBool(Bool))
	{
		ErrorMessage("Boolean");
	}
	return Bool;
}

const TArray<TSharedPtr<FJsonValue>>& FJsonValue::AsArray() const
{
	const TArray<TSharedPtr<FJsonValue>>* Array = &EmptyArray;
	if (!TryGetArray(Array))
	{
		ErrorMessage("Array");
	}
	return *Array;
}

const TSharedPtr<FJsonObject>& FJsonValue::AsObject() const
{
	const TSharedPtr<FJsonObject>* Object = &EmptyObject;
	if (!TryGetObject(Object))
	{
		ErrorMessage("Object");
	}
	return *Object;
}

bool FJsonValue::TryGetNumber(float& OutNumber) const
{
	double Double;
	if (TryGetNumber(Double))
	{
		OutNumber = float(Double);
		return true;
	}
	return false;
}

bool FJsonValue::TryGetNumber(int32& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(uint32& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(int64& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::TryGetNumber(uint8& OutNumber) const
{
	return TryConvertNumber(*this, OutNumber);
}

bool FJsonValue::CompareEqual(const FJsonValue& Lhs, const FJsonValue& Rhs)
{
	if (Lhs.Type != Rhs.Type)
	{
		return false;
	}

	switch (Lhs.Type)
	{
		case EJson::None:
		case EJson::Null:
			return true;

		case EJson::String:
			return Lhs.AsString().Equals(Rhs.AsString(), ESearchCase::CaseSensitive);

		case EJson::Number:
			return Lhs.AsNumber() == Rhs.AsNumber();

		case EJson::Boolean:
			return Lhs.AsBool() == Rhs.AsBool();

		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& LhsArray = Lhs.AsArray();
			const TArray<TSharedPtr<FJsonValue>>& RhsArray = Rhs.AsArray();
			if (LhsArray.Num() != RhsArray.Num())
			{
				return false;
			}
			for (int32 Index = 0; Index < LhsArray.Num(); ++Index)
			{
				if (!CompareEqual(*LhsArray[Index], *RhsArray[Index]))
				{
					return false;
				}
			}
			return true;
		}

		case EJson::Object:
		{
			const TSharedPtr<FJsonObject>& LhsObject = Lhs.AsObject();
			const TSharedPtr<FJsonObject>& RhsObject = Rhs.AsObject();
			if (LhsObject.IsValid() != RhsObject.IsValid())
			{
				return false;
			}
			if (!LhsObject.IsValid())
			{
				return true;
			}
			if (LhsObject->Values.Num() != RhsObject->Values.Num())
			{
				return false;
			}
			for (const auto& Pair : LhsObject->Values)
			{
				const TSharedPtr<FJsonValue>* RhsValue = RhsObject->Values.Find(Pair.Key);
				if (RhsValue == nullptr || !CompareEqual(*Pair.Value, **RhsValue))
				{
					return false;
				}
			}
			return true;
		}
	}
	return false;
}

void FJsonValue::ErrorMessage(const FString& InType) const
{
	UE_LOG(LogJson, Error, "Json Value of type '%s' used as a '%s'.", *GetType(), *InType);
}

bool FJsonValueString::TryGetNumber(double& OutDouble) const
{
	if (Value.IsNumeric())
	{
		OutDouble = FCString::Atod(*Value);
		return true;
	}
	return false;
}

bool FJsonValueString::TryGetBool(bool& OutBool) const
{
	OutBool = Value.ToBool();
	return true;
}

bool FJsonValueNumber::TryGetString(FString& OutString) const
{
	// Integers print without a fraction, other values with the shortest exact form.
	if (Value == double(int64(Value)))
	{
		OutString = FString::Printf("%lld", (long long)int64(Value));
	}
	else
	{
		OutString = FString::Printf("%.17g", Value);
	}
	return true;
}
