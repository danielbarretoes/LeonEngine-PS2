// FStructProperty (UE: PropertyStruct.cpp).

#include "Misc/Char.h"
#include "Serialization/Archive.h"
#include "UObject/PropertyHelpers.h"
#include "UObject/UnrealType.h"

using namespace UE::CoreUObject::Private;

IMPLEMENT_FIELD(FStructProperty)

FStructProperty::FStructProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FProperty(InOwner, InName, InObjectFlags)
	, Struct(nullptr)
{
	SetFieldClass(StaticClass());
}

FStructProperty::FStructProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
	EPropertyFlags InFlags, UScriptStruct* InStruct)
	: FProperty(InOwner, InName, InObjectFlags, InOffset, InFlags)
	, Struct(InStruct)
{
	SetFieldClass(StaticClass());
	ElementSize = Struct ? Struct->GetStructureSize() : 0;
}

FString FStructProperty::GetCPPType() const
{
	return Struct ? Struct->GetStructCPPName() : FString(TEXT("FStruct"));
}

int32 FStructProperty::GetMinAlignment() const
{
	return Struct->GetMinAlignment();
}

bool FStructProperty::ContainsObjectReference() const
{
	return Struct && Struct->RefLink != nullptr;
}

bool FStructProperty::SameType(const FProperty* Other) const
{
	return FProperty::SameType(Other) && Struct == ((const FStructProperty*)Other)->Struct;
}

void FStructProperty::LinkInternal(FArchive& Ar)
{
	(void)Ar;
	checkf(Struct, "Struct property %s has no struct", *GetName());
	ElementSize = Struct->GetStructureSize();
	const EStructFlags StructFlags = Struct->StructFlags;
	if (StructFlags & STRUCT_ZeroConstructor)
	{
		PropertyFlags |= CPF_ZeroConstructor;
	}
	if (StructFlags & STRUCT_IsPlainOldData)
	{
		PropertyFlags |= CPF_IsPlainOldData | CPF_NoDestructor;
	}
	else
	{
		PropertyFlags &= ~CPF_IsPlainOldData;
	}
	if (StructFlags & STRUCT_NoDestructor)
	{
		PropertyFlags |= CPF_NoDestructor;
	}
	if (Struct->GetCppStructOps() && Struct->GetCppStructOps()->HasGetTypeHash())
	{
		PropertyFlags |= CPF_HasGetValueTypeHash;
	}
}

bool FStructProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	return Struct->CompareScriptStruct(A, B, PortFlags);
}

void FStructProperty::CopyValuesInternal(void* Dest, void const* Src, int32 Count) const
{
	Struct->CopyScriptStruct(Dest, Src, Count);
}

void FStructProperty::ClearValueInternal(void* Data) const
{
	Struct->ClearScriptStruct(Data, 1);
}

void FStructProperty::DestroyValueInternal(void* Dest) const
{
	Struct->DestroyStruct(Dest, ArrayDim);
}

void FStructProperty::InitializeValueInternal(void* Dest) const
{
	Struct->InitializeStruct(Dest, ArrayDim);
}

uint32 FStructProperty::GetValueTypeHashInternal(const void* Src) const
{
	return Struct->GetStructTypeHash(Src);
}

void FStructProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	// (Name=Value,Name2=Value2), every property, values delimited (UE writes the ones that differ from the defaults).
	ValueStr += TEXT("(");
	bool bFirst = true;
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			if (!bFirst)
			{
				ValueStr += TEXT(",");
			}
			bFirst = false;
			ValueStr += Property->GetName();
			if (Property->ArrayDim > 1)
			{
				ValueStr += FString::Printf(TEXT("[%d]"), Index);
			}
			ValueStr += TEXT("=");
			Property->ExportText_InContainer(
				Index, ValueStr, PropertyValue, nullptr, Parent, PortFlags | PPF_Delimited, ExportRootScope);
		}
	}
	ValueStr += TEXT(")");
}

const TCHAR* FStructProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	Buffer = SkipWhitespace(Buffer);
	if (*Buffer != '(')
	{
		ReportImportError(ErrorText, FString::Printf(TEXT("%s: expected '(' to start a struct value"), *GetName()));
		return nullptr;
	}
	++Buffer;
	while (true)
	{
		Buffer = SkipWhitespace(Buffer);
		if (*Buffer == ')')
		{
			return Buffer + 1;
		}
		FString MemberName;
		while (FChar::IsAlnum(*Buffer) || *Buffer == '_')
		{
			MemberName += *Buffer++;
		}
		int32 ArrayIndex = 0;
		if (*Buffer == '[')
		{
			++Buffer;
			ArrayIndex = 0;
			while (FChar::IsDigit(*Buffer))
			{
				ArrayIndex = ArrayIndex * 10 + (*Buffer++ - '0');
			}
			if (*Buffer++ != ']')
			{
				ReportImportError(
					ErrorText, FString::Printf(TEXT("%s: missing ']' after %s"), *GetName(), *MemberName));
				return nullptr;
			}
		}
		Buffer = SkipWhitespace(Buffer);
		if (*Buffer != '=')
		{
			ReportImportError(ErrorText, FString::Printf(TEXT("%s: expected '=' after %s"), *GetName(), *MemberName));
			return nullptr;
		}
		Buffer = SkipWhitespace(Buffer + 1);
		FProperty* Property = Struct->FindPropertyByName(FName(*MemberName, FNAME_Find));
		if (!Property || ArrayIndex < 0 || ArrayIndex >= Property->ArrayDim)
		{
			ReportImportError(ErrorText,
				FString::Printf(TEXT("%s: %s has no member %s"), *GetName(), *Struct->GetName(), *MemberName));
			return nullptr;
		}
		Buffer = Property->ImportText(Buffer, Property->ContainerPtrToValuePtr<void>(Data, ArrayIndex),
			PortFlags | PPF_Delimited, OwnerObject, ErrorText);
		if (!Buffer)
		{
			return nullptr;
		}
		Buffer = SkipWhitespace(Buffer);
		if (*Buffer == ',')
		{
			++Buffer;
		}
		else if (*Buffer != ')')
		{
			ReportImportError(ErrorText, FString::Printf(TEXT("%s: expected ',' or ')'"), *GetName()));
			return nullptr;
		}
	}
}
