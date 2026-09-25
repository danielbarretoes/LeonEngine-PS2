/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_LegacyObject_generated_h
#error "LegacyObject.generated.h already included, missing '#pragma once' in LegacyObject.h"
#endif
#define LHTTEST_LegacyObject_generated_h

#define LegacyBody_LegacyObject_h_12_RPC_WRAPPERS

#define LegacyBody_LegacyObject_h_12_RPC_WRAPPERS_NO_PURE_DECLS

#define LegacyBody_LegacyObject_h_12_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesULegacyObject(); \
	friend struct Z_Construct_UClass_ULegacyObject_Statics; \
public: \
	DECLARE_CLASS(ULegacyObject, UObject, COMPILED_IN_FLAGS(0 | CLASS_Transient), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(ULegacyObject)

#define LegacyBody_LegacyObject_h_12_INCLASS \
private: \
	static void StaticRegisterNativesULegacyObject(); \
	friend struct Z_Construct_UClass_ULegacyObject_Statics; \
public: \
	DECLARE_CLASS(ULegacyObject, UObject, COMPILED_IN_FLAGS(0 | CLASS_Transient), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(ULegacyObject)

#define LegacyBody_LegacyObject_h_12_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API ULegacyObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(ULegacyObject) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, ULegacyObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(ULegacyObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API ULegacyObject(ULegacyObject&&); \
	NO_API ULegacyObject(const ULegacyObject&); \
public:

#define LegacyBody_LegacyObject_h_12_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API ULegacyObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API ULegacyObject(ULegacyObject&&); \
	NO_API ULegacyObject(const ULegacyObject&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, ULegacyObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(ULegacyObject); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(ULegacyObject)

#define LegacyBody_LegacyObject_h_7_PROLOG
#define LegacyBody_LegacyObject_h_8_PROLOG
#define LegacyBody_LegacyObject_h_9_PROLOG

#define LegacyBody_LegacyObject_h_12_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	LegacyBody_LegacyObject_h_12_RPC_WRAPPERS \
	LegacyBody_LegacyObject_h_12_INCLASS \
	LegacyBody_LegacyObject_h_12_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define LegacyBody_LegacyObject_h_12_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	LegacyBody_LegacyObject_h_12_RPC_WRAPPERS_NO_PURE_DECLS \
	LegacyBody_LegacyObject_h_12_INCLASS_NO_PURE_DECLS \
	LegacyBody_LegacyObject_h_12_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class ULegacyObject>();

#define LegacyBody_LegacyObject_h_21_RPC_WRAPPERS

#define LegacyBody_LegacyObject_h_21_RPC_WRAPPERS_NO_PURE_DECLS

#define LegacyBody_LegacyObject_h_21_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUCustomVTableObject(); \
	friend struct Z_Construct_UClass_UCustomVTableObject_Statics; \
public: \
	DECLARE_CLASS(UCustomVTableObject, UObject, COMPILED_IN_FLAGS(0 | CLASS_DefaultConfig | CLASS_Config | CLASS_NotPlaceable | CLASS_EditInlineNew | CLASS_MinimalAPI), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UCustomVTableObject) \
	static const TCHAR* StaticConfigName() {return TEXT("Engine");}

#define LegacyBody_LegacyObject_h_21_INCLASS \
private: \
	static void StaticRegisterNativesUCustomVTableObject(); \
	friend struct Z_Construct_UClass_UCustomVTableObject_Statics; \
public: \
	DECLARE_CLASS(UCustomVTableObject, UObject, COMPILED_IN_FLAGS(0 | CLASS_DefaultConfig | CLASS_Config | CLASS_NotPlaceable | CLASS_EditInlineNew | CLASS_MinimalAPI), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UCustomVTableObject) \
	static const TCHAR* StaticConfigName() {return TEXT("Engine");}

#define LegacyBody_LegacyObject_h_21_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UCustomVTableObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UCustomVTableObject) \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UCustomVTableObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UCustomVTableObject(UCustomVTableObject&&); \
	NO_API UCustomVTableObject(const UCustomVTableObject&); \
public:

#define LegacyBody_LegacyObject_h_21_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UCustomVTableObject(UCustomVTableObject&&); \
	NO_API UCustomVTableObject(const UCustomVTableObject&); \
public: \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UCustomVTableObject); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UCustomVTableObject)

#define LegacyBody_LegacyObject_h_18_PROLOG

#define LegacyBody_LegacyObject_h_21_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	LegacyBody_LegacyObject_h_21_RPC_WRAPPERS \
	LegacyBody_LegacyObject_h_21_INCLASS \
	LegacyBody_LegacyObject_h_21_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define LegacyBody_LegacyObject_h_21_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	LegacyBody_LegacyObject_h_21_RPC_WRAPPERS_NO_PURE_DECLS \
	LegacyBody_LegacyObject_h_21_INCLASS_NO_PURE_DECLS \
	LegacyBody_LegacyObject_h_21_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UCustomVTableObject>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID LegacyBody_LegacyObject_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
