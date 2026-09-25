#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "JsonGlobals.h"
#include "Templates/SharedPointer.h"

class FJsonObject;

/**
 * A JSON value (UE: FJsonValue). Numbers are doubles like UE; the As* accessors log an error and return a default
 * when the type does not match, the TryGet* ones return false.
 */
class JSON_API FJsonValue
{
public:
	virtual ~FJsonValue() = default;

	virtual double AsNumber() const;
	virtual FString AsString() const;
	virtual bool AsBool() const;
	virtual const TArray<TSharedPtr<FJsonValue>>& AsArray() const;
	virtual const TSharedPtr<FJsonObject>& AsObject() const;

	virtual bool TryGetNumber(double& /*OutNumber*/) const
	{
		return false;
	}
	bool TryGetNumber(float& OutNumber) const;
	bool TryGetNumber(int32& OutNumber) const;
	bool TryGetNumber(uint32& OutNumber) const;
	bool TryGetNumber(int64& OutNumber) const;
	bool TryGetNumber(uint8& OutNumber) const;

	virtual bool TryGetString(FString& /*OutString*/) const
	{
		return false;
	}

	virtual bool TryGetBool(bool& /*OutBool*/) const
	{
		return false;
	}

	virtual bool TryGetArray(const TArray<TSharedPtr<FJsonValue>>*& /*OutArray*/) const
	{
		return false;
	}

	virtual bool TryGetObject(const TSharedPtr<FJsonObject>*& /*OutObject*/) const
	{
		return false;
	}

	bool IsNull() const
	{
		return Type == EJson::Null || Type == EJson::None;
	}

	/** Deep comparison (UE: CompareEqual). */
	static bool CompareEqual(const FJsonValue& Lhs, const FJsonValue& Rhs);

	EJson Type = EJson::None;

protected:
	virtual FString GetType() const = 0;
	void ErrorMessage(const FString& InType) const;
};

inline bool operator==(const FJsonValue& Lhs, const FJsonValue& Rhs)
{
	return FJsonValue::CompareEqual(Lhs, Rhs);
}

inline bool operator!=(const FJsonValue& Lhs, const FJsonValue& Rhs)
{
	return !FJsonValue::CompareEqual(Lhs, Rhs);
}

/** A string; TryGetNumber / TryGetBool parse it (UE: FJsonValueString). */
class JSON_API FJsonValueString : public FJsonValue
{
public:
	explicit FJsonValueString(const FString& InString)
		: Value(InString)
	{
		Type = EJson::String;
	}

	virtual bool TryGetString(FString& OutString) const override
	{
		OutString = Value;
		return true;
	}
	virtual bool TryGetNumber(double& OutDouble) const override;
	virtual bool TryGetBool(bool& OutBool) const override;

protected:
	virtual FString GetType() const override
	{
		return FString("String");
	}

	FString Value;
};

/** A number (UE: FJsonValueNumber). */
class JSON_API FJsonValueNumber : public FJsonValue
{
public:
	explicit FJsonValueNumber(double InNumber)
		: Value(InNumber)
	{
		Type = EJson::Number;
	}

	virtual bool TryGetNumber(double& OutNumber) const override
	{
		OutNumber = Value;
		return true;
	}
	virtual bool TryGetBool(bool& OutBool) const override
	{
		OutBool = Value != 0.0;
		return true;
	}
	virtual bool TryGetString(FString& OutString) const override;

protected:
	virtual FString GetType() const override
	{
		return FString("Number");
	}

	double Value;
};

/** A boolean (UE: FJsonValueBoolean). */
class JSON_API FJsonValueBoolean : public FJsonValue
{
public:
	explicit FJsonValueBoolean(bool InBool)
		: Value(InBool)
	{
		Type = EJson::Boolean;
	}

	virtual bool TryGetNumber(double& OutNumber) const override
	{
		OutNumber = Value ? 1.0 : 0.0;
		return true;
	}
	virtual bool TryGetBool(bool& OutBool) const override
	{
		OutBool = Value;
		return true;
	}
	virtual bool TryGetString(FString& OutString) const override
	{
		OutString = Value ? FString("true") : FString("false");
		return true;
	}

protected:
	virtual FString GetType() const override
	{
		return FString("Boolean");
	}

	bool Value;
};

/** An array (UE: FJsonValueArray). */
class JSON_API FJsonValueArray : public FJsonValue
{
public:
	explicit FJsonValueArray(const TArray<TSharedPtr<FJsonValue>>& InArray)
		: Value(InArray)
	{
		Type = EJson::Array;
	}

	virtual bool TryGetArray(const TArray<TSharedPtr<FJsonValue>>*& OutArray) const override
	{
		OutArray = &Value;
		return true;
	}

protected:
	virtual FString GetType() const override
	{
		return FString("Array");
	}

	TArray<TSharedPtr<FJsonValue>> Value;
};

/** An object (UE: FJsonValueObject). */
class JSON_API FJsonValueObject : public FJsonValue
{
public:
	explicit FJsonValueObject(TSharedPtr<FJsonObject> InObject)
		: Value(InObject)
	{
		Type = EJson::Object;
	}

	virtual bool TryGetObject(const TSharedPtr<FJsonObject>*& OutObject) const override
	{
		OutObject = &Value;
		return true;
	}

protected:
	virtual FString GetType() const override
	{
		return FString("Object");
	}

	TSharedPtr<FJsonObject> Value;
};

/** null (UE: FJsonValueNull). */
class JSON_API FJsonValueNull : public FJsonValue
{
public:
	FJsonValueNull()
	{
		Type = EJson::Null;
	}

protected:
	virtual FString GetType() const override
	{
		return FString("Null");
	}
};
