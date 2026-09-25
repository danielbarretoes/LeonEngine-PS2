// The object reference property types (UE: PropertyBaseObject.cpp, PropertyObject.cpp, PropertyClass.cpp,
// PropertyWeakObjectPtr.cpp, PropertySoftObjectPtr.cpp, PropertySoftClassPtr.cpp).

#include "Misc/CString.h"
#include "UObject/Package.h"
#include "UObject/PropertyHelpers.h"
#include "UObject/UnrealType.h"

using namespace UE::CoreUObject::Private;

// FObjectPropertyBase

IMPLEMENT_FIELD(FObjectPropertyBase)

FObjectPropertyBase::FObjectPropertyBase(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FProperty(InOwner, InName, InObjectFlags)
	, PropertyClass(nullptr)
{
}

FObjectPropertyBase::FObjectPropertyBase(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags,
	int32 InOffset, EPropertyFlags InFlags, UClass* InClass)
	: FProperty(InOwner, InName, InObjectFlags, InOffset, InFlags)
	, PropertyClass(InClass)
{
}

bool FObjectPropertyBase::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	(void)PortFlags;
	const UObject* ObjectA = A ? GetObjectPropertyValue(A) : nullptr;
	const UObject* ObjectB = B ? GetObjectPropertyValue(B) : nullptr;
	return ObjectA == ObjectB;
}

bool FObjectPropertyBase::SameType(const FProperty* Other) const
{
	return FProperty::SameType(Other) && PropertyClass == ((const FObjectPropertyBase*)Other)->PropertyClass;
}

void FObjectPropertyBase::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	(void)Parent;
	(void)PortFlags;
	(void)ExportRootScope;
	const UObject* Object = GetObjectPropertyValue(PropertyValue);
	if (!Object)
	{
		ValueStr += TEXT("None");
		return;
	}
	// ClassName'PathName' (UE).
	ValueStr += Object->GetClass()->GetName();
	ValueStr += TEXT("'");
	ValueStr += Object->GetPathName();
	ValueStr += TEXT("'");
}

UObject* FObjectPropertyBase::FindImportedObject(
	const FProperty* Property, UObject* OwnerObject, UClass* ObjectClass, UClass* RequiredMetaClass, const TCHAR* Text)
{
	(void)Property;
	(void)RequiredMetaClass;
	UObject* Result = StaticFindObject(ObjectClass, ANY_PACKAGE, Text);
	if (!Result && OwnerObject)
	{
		// A name relative to the owner's package ("Asset" in /Game/Pkg).
		Result = StaticFindObject(ObjectClass, OwnerObject->GetOutermost(), Text);
	}
	return Result;
}

bool FObjectPropertyBase::AllowObjectTypeReference(UObject* Object) const
{
	return !Object || !PropertyClass || Object->IsA(PropertyClass);
}

const TCHAR* FObjectPropertyBase::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	(void)PortFlags;
	FString Token;
	bool bQuoted = false;
	const TCHAR* End = ReadToken(Buffer, Token, bQuoted);
	if (!End)
	{
		ReportImportError(ErrorText, FString::Printf(TEXT("%s: missing closing '\"'"), *GetName()));
		return nullptr;
	}
	if (Token == TEXT("None") || Token.Len() == 0)
	{
		SetObjectPropertyValue(Data, nullptr);
		return End;
	}
	// ClassName'PathName' or a plain path.
	FString Path = Token;
	int32 QuoteIndex = INDEX_NONE;
	if (Token.FindChar('\'', QuoteIndex) && Token.Len() > QuoteIndex + 1 && Token[Token.Len() - 1] == '\'')
	{
		Path = Token.Mid(QuoteIndex + 1, Token.Len() - QuoteIndex - 2);
	}
	UObject* Object = FindImportedObject(this, OwnerObject, PropertyClass, nullptr, *Path);
	if (!Object || !AllowObjectTypeReference(Object))
	{
		ReportImportError(ErrorText,
			FString::Printf(TEXT("%s: no %s named '%s'"), *GetName(),
				PropertyClass ? *PropertyClass->GetName() : TEXT("object"), *Path));
		return nullptr;
	}
	SetObjectPropertyValue(Data, Object);
	return End;
}

// FObjectProperty

IMPLEMENT_FIELD(FObjectProperty)

FObjectProperty::FObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
{
	SetFieldClass(StaticClass());
}

FObjectProperty::FObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
	EPropertyFlags InFlags, UClass* InClass)
	: Super(InOwner, InName, InObjectFlags, InOffset, InFlags, InClass)
{
	SetFieldClass(StaticClass());
}

FString FObjectProperty::GetCPPType() const
{
	return FString::Printf(TEXT("U%s*"), PropertyClass ? *PropertyClass->GetName() : TEXT("Object"));
}

UObject* FObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return *(UObject* const*)PropertyValueAddress;
}

void FObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	*(UObject**)PropertyValueAddress = Value;
}

// FClassProperty

IMPLEMENT_FIELD(FClassProperty)

FClassProperty::FClassProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FObjectProperty(InOwner, InName, InObjectFlags)
	, MetaClass(nullptr)
{
	SetFieldClass(StaticClass());
}

FClassProperty::FClassProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
	EPropertyFlags InFlags, UClass* InMetaClass, UClass* InClassType)
	: FObjectProperty(InOwner, InName, InObjectFlags, InOffset, InFlags, InClassType)
	, MetaClass(InMetaClass)
{
	SetFieldClass(StaticClass());
}

FString FClassProperty::GetCPPType() const
{
	if (HasAnyPropertyFlags(CPF_UObjectWrapper) && MetaClass)
	{
		return FString::Printf(TEXT("TSubclassOf<U%s>"), *MetaClass->GetName());
	}
	return TEXT("UClass*");
}

bool FClassProperty::SameType(const FProperty* Other) const
{
	return FObjectProperty::SameType(Other) && MetaClass == ((const FClassProperty*)Other)->MetaClass;
}

bool FClassProperty::AllowObjectTypeReference(UObject* Object) const
{
	if (!FObjectProperty::AllowObjectTypeReference(Object))
	{
		return false;
	}
	const UClass* ClassObject = Cast<UClass>(Object);
	return !Object || !MetaClass || (ClassObject && ClassObject->IsChildOf(MetaClass));
}

// FWeakObjectProperty

IMPLEMENT_FIELD(FWeakObjectProperty)

FWeakObjectProperty::FWeakObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
{
	SetFieldClass(StaticClass());
}

FWeakObjectProperty::FWeakObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags,
	int32 InOffset, EPropertyFlags InFlags, UClass* InClass)
	: Super(InOwner, InName, InObjectFlags, InOffset, InFlags, InClass)
{
	SetFieldClass(StaticClass());
}

FString FWeakObjectProperty::GetCPPType() const
{
	return FString::Printf(TEXT("TWeakObjectPtr<U%s>"), PropertyClass ? *PropertyClass->GetName() : TEXT("Object"));
}

UObject* FWeakObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return ((const FWeakObjectPtr*)PropertyValueAddress)->Get();
}

void FWeakObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	*(FWeakObjectPtr*)PropertyValueAddress = Value;
}

bool FWeakObjectProperty::ContainsObjectReference(
	TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType) const
{
	(void)EncounteredStructProps;
	return !!(InReferenceType & EPropertyObjectReferenceType::Weak);
}

// FSoftObjectProperty

IMPLEMENT_FIELD(FSoftObjectProperty)

FSoftObjectProperty::FSoftObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: Super(InOwner, InName, InObjectFlags)
{
	SetFieldClass(StaticClass());
}

FSoftObjectProperty::FSoftObjectProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags,
	int32 InOffset, EPropertyFlags InFlags, UClass* InClass)
	: Super(InOwner, InName, InObjectFlags, InOffset, InFlags, InClass)
{
	SetFieldClass(StaticClass());
}

FString FSoftObjectProperty::GetCPPType() const
{
	return FString::Printf(TEXT("TSoftObjectPtr<U%s>"), PropertyClass ? *PropertyClass->GetName() : TEXT("Object"));
}

UObject* FSoftObjectProperty::GetObjectPropertyValue(const void* PropertyValueAddress) const
{
	return ((const FSoftObjectPtr*)PropertyValueAddress)->Get();
}

void FSoftObjectProperty::SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
{
	*(FSoftObjectPtr*)PropertyValueAddress = Value;
}

bool FSoftObjectProperty::ContainsObjectReference(
	TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType) const
{
	(void)EncounteredStructProps;
	return !!(InReferenceType & EPropertyObjectReferenceType::Weak);
}

bool FSoftObjectProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	(void)PortFlags;
	const FSoftObjectPtr Default;
	const FSoftObjectPtr& ValueA = *(const FSoftObjectPtr*)A;
	const FSoftObjectPtr& ValueB = B ? *(const FSoftObjectPtr*)B : Default;
	return ValueA == ValueB;
}

void FSoftObjectProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	(void)Parent;
	(void)PortFlags;
	(void)ExportRootScope;
	const FSoftObjectPtr& Value = *(const FSoftObjectPtr*)PropertyValue;
	ValueStr += Value.IsNull() ? FString(TEXT("None")) : Value.ToString();
}

const TCHAR* FSoftObjectProperty::ImportText_Internal(
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
	FSoftObjectPtr& Value = *(FSoftObjectPtr*)Data;
	if (Token == TEXT("None") || Token.Len() == 0)
	{
		Value.Reset();
	}
	else
	{
		// A soft reference keeps its path even when the object is not in memory (P11 loads it).
		Value = FSoftObjectPath(Token);
	}
	return End;
}

// FSoftClassProperty

IMPLEMENT_FIELD(FSoftClassProperty)

FSoftClassProperty::FSoftClassProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FSoftObjectProperty(InOwner, InName, InObjectFlags)
	, MetaClass(nullptr)
{
	SetFieldClass(StaticClass());
}

FSoftClassProperty::FSoftClassProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags,
	int32 InOffset, EPropertyFlags InFlags, UClass* InMetaClass)
	: FSoftObjectProperty(InOwner, InName, InObjectFlags, InOffset, InFlags, UClass::StaticClass())
	, MetaClass(InMetaClass)
{
	SetFieldClass(StaticClass());
}

FString FSoftClassProperty::GetCPPType() const
{
	return FString::Printf(TEXT("TSoftClassPtr<U%s>"), MetaClass ? *MetaClass->GetName() : TEXT("Object"));
}

bool FSoftClassProperty::SameType(const FProperty* Other) const
{
	return FSoftObjectProperty::SameType(Other) && MetaClass == ((const FSoftClassProperty*)Other)->MetaClass;
}
