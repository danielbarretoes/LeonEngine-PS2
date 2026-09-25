// FProperty and the scalar property types: numbers, bool, byte, enum, string, name and text (UE: Property.cpp,
// PropertyNumeric.cpp, PropertyBool.cpp, PropertyStr.cpp, PropertyName.cpp, TextProperty.cpp, EnumProperty.cpp).

#include "Misc/CString.h"
#include "Misc/Char.h"
#include "Misc/OutputDevice.h"
#include "Serialization/Archive.h"
#include "UObject/PropertyHelpers.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogProperty, Log, All);

// Helpers

namespace UE::CoreUObject::Private
{
	const TCHAR* SkipWhitespace(const TCHAR* Buffer)
	{
		while (*Buffer == ' ' || *Buffer == '\t' || *Buffer == '\r' || *Buffer == '\n')
		{
			++Buffer;
		}
		return Buffer;
	}

	const TCHAR* ReadToken(const TCHAR* Buffer, FString& OutToken, bool& bOutQuoted)
	{
		OutToken.Empty();
		Buffer = SkipWhitespace(Buffer);
		bOutQuoted = *Buffer == '"';
		if (!bOutQuoted)
		{
			while (*Buffer && *Buffer != ',' && *Buffer != ')' && *Buffer != ' ' && *Buffer != '\t' &&
				*Buffer != '\r' && *Buffer != '\n')
			{
				OutToken += *Buffer++;
			}
			return Buffer;
		}
		++Buffer;
		while (*Buffer && *Buffer != '"')
		{
			if (*Buffer == '\\' && Buffer[1])
			{
				++Buffer;
				switch (*Buffer)
				{
					case 'n':
						OutToken += '\n';
						break;
					case 't':
						OutToken += '\t';
						break;
					default:
						OutToken += *Buffer;
						break;
				}
				++Buffer;
				continue;
			}
			OutToken += *Buffer++;
		}
		if (*Buffer != '"')
		{
			return nullptr;
		}
		return Buffer + 1;
	}

	void AppendQuoted(FString& Out, const FString& Value)
	{
		Out += '"';
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			const TCHAR Char = Value[Index];
			switch (Char)
			{
				case '"':
					Out += TEXT("\\\"");
					break;
				case '\\':
					Out += TEXT("\\\\");
					break;
				case '\n':
					Out += TEXT("\\n");
					break;
				case '\t':
					Out += TEXT("\\t");
					break;
				default:
					Out += Char;
					break;
			}
		}
		Out += '"';
	}

	void ReportImportError(FOutputDevice* ErrorText, const FString& Message)
	{
		if (ErrorText)
		{
			ErrorText->Log(ELogVerbosity::Warning, Message);
		}
		else
		{
			UE_LOG(LogProperty, Warning, TEXT("%s"), *Message);
		}
	}

	const TCHAR* ParseNumber(const TCHAR* Buffer, bool bFloatingPoint, bool bSigned, int64& OutInteger,
		uint64& OutUnsigned, double& OutFloat)
	{
		Buffer = SkipWhitespace(Buffer);
		TCHAR* End = nullptr;
		if (bFloatingPoint)
		{
			OutFloat = FCString::Strtod(Buffer, &End);
		}
		else if (Buffer[0] == '0' && (Buffer[1] == 'x' || Buffer[1] == 'X'))
		{
			OutUnsigned = FCString::Strtoui64(Buffer + 2, &End, 16);
			OutInteger = int64(OutUnsigned);
			if (End == Buffer + 2)
			{
				return nullptr;
			}
		}
		else if (bSigned)
		{
			OutInteger = FCString::Strtoi64(Buffer, &End, 10);
			OutUnsigned = uint64(OutInteger);
		}
		else
		{
			OutUnsigned = FCString::Strtoui64(Buffer, &End, 10);
			OutInteger = int64(OutUnsigned);
		}
		if (!End || End == Buffer)
		{
			return nullptr;
		}
		// A float written as C++ writes it ("1.5f").
		if (bFloatingPoint && (*End == 'f' || *End == 'F'))
		{
			++End;
		}
		return End;
	}

	void AppendNumber(FString& Out, int64 Value)
	{
		Out += FString::Printf(TEXT("%lld"), (long long)Value);
	}

	void AppendUnsignedNumber(FString& Out, uint64 Value)
	{
		Out += FString::Printf(TEXT("%llu"), (unsigned long long)Value);
	}

	void AppendFloat(FString& Out, double Value, bool bIsFloat)
	{
		(void)bIsFloat;
		Out += FString::SanitizeFloat(Value);
	}
} // namespace UE::CoreUObject::Private

using namespace UE::CoreUObject::Private;

// FProperty

IMPLEMENT_FIELD(FProperty)

FProperty::FProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FField(InOwner, InName, InObjectFlags)
	, ArrayDim(1)
	, ElementSize(0)
	, PropertyFlags(CPF_None)
	, PropertyLinkNext(nullptr)
	, NextRef(nullptr)
	, DestructorLinkNext(nullptr)
	, PostConstructLinkNext(nullptr)
	, Offset_Internal(0)
{
}

FProperty::FProperty(
	FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
	: FField(InOwner, InName, InObjectFlags)
	, ArrayDim(1)
	, ElementSize(0)
	, PropertyFlags(InFlags)
	, PropertyLinkNext(nullptr)
	, NextRef(nullptr)
	, DestructorLinkNext(nullptr)
	, PostConstructLinkNext(nullptr)
	, Offset_Internal(InOffset)
{
	Init();
}

void FProperty::Init()
{
	if (UObject* OwnerObject = Owner.ToUObject())
	{
		CastChecked<UField>(OwnerObject)->AddCppProperty(this);
	}
	else if (FField* OwnerField = Owner.ToField())
	{
		OwnerField->AddCppProperty(this);
	}
}

bool FProperty::SameType(const FProperty* Other) const
{
	return Other && GetClass() == Other->GetClass();
}

const TCHAR* FProperty::ImportText(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	if (!Buffer)
	{
		return nullptr;
	}
	return ImportText_Internal(Buffer, Data, PortFlags, OwnerObject, ErrorText);
}

void FProperty::ExportText_InContainer(int32 Index, FString& ValueStr, const void* Data, const void* Delta,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	ExportTextItem(ValueStr, ContainerPtrToValuePtr<void>(Data, Index),
		Delta ? ContainerPtrToValuePtr<void>(Delta, Index) : nullptr, Parent, PortFlags, ExportRootScope);
}

void FProperty::LinkInternal(FArchive& Ar)
{
	(void)Ar;
}

void FProperty::CopyValuesInternal(void* Dest, void const* Src, int32 Count) const
{
	(void)Dest;
	(void)Src;
	(void)Count;
	UE_LOG(LogProperty, Fatal, TEXT("Property %s cannot copy values"), *GetFullName());
}

uint32 FProperty::GetValueTypeHashInternal(const void* Src) const
{
	(void)Src;
	UE_LOG(LogProperty, Fatal, TEXT("Property %s has no GetValueTypeHash"), *GetFullName());
	return 0;
}

void FProperty::ClearValueInternal(void* Data) const
{
	FMemory::Memzero(Data, ElementSize);
}

void FProperty::DestroyValueInternal(void* Dest) const
{
	(void)Dest;
}

void FProperty::InitializeValueInternal(void* Dest) const
{
	FMemory::Memzero(Dest, SIZE_T(ElementSize) * SIZE_T(ArrayDim));
}

// FNumericProperty

IMPLEMENT_FIELD(FNumericProperty)

FNumericProperty::FNumericProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FProperty(InOwner, InName, InObjectFlags)
{
}

FNumericProperty::FNumericProperty(
	FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
	: FProperty(InOwner, InName, InObjectFlags, InOffset, InFlags)
{
}

bool FNumericProperty::IsFloatingPoint() const
{
	return false;
}

bool FNumericProperty::IsInteger() const
{
	return true;
}

UEnum* FNumericProperty::GetIntPropertyEnum() const
{
	return nullptr;
}

void FNumericProperty::SetIntPropertyValue(void* Data, uint64 Value) const
{
	(void)Data;
	(void)Value;
	checkNoEntry();
}

void FNumericProperty::SetIntPropertyValue(void* Data, int64 Value) const
{
	(void)Data;
	(void)Value;
	checkNoEntry();
}

void FNumericProperty::SetFloatingPointPropertyValue(void* Data, double Value) const
{
	(void)Data;
	(void)Value;
	checkNoEntry();
}

void FNumericProperty::SetNumericPropertyValueFromString(void* Data, TCHAR const* Value) const
{
	int64 Integer = 0;
	uint64 Unsigned = 0;
	double Float = 0.0;
	if (ParseNumber(Value, IsFloatingPoint(), true, Integer, Unsigned, Float))
	{
		if (IsFloatingPoint())
		{
			SetFloatingPointPropertyValue(Data, Float);
		}
		else
		{
			SetIntPropertyValue(Data, Integer);
		}
	}
}

int64 FNumericProperty::GetSignedIntPropertyValue(void const* Data) const
{
	// Every numeric property type overrides this.
	(void)Data;
	return 0;
}

uint64 FNumericProperty::GetUnsignedIntPropertyValue(void const* Data) const
{
	// Every numeric property type overrides this.
	(void)Data;
	return 0;
}

double FNumericProperty::GetFloatingPointPropertyValue(void const* Data) const
{
	// Every numeric property type overrides this.
	(void)Data;
	return 0.0;
}

FString FNumericProperty::GetNumericPropertyValueToString(void const* Data) const
{
	// Every numeric property type overrides this.
	(void)Data;
	return FString();
}

const TCHAR* FNumericProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	(void)PortFlags;
	(void)OwnerObject;
	int64 Integer = 0;
	uint64 Unsigned = 0;
	double Float = 0.0;
	// Unsigned properties parse as unsigned, so uint64 values above INT64_MAX survive.
	const bool bSigned = IsFloatingPoint() ||
		GetClass()->HasAnyCastFlags(
			CASTCLASS_FInt8Property | CASTCLASS_FInt16Property | CASTCLASS_FIntProperty | CASTCLASS_FInt64Property);
	const TCHAR* End = ParseNumber(Buffer, IsFloatingPoint(), bSigned, Integer, Unsigned, Float);
	if (!End)
	{
		ReportImportError(ErrorText, FString::Printf(TEXT("%s: '%s' is not a number"), *GetName(), Buffer));
		return nullptr;
	}
	if (IsFloatingPoint())
	{
		SetFloatingPointPropertyValue(Data, Float);
	}
	else if (bSigned)
	{
		SetIntPropertyValue(Data, Integer);
	}
	else
	{
		SetIntPropertyValue(Data, Unsigned);
	}
	return End;
}

// The sized numeric types

#define LEON_IMPLEMENT_NUMERIC_PROPERTY(TClass, CppTypeName)                                                           \
	IMPLEMENT_FIELD(TClass)                                                                                            \
	TClass::TClass(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)                             \
		: Super(InOwner, InName, InObjectFlags)                                                                        \
	{                                                                                                                  \
		SetFieldClass(StaticClass());                                                                                  \
	}                                                                                                                  \
	TClass::TClass(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,             \
		EPropertyFlags InFlags)                                                                                        \
		: Super(InOwner, InName, InObjectFlags, InOffset, InFlags)                                                     \
	{                                                                                                                  \
		SetFieldClass(StaticClass());                                                                                  \
	}                                                                                                                  \
	FString TClass::GetCPPType() const                                                                                 \
	{                                                                                                                  \
		return TEXT(CppTypeName);                                                                                      \
	}

LEON_IMPLEMENT_NUMERIC_PROPERTY(FInt8Property, "int8")
LEON_IMPLEMENT_NUMERIC_PROPERTY(FInt16Property, "int16")
LEON_IMPLEMENT_NUMERIC_PROPERTY(FIntProperty, "int32")
LEON_IMPLEMENT_NUMERIC_PROPERTY(FInt64Property, "int64")
LEON_IMPLEMENT_NUMERIC_PROPERTY(FUInt16Property, "uint16")
LEON_IMPLEMENT_NUMERIC_PROPERTY(FUInt32Property, "uint32")
LEON_IMPLEMENT_NUMERIC_PROPERTY(FUInt64Property, "uint64")
LEON_IMPLEMENT_NUMERIC_PROPERTY(FFloatProperty, "float")
LEON_IMPLEMENT_NUMERIC_PROPERTY(FDoubleProperty, "double")

#undef LEON_IMPLEMENT_NUMERIC_PROPERTY

// FByteProperty

IMPLEMENT_FIELD(FByteProperty)

FByteProperty::FByteProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
	, Enum(nullptr)
{
	SetFieldClass(StaticClass());
}

FByteProperty::FByteProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
	EPropertyFlags InFlags, UEnum* InEnum)
	: Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
	, Enum(InEnum)
{
	SetFieldClass(StaticClass());
}

FString FByteProperty::GetCPPType() const
{
	return Enum ? FString::Printf(TEXT("TEnumAsByte<%s>"), *Enum->GetName()) : FString(TEXT("uint8"));
}

UEnum* FByteProperty::GetIntPropertyEnum() const
{
	return Enum;
}

void FByteProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	const uint8 Value = *(const uint8*)PropertyValue;
	if (Enum)
	{
		const FString NameString = Enum->GetNameStringByValue(Value);
		if (NameString.Len() > 0)
		{
			ValueStr += NameString;
			return;
		}
	}
	Super::ExportTextItem(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope);
}

const TCHAR* FByteProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	const TCHAR* Start = SkipWhitespace(Buffer);
	if (Enum && FChar::IsAlpha(*Start))
	{
		FString Token;
		bool bQuoted = false;
		const TCHAR* End = ReadToken(Start, Token, bQuoted);
		const int64 Value = Enum->GetValueByNameString(Token);
		if (Value == INDEX_NONE)
		{
			ReportImportError(ErrorText,
				FString::Printf(TEXT("%s: '%s' is not an entry of %s"), *GetName(), *Token, *Enum->GetName()));
			return nullptr;
		}
		*(uint8*)Data = uint8(Value);
		return End;
	}
	return Super::ImportText_Internal(Start, Data, PortFlags, OwnerObject, ErrorText);
}

// FBoolProperty

IMPLEMENT_FIELD(FBoolProperty)

FBoolProperty::FBoolProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FProperty(InOwner, InName, InObjectFlags)
	, FieldSize(0)
	, ByteOffset(0)
	, ByteMask(1)
	, FieldMask(1)
{
	SetFieldClass(StaticClass());
	SetBoolSize(sizeof(bool), true);
}

FBoolProperty::FBoolProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
	EPropertyFlags InFlags, uint32 InBitMask, uint32 InElementSize, bool bIsNativeBool)
	: FProperty(InOwner, InName, InObjectFlags, InOffset, InFlags)
	, FieldSize(0)
	, ByteOffset(0)
	, ByteMask(1)
	, FieldMask(1)
{
	SetFieldClass(StaticClass());
	SetBoolSize(InElementSize, bIsNativeBool, InBitMask);
}

void FBoolProperty::SetBoolSize(const uint32 InSize, const bool bIsNativeBool, const uint32 InBitMask)
{
	if (bIsNativeBool)
	{
		PropertyFlags |= CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor;
	}
	else
	{
		PropertyFlags &= ~(CPF_IsPlainOldData | CPF_ZeroConstructor);
		PropertyFlags |= CPF_NoDestructor;
	}
	const uint32 TestBitmask = InBitMask ? InBitMask : 1;
	ElementSize = int32(InSize);
	FieldSize = uint8(InSize);
	ByteOffset = 0;
	if (bIsNativeBool)
	{
		ByteMask = 1;
		FieldMask = 0xff;
	}
	else
	{
		// The byte of the storage that holds the bit (little-endian, as every Leon platform).
		for (ByteOffset = 0; ByteOffset < InSize; ++ByteOffset)
		{
			ByteMask = uint8(TestBitmask >> (8 * ByteOffset));
			if (ByteMask)
			{
				break;
			}
		}
		FieldMask = ByteMask;
	}
	checkf(FieldSize != 0 && FieldMask != 0 && ByteMask != 0, "Invalid bool property %s", *GetName());
}

FString FBoolProperty::GetCPPType() const
{
	if (IsNativeBool())
	{
		return TEXT("bool");
	}
	switch (FieldSize)
	{
		case 2:
			return TEXT("uint16");
		case 4:
			return TEXT("uint32");
		case 8:
			return TEXT("uint64");
		default:
			return TEXT("uint8");
	}
}

int32 FBoolProperty::GetMinAlignment() const
{
	return FieldSize ? int32(FieldSize) : 1;
}

void FBoolProperty::LinkInternal(FArchive& Ar)
{
	(void)Ar;
	check(FieldSize != 0);
	ElementSize = FieldSize;
	if (IsNativeBool())
	{
		PropertyFlags |= CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor;
	}
	else
	{
		PropertyFlags &= ~(CPF_IsPlainOldData | CPF_ZeroConstructor);
		PropertyFlags |= CPF_NoDestructor;
	}
	PropertyFlags |= CPF_HasGetValueTypeHash;
}

bool FBoolProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	(void)PortFlags;
	const bool bValueA = GetPropertyValue(A);
	const bool bValueB = B ? GetPropertyValue(B) : false;
	return bValueA == bValueB;
}

void FBoolProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	(void)Parent;
	(void)PortFlags;
	(void)ExportRootScope;
	ValueStr += GetPropertyValue(PropertyValue) ? TEXT("True") : TEXT("False");
}

const TCHAR* FBoolProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	(void)PortFlags;
	(void)OwnerObject;
	FString Token;
	bool bQuoted = false;
	const TCHAR* End = ReadToken(Buffer, Token, bQuoted);
	if (!End)
	{
		return nullptr;
	}
	if (Token == TEXT("1") || Token == TEXT("True") || Token == TEXT("Yes") || Token == TEXT("On"))
	{
		SetPropertyValue(Data, true);
	}
	else if (Token == TEXT("0") || Token == TEXT("False") || Token == TEXT("No") || Token == TEXT("Off"))
	{
		SetPropertyValue(Data, false);
	}
	else
	{
		ReportImportError(ErrorText, FString::Printf(TEXT("%s: '%s' is not a bool"), *GetName(), *Token));
		return nullptr;
	}
	return End;
}

void FBoolProperty::CopyCompleteValueToScriptVM(void* Dest, void const* Src) const
{
	*(uint32*)Dest = GetPropertyValue(Src) ? 1u : 0u;
}

void FBoolProperty::CopyValuesInternal(void* Dest, void const* Src, int32 Count) const
{
	check(FieldSize != 0 && !IsNativeBool());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const uint8* SrcByte = (const uint8*)Src + Index * FieldSize + ByteOffset;
		uint8* DestByte = (uint8*)Dest + Index * FieldSize + ByteOffset;
		*DestByte = (*DestByte & ~FieldMask) | (*SrcByte & FieldMask);
	}
}

void FBoolProperty::ClearValueInternal(void* Data) const
{
	uint8* ByteValue = (uint8*)Data + ByteOffset;
	*ByteValue &= ~FieldMask;
}

void FBoolProperty::InitializeValueInternal(void* Data) const
{
	uint8* ByteValue = (uint8*)Data + ByteOffset;
	*ByteValue &= ~FieldMask;
}

uint32 FBoolProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetPropertyValue(Src) ? 1u : 0u;
}

// FStrProperty

IMPLEMENT_FIELD(FStrProperty)

FStrProperty::FStrProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
{
	SetFieldClass(StaticClass());
}

FStrProperty::FStrProperty(
	FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
	: Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
{
	SetFieldClass(StaticClass());
}

FString FStrProperty::GetCPPType() const
{
	return TEXT("FString");
}

void FStrProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	(void)Parent;
	(void)PortFlags;
	(void)ExportRootScope;
	AppendQuoted(ValueStr, *(const FString*)PropertyValue);
}

const TCHAR* FStrProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	(void)OwnerObject;
	const TCHAR* Start = SkipWhitespace(Buffer);
	if (*Start != '"' && !(PortFlags & PPF_Delimited))
	{
		// An unquoted value is the whole text (UE).
		*(FString*)Data = Start;
		return Start + FCString::Strlen(Start);
	}
	FString Token;
	bool bQuoted = false;
	const TCHAR* End = ReadToken(Start, Token, bQuoted);
	if (!End)
	{
		ReportImportError(ErrorText, FString::Printf(TEXT("%s: missing closing '\"' in %s"), *GetName(), Start));
		return nullptr;
	}
	*(FString*)Data = Token;
	return End;
}

// FNameProperty

IMPLEMENT_FIELD(FNameProperty)

FNameProperty::FNameProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
{
	SetFieldClass(StaticClass());
}

FNameProperty::FNameProperty(
	FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
	: Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
{
	SetFieldClass(StaticClass());
}

FString FNameProperty::GetCPPType() const
{
	return TEXT("FName");
}

void FNameProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	(void)Parent;
	(void)ExportRootScope;
	const FName& Value = *(const FName*)PropertyValue;
	if (PortFlags & PPF_Delimited)
	{
		AppendQuoted(ValueStr, Value.ToString());
	}
	else
	{
		ValueStr += Value.ToString();
	}
}

const TCHAR* FNameProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	(void)PortFlags;
	(void)OwnerObject;
	FString Token;
	bool bQuoted = false;
	const TCHAR* End = ReadToken(Buffer, Token, bQuoted);
	if (!End)
	{
		ReportImportError(ErrorText, FString::Printf(TEXT("%s: missing closing '\"'"), *GetName()));
		return nullptr;
	}
	*(FName*)Data = FName(*Token);
	return End;
}

// FTextProperty

IMPLEMENT_FIELD(FTextProperty)

FTextProperty::FTextProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
{
	SetFieldClass(StaticClass());
}

FTextProperty::FTextProperty(
	FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
	: Super(InOwner, InName, InObjectFlags, InOffset, InFlags)
{
	SetFieldClass(StaticClass());
}

FString FTextProperty::GetCPPType() const
{
	return TEXT("FText");
}

bool FTextProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	(void)PortFlags;
	const FText& TextA = *(const FText*)A;
	return B ? TextA.EqualTo(*(const FText*)B) : TextA.IsEmpty();
}

void FTextProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	(void)Parent;
	(void)PortFlags;
	(void)ExportRootScope;
	AppendQuoted(ValueStr, ((const FText*)PropertyValue)->ToString());
}

const TCHAR* FTextProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	(void)OwnerObject;
	FString String;
	const TCHAR* Start = SkipWhitespace(Buffer);
	if (*Start != '"' && !(PortFlags & PPF_Delimited))
	{
		*(FText*)Data = FText::FromString(FString(Start));
		return Start + FCString::Strlen(Start);
	}
	bool bQuoted = false;
	const TCHAR* TokenEnd = ReadToken(Start, String, bQuoted);
	if (!TokenEnd)
	{
		ReportImportError(ErrorText, FString::Printf(TEXT("%s: missing closing '\"'"), *GetName()));
		return nullptr;
	}
	*(FText*)Data = FText::FromString(String);
	return TokenEnd;
}

uint32 FTextProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(((const FText*)Src)->ToString());
}

// FEnumProperty

IMPLEMENT_FIELD(FEnumProperty)

FEnumProperty::FEnumProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FProperty(InOwner, InName, InObjectFlags)
	, UnderlyingProp(nullptr)
	, Enum(nullptr)
{
	SetFieldClass(StaticClass());
}

FEnumProperty::FEnumProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
	EPropertyFlags InFlags, UEnum* InEnum)
	: FProperty(InOwner, InName, InObjectFlags, InOffset, InFlags)
	, UnderlyingProp(nullptr)
	, Enum(InEnum)
{
	SetFieldClass(StaticClass());
}

FEnumProperty::~FEnumProperty()
{
	delete UnderlyingProp;
}

void FEnumProperty::AddCppProperty(FProperty* Inner)
{
	checkf(!UnderlyingProp, "Enum property %s already has an underlying property", *GetName());
	// Called from the inner property's constructor: its field class is only known once it is built (LinkInternal
	// checks it).
	UnderlyingProp = static_cast<FNumericProperty*>(Inner);
}

FString FEnumProperty::GetCPPType() const
{
	return Enum ? Enum->CppType : FString(TEXT("uint8"));
}

int32 FEnumProperty::GetMinAlignment() const
{
	return UnderlyingProp->GetMinAlignment();
}

void FEnumProperty::LinkInternal(FArchive& Ar)
{
	checkf(UnderlyingProp, "Enum property %s has no underlying property", *GetName());
	CastFieldChecked<FNumericProperty>(UnderlyingProp);
	UnderlyingProp->LinkWithoutChangingOffset(Ar);
	ElementSize = UnderlyingProp->ElementSize;
	PropertyFlags |= CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor | CPF_HasGetValueTypeHash;
}

bool FEnumProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	return UnderlyingProp->Identical(A, B, PortFlags);
}

void FEnumProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	const int64 Value = UnderlyingProp->GetSignedIntPropertyValue(PropertyValue);
	const FString NameString = Enum ? Enum->GetNameStringByValue(Value) : FString();
	if (NameString.Len() > 0)
	{
		ValueStr += NameString;
	}
	else
	{
		UnderlyingProp->ExportTextItem(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, ExportRootScope);
	}
}

const TCHAR* FEnumProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	const TCHAR* Start = SkipWhitespace(Buffer);
	if (Enum && FChar::IsAlpha(*Start))
	{
		FString Token;
		bool bQuoted = false;
		const TCHAR* End = ReadToken(Start, Token, bQuoted);
		const int64 Value = Enum->GetValueByNameString(Token);
		if (Value == INDEX_NONE)
		{
			ReportImportError(ErrorText,
				FString::Printf(TEXT("%s: '%s' is not an entry of %s"), *GetName(), *Token, *Enum->GetName()));
			return nullptr;
		}
		UnderlyingProp->SetIntPropertyValue(Data, Value);
		return End;
	}
	return UnderlyingProp->ImportText(Start, Data, PortFlags, OwnerObject, ErrorText);
}

uint32 FEnumProperty::GetValueTypeHashInternal(const void* Src) const
{
	return UnderlyingProp->GetValueTypeHash(Src);
}
