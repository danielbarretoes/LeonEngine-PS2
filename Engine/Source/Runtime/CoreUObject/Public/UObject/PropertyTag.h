#pragma once

// The header of one saved property (UE: UObject/PropertyTag.h).

#include "CoreMinimal.h"
#include "Serialization/Archive.h"

class FProperty;

/**
 * Describes one property value in tagged serialization (UE: FPropertyTag): its name, its type and the byte size of
 * the value that follows, so a loader can skip a value it does not understand (a renamed or removed property, a type
 * it cannot convert). A NAME_None name ends the list. The type details a loader checks before reading the value:
 * StructName (struct properties), EnumName (enum and enum byte properties), InnerType (arrays and sets; a map's key)
 * and ValueType (a map's value). A bool's value is BoolVal, with no value bytes.
 */
struct COREUOBJECT_API FPropertyTag
{
	/** The property (transient). */
	FProperty* Prop = nullptr;

	/** The property's type: its field class name ("IntProperty", "StructProperty"). */
	FName Type;

	/** The value of a BoolProperty. */
	uint8 BoolVal = 0;

	/** The property's name. */
	FName Name;

	/** A StructProperty's struct ("Vector", "PackageTestInner"). */
	FName StructName;

	/** An EnumProperty's enum, or a ByteProperty's enum (NAME_None for a plain byte). */
	FName EnumName;

	/** An ArrayProperty's or SetProperty's element type, a MapProperty's key type. */
	FName InnerType;

	/** A MapProperty's value type. */
	FName ValueType;

	/** Size of the value in bytes. */
	int32 Size = 0;

	/** The element of a C array property, 0 otherwise. */
	int32 ArrayIndex = INDEX_NONE;

	/** Where Size was written (transient: the saver patches it once the value is written). */
	int64 SizeOffset = INDEX_NONE;

	/** Always 0: Leon has no property GUIDs (UE: blueprint property renames). */
	uint8 HasPropertyGuid = 0;

	FPropertyTag() = default;

	/** The tag of element InIndex of Property, whose value is at Value (UE). */
	FPropertyTag(FProperty* Property, int32 InIndex, uint8* Value);

	/**
	 * Name, then (unless it is NAME_None) Type, Size, ArrayIndex, the type details of Type and HasPropertyGuid, in UE
	 * 4.27's order. Saving records SizeOffset.
	 */
	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FPropertyTag& Tag);

	/** Loads or saves the value: BoolVal for a bool, FProperty::SerializeItem otherwise (UE). */
	void SerializeTaggedProperty(FArchive& Ar, FProperty* Property, uint8* Value, const uint8* Defaults) const;
};
