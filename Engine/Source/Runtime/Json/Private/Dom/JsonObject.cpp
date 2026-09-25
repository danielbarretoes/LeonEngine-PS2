#include "Dom/JsonObject.h"

TSharedPtr<FJsonValue> FJsonObject::TryGetField(const FString& FieldName) const
{
	const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
	return (Field != nullptr && Field->IsValid()) ? *Field : TSharedPtr<FJsonValue>();
}

bool FJsonObject::HasField(const FString& FieldName) const
{
	const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
	return Field != nullptr && Field->IsValid();
}

void FJsonObject::SetField(const FString& FieldName, const TSharedPtr<FJsonValue>& Value)
{
	Values.Add(FieldName, Value);
}

void FJsonObject::RemoveField(const FString& FieldName)
{
	Values.Remove(FieldName);
}

double FJsonObject::GetNumberField(const FString& FieldName) const
{
	return GetField<EJson::None>(FieldName)->AsNumber();
}

int32 FJsonObject::GetIntegerField(const FString& FieldName) const
{
	int32 Result = 0;
	GetField<EJson::None>(FieldName)->TryGetNumber(Result);
	return Result;
}

bool FJsonObject::TryGetNumberField(const FString& FieldName, double& OutNumber) const
{
	const TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(const FString& FieldName, float& OutNumber) const
{
	const TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(const FString& FieldName, int32& OutNumber) const
{
	const TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(const FString& FieldName, uint32& OutNumber) const
{
	const TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(const FString& FieldName, int64& OutNumber) const
{
	const TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

void FJsonObject::SetNumberField(const FString& FieldName, double Number)
{
	Values.Add(FieldName, MakeShared<FJsonValueNumber>(Number));
}

FString FJsonObject::GetStringField(const FString& FieldName) const
{
	return GetField<EJson::None>(FieldName)->AsString();
}

bool FJsonObject::TryGetStringField(const FString& FieldName, FString& OutString) const
{
	const TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetString(OutString);
}

bool FJsonObject::TryGetStringArrayField(const FString& FieldName, TArray<FString>& OutArray) const
{
	const TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (!Field.IsValid() || !Field->TryGetArray(Array))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Element : *Array)
	{
		FString Value;
		if (!Element.IsValid() || !Element->TryGetString(Value))
		{
			return false;
		}
		OutArray.Add(Value);
	}
	return true;
}

void FJsonObject::SetStringField(const FString& FieldName, const FString& StringValue)
{
	Values.Add(FieldName, MakeShared<FJsonValueString>(StringValue));
}

bool FJsonObject::GetBoolField(const FString& FieldName) const
{
	return GetField<EJson::None>(FieldName)->AsBool();
}

bool FJsonObject::TryGetBoolField(const FString& FieldName, bool& OutBool) const
{
	const TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetBool(OutBool);
}

void FJsonObject::SetBoolField(const FString& FieldName, bool InValue)
{
	Values.Add(FieldName, MakeShared<FJsonValueBoolean>(InValue));
}

const TArray<TSharedPtr<FJsonValue>>& FJsonObject::GetArrayField(const FString& FieldName) const
{
	const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
	if (Field != nullptr && Field->IsValid())
	{
		return (*Field)->AsArray();
	}
	UE_LOG(LogJson, Error, "Field %s was not found or is of the wrong type.", *FieldName);
	static const TArray<TSharedPtr<FJsonValue>> EmptyArray;
	return EmptyArray;
}

bool FJsonObject::TryGetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArray) const
{
	const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
	return Field != nullptr && Field->IsValid() && (*Field)->TryGetArray(OutArray);
}

void FJsonObject::SetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>& Array)
{
	Values.Add(FieldName, MakeShared<FJsonValueArray>(Array));
}

const TSharedPtr<FJsonObject>& FJsonObject::GetObjectField(const FString& FieldName) const
{
	const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
	if (Field != nullptr && Field->IsValid())
	{
		return (*Field)->AsObject();
	}
	UE_LOG(LogJson, Error, "Field %s was not found or is of the wrong type.", *FieldName);
	static const TSharedPtr<FJsonObject> EmptyObject;
	return EmptyObject;
}

bool FJsonObject::TryGetObjectField(const FString& FieldName, const TSharedPtr<FJsonObject>*& OutObject) const
{
	const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
	return Field != nullptr && Field->IsValid() && (*Field)->TryGetObject(OutObject);
}

void FJsonObject::SetObjectField(const FString& FieldName, const TSharedPtr<FJsonObject>& JsonObject)
{
	if (JsonObject.IsValid())
	{
		Values.Add(FieldName, MakeShared<FJsonValueObject>(JsonObject));
	}
	else
	{
		Values.Add(FieldName, MakeShared<FJsonValueNull>());
	}
}
