#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Dom/JsonValue.h"
#include "JsonGlobals.h"
#include "Templates/SharedPointer.h"

/**
 * A JSON object: named values in insertion order (UE: FJsonObject). Field names follow FString's case-insensitive
 * comparison, like UE. Get*Field log an error when the field is missing or of another type; TryGet*Field return false.
 */
class JSON_API FJsonObject
{
public:
	TMap<FString, TSharedPtr<FJsonValue>> Values;

	/** The field if it has type JsonType, else an error and a null value (UE: GetField<JsonType>). */
	template <EJson JsonType>
	TSharedPtr<FJsonValue> GetField(const FString& FieldName) const
	{
		const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
		if (Field != nullptr && Field->IsValid() && (JsonType == EJson::None || (*Field)->Type == JsonType))
		{
			return *Field;
		}
		UE_LOG(LogJson, Error, "Field %s was not found or is of the wrong type.", *FieldName);
		return MakeShared<FJsonValueNull>();
	}

	/** The field, or null when missing (UE: TryGetField). */
	TSharedPtr<FJsonValue> TryGetField(const FString& FieldName) const;

	bool HasField(const FString& FieldName) const;

	template <EJson JsonType>
	bool HasTypedField(const FString& FieldName) const
	{
		const TSharedPtr<FJsonValue>* Field = Values.Find(FieldName);
		return Field != nullptr && Field->IsValid() && (*Field)->Type == JsonType;
	}

	void SetField(const FString& FieldName, const TSharedPtr<FJsonValue>& Value);
	void RemoveField(const FString& FieldName);

	double GetNumberField(const FString& FieldName) const;
	int32 GetIntegerField(const FString& FieldName) const;
	bool TryGetNumberField(const FString& FieldName, double& OutNumber) const;
	bool TryGetNumberField(const FString& FieldName, float& OutNumber) const;
	bool TryGetNumberField(const FString& FieldName, int32& OutNumber) const;
	bool TryGetNumberField(const FString& FieldName, uint32& OutNumber) const;
	bool TryGetNumberField(const FString& FieldName, int64& OutNumber) const;
	void SetNumberField(const FString& FieldName, double Number);

	FString GetStringField(const FString& FieldName) const;
	bool TryGetStringField(const FString& FieldName, FString& OutString) const;
	bool TryGetStringArrayField(const FString& FieldName, TArray<FString>& OutArray) const;
	void SetStringField(const FString& FieldName, const FString& StringValue);

	bool GetBoolField(const FString& FieldName) const;
	bool TryGetBoolField(const FString& FieldName, bool& OutBool) const;
	void SetBoolField(const FString& FieldName, bool InValue);

	const TArray<TSharedPtr<FJsonValue>>& GetArrayField(const FString& FieldName) const;
	bool TryGetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArray) const;
	void SetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>& Array);

	const TSharedPtr<FJsonObject>& GetObjectField(const FString& FieldName) const;
	bool TryGetObjectField(const FString& FieldName, const TSharedPtr<FJsonObject>*& OutObject) const;
	void SetObjectField(const FString& FieldName, const TSharedPtr<FJsonObject>& JsonObject);
};
