#include "UObject/PropertyTag.h"

#include "UObject/UnrealType.h"

FPropertyTag::FPropertyTag(FProperty* Property, int32 InIndex, uint8* Value)
	: Prop(Property)
	, Type(Property->GetID())
	, Name(Property->GetFName())
	, ArrayIndex(InIndex)
{
	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		StructName = StructProperty->Struct->GetFName();
	}
	else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		if (UEnum* Enum = EnumProperty->GetEnum())
		{
			EnumName = Enum->GetFName();
		}
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		if (ByteProperty->Enum)
		{
			EnumName = ByteProperty->Enum->GetFName();
		}
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		InnerType = ArrayProperty->Inner->GetID();
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		InnerType = SetProperty->ElementProp->GetID();
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		InnerType = MapProperty->KeyProp->GetID();
		ValueType = MapProperty->ValueProp->GetID();
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		BoolVal = BoolProperty->GetPropertyValue(Value) ? 1 : 0;
	}
}

FArchive& operator<<(FArchive& Ar, FPropertyTag& Tag)
{
	Ar << Tag.Name;
	if (Tag.Name.IsNone())
	{
		return Ar;
	}
	Ar << Tag.Type;
	if (Ar.IsSaving())
	{
		Tag.SizeOffset = Ar.Tell();
	}
	Ar << Tag.Size << Tag.ArrayIndex;
	if (Ar.IsLoading() && (Tag.Size < 0 || Tag.ArrayIndex < 0))
	{
		Ar.SetCriticalError();
		return Ar;
	}

	// The type details (UE 4.27's order; no struct GUID).
	if (Tag.Type.GetNumber() == NAME_NO_NUMBER_INTERNAL)
	{
		if (Tag.Type == NAME_StructProperty)
		{
			Ar << Tag.StructName;
		}
		else if (Tag.Type == NAME_BoolProperty)
		{
			Ar << Tag.BoolVal;
		}
		else if (Tag.Type == NAME_ByteProperty || Tag.Type == NAME_EnumProperty)
		{
			Ar << Tag.EnumName;
		}
		else if (Tag.Type == NAME_ArrayProperty || Tag.Type == NAME_SetProperty)
		{
			Ar << Tag.InnerType;
		}
		else if (Tag.Type == NAME_MapProperty)
		{
			Ar << Tag.InnerType;
			Ar << Tag.ValueType;
		}
	}

	Ar << Tag.HasPropertyGuid;
	if (Tag.HasPropertyGuid)
	{
		// UE writes the property GUID here; Leon never does, but reads past one.
		uint32 PropertyGuid[4] = {};
		Ar << PropertyGuid[0] << PropertyGuid[1] << PropertyGuid[2] << PropertyGuid[3];
	}
	return Ar;
}

void FPropertyTag::SerializeTaggedProperty(FArchive& Ar, FProperty* Property, uint8* Value, const uint8* Defaults) const
{
	if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		// The value lives in the tag.
		if (Ar.IsLoading())
		{
			BoolProperty->SetPropertyValue(Value, BoolVal != 0);
		}
		return;
	}
	Property->SerializeItem(Ar, Value, Defaults);
}
