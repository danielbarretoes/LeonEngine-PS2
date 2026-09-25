#pragma once

// The macros of the generated exec thunks: they read a UFUNCTION's parameters from the call frame and call the C++
// function (UE: UObject/ScriptMacros.h). Included by every .generated.h, which also brings UObject and the property
// types.

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Script.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"

/** A parameter read by value into a local (UE: PARAM_PASSED_BY_VAL). */
#define PARAM_PASSED_BY_VAL(ParamName, PropertyType, ParamType)                                                        \
	ParamType ParamName = PropertyType::GetDefaultPropertyValue();                                                     \
	Stack.StepCompiledIn<PropertyType>(&ParamName);

/** A parameter read by value into a zeroed local (UE: PARAM_PASSED_BY_VAL_ZEROED). */
#define PARAM_PASSED_BY_VAL_ZEROED(ParamName, PropertyType, ParamType)                                                 \
	ParamType ParamName = (ParamType)0;                                                                                \
	Stack.StepCompiledIn<PropertyType>(&ParamName);

/** A struct or container parameter read by value (UE: PARAM_PASSED_BY_VAL for non-property types). */
#define PARAM_PASSED_BY_STRUCT(ParamName, PropertyType, ParamType)                                                     \
	ParamType ParamName;                                                                                               \
	Stack.StepCompiledIn<PropertyType>(&ParamName);

/** A by-reference parameter, used in place (UE: PARAM_PASSED_BY_REF). */
#define PARAM_PASSED_BY_REF(ParamName, PropertyType, ParamType)                                                        \
	ParamType ParamName##Temp;                                                                                         \
	ParamType& ParamName = Stack.StepCompiledInRef<PropertyType, ParamType>(&ParamName##Temp);

/** A numeric, string, name or text parameter: P_GET_PROPERTY(FIntProperty, Z_Param_Count) (UE). */
#define P_GET_PROPERTY(PropertyType, ParamName)                                                                        \
	PropertyType::TCppType ParamName = PropertyType::GetDefaultPropertyValue();                                        \
	Stack.StepCompiledIn<PropertyType>(&ParamName);

#define P_GET_PROPERTY_REF(PropertyType, ParamName)                                                                    \
	PropertyType::TCppType ParamName##Temp = PropertyType::GetDefaultPropertyValue();                                  \
	PropertyType::TCppType& ParamName = Stack.StepCompiledInRef<PropertyType, PropertyType::TCppType>(&ParamName##Temp);

/** A bool parameter; the frame hands it over as a uint32 (UE). */
#define P_GET_UBOOL(ParamName)                                                                                         \
	uint32 ParamName##32 = 0;                                                                                          \
	bool ParamName = false;                                                                                            \
	Stack.StepCompiledIn<FBoolProperty>(&ParamName##32);                                                               \
	ParamName = !!ParamName##32;

/** An enum class parameter (UE). */
#define P_GET_ENUM(EnumType, ParamName)                                                                                \
	EnumType ParamName = (EnumType)0;                                                                                  \
	Stack.StepCompiledIn<FEnumProperty>(&ParamName);

/** A UObject* (or TSubclassOf, as UClass*) parameter (UE). */
#define P_GET_OBJECT(ObjectType, ParamName) PARAM_PASSED_BY_VAL_ZEROED(ParamName, FObjectPropertyBase, ObjectType*)

#define P_GET_SOFTOBJECT(ObjectType, ParamName) PARAM_PASSED_BY_STRUCT(ParamName, FSoftObjectProperty, ObjectType)
#define P_GET_SOFTCLASS(ObjectType, ParamName) PARAM_PASSED_BY_STRUCT(ParamName, FSoftClassProperty, ObjectType)
#define P_GET_WEAKOBJECT(ObjectType, ParamName) PARAM_PASSED_BY_STRUCT(ParamName, FWeakObjectProperty, ObjectType)

#define P_GET_STRUCT(StructType, ParamName) PARAM_PASSED_BY_STRUCT(ParamName, FStructProperty, StructType)
#define P_GET_STRUCT_REF(StructType, ParamName) PARAM_PASSED_BY_REF(ParamName, FStructProperty, StructType)

#define P_GET_TARRAY(ElementType, ParamName) PARAM_PASSED_BY_STRUCT(ParamName, FArrayProperty, TArray<ElementType>)
#define P_GET_TARRAY_REF(ElementType, ParamName) PARAM_PASSED_BY_REF(ParamName, FArrayProperty, TArray<ElementType>)

#define P_GET_TSET(ElementType, ParamName) PARAM_PASSED_BY_STRUCT(ParamName, FSetProperty, TSet<ElementType>)
#define P_GET_TSET_REF(ElementType, ParamName) PARAM_PASSED_BY_REF(ParamName, FSetProperty, TSet<ElementType>)

/** TMap parameters: the key and value types are two macro arguments (UE). */
#define P_GET_TMAP(KeyType, ValueType, ParamName)                                                                      \
	TMap<KeyType, ValueType> ParamName;                                                                                \
	Stack.StepCompiledIn<FMapProperty>(&ParamName);
#define P_GET_TMAP_REF(KeyType, ValueType, ParamName)                                                                  \
	TMap<KeyType, ValueType> ParamName##Temp;                                                                          \
	TMap<KeyType, ValueType>& ParamName =                                                                              \
		Stack.StepCompiledInRef<FMapProperty, TMap<KeyType, ValueType>>(&ParamName##Temp);

/** The object the thunk was called on (UE). */
#define P_THIS_CAST(ClassType) ((ClassType*)Context)
#define P_THIS P_THIS_CAST(ThisClass)

/**
 * Ends the parameter list (UE). It also marks Context and Z_Param__Result as used: static and void thunks do not read
 * them, and Leon builds warn about unused parameters.
 */
#define P_FINISH                                                                                                       \
	Stack.Code += !!Stack.Code;                                                                                        \
	(void)Context;                                                                                                     \
	(void)Z_Param__Result;

/** Brackets the native call (UE: also a profiling scope). */
#define P_NATIVE_BEGIN {
#define P_NATIVE_END }
