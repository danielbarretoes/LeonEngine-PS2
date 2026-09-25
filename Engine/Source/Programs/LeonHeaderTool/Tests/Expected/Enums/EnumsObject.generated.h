/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_EnumsObject_generated_h
#error "EnumsObject.generated.h already included, missing '#pragma once' in EnumsObject.h"
#endif
#define LHTTEST_EnumsObject_generated_h

#define FOREACH_ENUM_ETEAM(op) \
	op(ETeam::None) \
	op(ETeam::CounterTerrorists) \
	op(ETeam::Terrorists)

enum class ETeam : uint8;
template<> LHTTEST_API UEnum* StaticEnum<ETeam>();

#define FOREACH_ENUM_EWEAPONFLAGS(op) \
	op(EWeaponFlags::None) \
	op(EWeaponFlags::Automatic) \
	op(EWeaponFlags::Silenced)

enum class EWeaponFlags : int32;
template<> LHTTEST_API UEnum* StaticEnum<EWeaponFlags>();

#define FOREACH_ENUM_EUNTYPED(op) \
	op(EUntyped::First) \
	op(EUntyped::Second)


#define FOREACH_ENUM_EROUNDSTATE(op) \
	op(RS_Freeze) \
	op(RS_Live) \
	op(RS_End)


#define Enums_EnumsObject_h_41_RPC_WRAPPERS

#define Enums_EnumsObject_h_41_RPC_WRAPPERS_NO_PURE_DECLS

#define Enums_EnumsObject_h_41_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUEnumsObject(); \
	friend struct Z_Construct_UClass_UEnumsObject_Statics; \
public: \
	DECLARE_CLASS(UEnumsObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UEnumsObject)

#define Enums_EnumsObject_h_41_INCLASS \
private: \
	static void StaticRegisterNativesUEnumsObject(); \
	friend struct Z_Construct_UClass_UEnumsObject_Statics; \
public: \
	DECLARE_CLASS(UEnumsObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UEnumsObject)

#define Enums_EnumsObject_h_41_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UEnumsObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UEnumsObject) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UEnumsObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UEnumsObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UEnumsObject(UEnumsObject&&); \
	NO_API UEnumsObject(const UEnumsObject&); \
public:

#define Enums_EnumsObject_h_41_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UEnumsObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UEnumsObject(UEnumsObject&&); \
	NO_API UEnumsObject(const UEnumsObject&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UEnumsObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UEnumsObject); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UEnumsObject)

#define Enums_EnumsObject_h_38_PROLOG

#define Enums_EnumsObject_h_41_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	Enums_EnumsObject_h_41_RPC_WRAPPERS \
	Enums_EnumsObject_h_41_INCLASS \
	Enums_EnumsObject_h_41_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define Enums_EnumsObject_h_41_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	Enums_EnumsObject_h_41_RPC_WRAPPERS_NO_PURE_DECLS \
	Enums_EnumsObject_h_41_INCLASS_NO_PURE_DECLS \
	Enums_EnumsObject_h_41_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UEnumsObject>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID Enums_EnumsObject_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
