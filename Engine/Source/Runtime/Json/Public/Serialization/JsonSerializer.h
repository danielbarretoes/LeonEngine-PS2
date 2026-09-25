#pragma once

#include "CoreTypes.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonGlobals.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

/** Between the JSON DOM and text (UE: FJsonSerializer). */
class FJsonSerializer
{
public:
	/** Reads an object; false on a syntax error or when the root is not an object. */
	template <class CharType>
	static bool Deserialize(const TSharedRef<TJsonReader<CharType>>& Reader, TSharedPtr<FJsonObject>& OutObject)
	{
		TSharedPtr<FJsonValue> Value;
		if (!Deserialize(Reader, Value) || Value->Type != EJson::Object)
		{
			return false;
		}
		OutObject = Value->AsObject();
		return true;
	}

	/** Reads an array. */
	template <class CharType>
	static bool Deserialize(const TSharedRef<TJsonReader<CharType>>& Reader, TArray<TSharedPtr<FJsonValue>>& OutArray)
	{
		TSharedPtr<FJsonValue> Value;
		if (!Deserialize(Reader, Value) || Value->Type != EJson::Array)
		{
			return false;
		}
		OutArray = Value->AsArray();
		return true;
	}

	/** Reads any value. */
	template <class CharType>
	static bool Deserialize(const TSharedRef<TJsonReader<CharType>>& Reader, TSharedPtr<FJsonValue>& OutValue)
	{
		EJsonNotation Notation;
		if (!Reader->ReadNext(Notation))
		{
			return false;
		}
		OutValue = ReadValue(*Reader, Notation);
		if (!OutValue.IsValid())
		{
			return false;
		}

		// Nothing but whitespace may follow the root value.
		EJsonNotation Trailing;
		Reader->ReadNext(Trailing);
		return Reader->GetErrorMessage().IsEmpty();
	}

	template <class CharType, class PrintPolicy>
	static bool Serialize(const TSharedRef<FJsonObject>& Object,
		const TSharedRef<TJsonWriter<CharType, PrintPolicy>>& Writer, bool bCloseWriter = true)
	{
		WriteObject(*Writer, nullptr, *Object);
		return !bCloseWriter || Writer->Close();
	}

	template <class CharType, class PrintPolicy>
	static bool Serialize(const TArray<TSharedPtr<FJsonValue>>& Array,
		const TSharedRef<TJsonWriter<CharType, PrintPolicy>>& Writer, bool bCloseWriter = true)
	{
		WriteArray(*Writer, nullptr, Array);
		return !bCloseWriter || Writer->Close();
	}

	template <class CharType, class PrintPolicy>
	static bool Serialize(const TSharedPtr<FJsonValue>& Value, const FString& Identifier,
		const TSharedRef<TJsonWriter<CharType, PrintPolicy>>& Writer, bool bCloseWriter = true)
	{
		if (!Value.IsValid())
		{
			return false;
		}
		WriteJsonValue(*Writer, Identifier.IsEmpty() ? nullptr : &Identifier, *Value);
		return !bCloseWriter || Writer->Close();
	}

private:
	template <class CharType>
	static TSharedPtr<FJsonValue> ReadValue(TJsonReader<CharType>& Reader, EJsonNotation Notation)
	{
		switch (Notation)
		{
			case EJsonNotation::String:
				return MakeShared<FJsonValueString>(Reader.GetValueAsString());

			case EJsonNotation::Number:
				return MakeShared<FJsonValueNumber>(Reader.GetValueAsNumber());

			case EJsonNotation::Boolean:
				return MakeShared<FJsonValueBoolean>(Reader.GetValueAsBoolean());

			case EJsonNotation::Null:
				return MakeShared<FJsonValueNull>();

			case EJsonNotation::ObjectStart:
			{
				TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
				EJsonNotation Next;
				while (Reader.ReadNext(Next))
				{
					if (Next == EJsonNotation::ObjectEnd)
					{
						return MakeShared<FJsonValueObject>(Object);
					}
					// The identifier belongs to this field; reading the value moves the reader on.
					const FString FieldName = Reader.GetIdentifier();
					TSharedPtr<FJsonValue> Field = ReadValue(Reader, Next);
					if (!Field.IsValid())
					{
						return nullptr;
					}
					Object->Values.Add(FieldName, Field);
				}
				return nullptr;
			}

			case EJsonNotation::ArrayStart:
			{
				TArray<TSharedPtr<FJsonValue>> Array;
				EJsonNotation Next;
				while (Reader.ReadNext(Next))
				{
					if (Next == EJsonNotation::ArrayEnd)
					{
						return MakeShared<FJsonValueArray>(Array);
					}
					TSharedPtr<FJsonValue> Element = ReadValue(Reader, Next);
					if (!Element.IsValid())
					{
						return nullptr;
					}
					Array.Add(Element);
				}
				return nullptr;
			}

			default:
				return nullptr;
		}
	}

	template <class WriterType>
	static void WriteObject(WriterType& Writer, const FString* Identifier, const FJsonObject& Object)
	{
		if (Identifier != nullptr)
		{
			Writer.WriteObjectStart(*Identifier);
		}
		else
		{
			Writer.WriteObjectStart();
		}
		for (const auto& Pair : Object.Values)
		{
			if (Pair.Value.IsValid())
			{
				WriteJsonValue(Writer, &Pair.Key, *Pair.Value);
			}
		}
		Writer.WriteObjectEnd();
	}

	template <class WriterType>
	static void WriteArray(WriterType& Writer, const FString* Identifier, const TArray<TSharedPtr<FJsonValue>>& Array)
	{
		if (Identifier != nullptr)
		{
			Writer.WriteArrayStart(*Identifier);
		}
		else
		{
			Writer.WriteArrayStart();
		}
		for (const TSharedPtr<FJsonValue>& Element : Array)
		{
			if (Element.IsValid())
			{
				WriteJsonValue(Writer, nullptr, *Element);
			}
		}
		Writer.WriteArrayEnd();
	}

	template <class WriterType>
	static void WriteJsonValue(WriterType& Writer, const FString* Identifier, const FJsonValue& Value)
	{
		switch (Value.Type)
		{
			case EJson::Object:
				if (Value.AsObject().IsValid())
				{
					WriteObject(Writer, Identifier, *Value.AsObject());
				}
				else if (Identifier != nullptr)
				{
					Writer.WriteNull(*Identifier);
				}
				else
				{
					Writer.WriteNull();
				}
				break;

			case EJson::Array:
				WriteArray(Writer, Identifier, Value.AsArray());
				break;

			case EJson::String:
				Identifier != nullptr ? Writer.WriteValue(*Identifier, Value.AsString())
									  : Writer.WriteValue(Value.AsString());
				break;

			case EJson::Number:
				Identifier != nullptr ? Writer.WriteValue(*Identifier, Value.AsNumber())
									  : Writer.WriteValue(Value.AsNumber());
				break;

			case EJson::Boolean:
				Identifier != nullptr ? Writer.WriteValue(*Identifier, Value.AsBool())
									  : Writer.WriteValue(Value.AsBool());
				break;

			default:
				Identifier != nullptr ? Writer.WriteNull(*Identifier) : Writer.WriteNull();
				break;
		}
	}
};
