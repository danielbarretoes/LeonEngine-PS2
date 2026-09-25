/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_BasicObject_generated_h
#error "BasicObject.generated.h already included, missing '#pragma once' in BasicObject.h"
#endif
#define LHTTEST_BasicObject_generated_h

#define ClassBasic_BasicObject_h_20_RPC_WRAPPERS

#define ClassBasic_BasicObject_h_20_RPC_WRAPPERS_NO_PURE_DECLS

#define ClassBasic_BasicObject_h_20_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUBasicObject(); \
	friend struct Z_Construct_UClass_UBasicObject_Statics; \
public: \
	DECLARE_CLASS(UBasicObject, UObject, COMPILED_IN_FLAGS(0 | CLASS_Abstract | CLASS_Config | CLASS_Transient), CASTCLASS_None, TEXT("/Script/LhtTest"), LHTTEST_API) \
	DECLARE_SERIALIZER(UBasicObject) \
	static const TCHAR* StaticConfigName() {return TEXT("Game");}

#define ClassBasic_BasicObject_h_20_INCLASS \
private: \
	static void StaticRegisterNativesUBasicObject(); \
	friend struct Z_Construct_UClass_UBasicObject_Statics; \
public: \
	DECLARE_CLASS(UBasicObject, UObject, COMPILED_IN_FLAGS(0 | CLASS_Abstract | CLASS_Config | CLASS_Transient), CASTCLASS_None, TEXT("/Script/LhtTest"), LHTTEST_API) \
	DECLARE_SERIALIZER(UBasicObject) \
	static const TCHAR* StaticConfigName() {return TEXT("Game");}

#define ClassBasic_BasicObject_h_20_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	LHTTEST_API UBasicObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UBasicObject) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(LHTTEST_API, UBasicObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UBasicObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	LHTTEST_API UBasicObject(UBasicObject&&); \
	LHTTEST_API UBasicObject(const UBasicObject&); \
public:

#define ClassBasic_BasicObject_h_20_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	LHTTEST_API UBasicObject(UBasicObject&&); \
	LHTTEST_API UBasicObject(const UBasicObject&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(LHTTEST_API, UBasicObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UBasicObject); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(UBasicObject)

#define ClassBasic_BasicObject_h_17_PROLOG

#define ClassBasic_BasicObject_h_20_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ClassBasic_BasicObject_h_20_RPC_WRAPPERS \
	ClassBasic_BasicObject_h_20_INCLASS \
	ClassBasic_BasicObject_h_20_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define ClassBasic_BasicObject_h_20_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ClassBasic_BasicObject_h_20_RPC_WRAPPERS_NO_PURE_DECLS \
	ClassBasic_BasicObject_h_20_INCLASS_NO_PURE_DECLS \
	ClassBasic_BasicObject_h_20_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UBasicObject>();

#define ClassBasic_BasicObject_h_99_RPC_WRAPPERS

#define ClassBasic_BasicObject_h_99_RPC_WRAPPERS_NO_PURE_DECLS

#define ClassBasic_BasicObject_h_99_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUPlainObject(); \
	friend struct Z_Construct_UClass_UPlainObject_Statics; \
public: \
	DECLARE_CLASS(UPlainObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UPlainObject)

#define ClassBasic_BasicObject_h_99_INCLASS \
private: \
	static void StaticRegisterNativesUPlainObject(); \
	friend struct Z_Construct_UClass_UPlainObject_Statics; \
public: \
	DECLARE_CLASS(UPlainObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UPlainObject)

#define ClassBasic_BasicObject_h_99_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UPlainObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UPlainObject) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UPlainObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UPlainObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UPlainObject(UPlainObject&&); \
	NO_API UPlainObject(const UPlainObject&); \
public:

#define ClassBasic_BasicObject_h_99_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UPlainObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UPlainObject(UPlainObject&&); \
	NO_API UPlainObject(const UPlainObject&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UPlainObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UPlainObject); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UPlainObject)

#define ClassBasic_BasicObject_h_96_PROLOG

#define ClassBasic_BasicObject_h_99_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ClassBasic_BasicObject_h_99_RPC_WRAPPERS \
	ClassBasic_BasicObject_h_99_INCLASS \
	ClassBasic_BasicObject_h_99_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define ClassBasic_BasicObject_h_99_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ClassBasic_BasicObject_h_99_RPC_WRAPPERS_NO_PURE_DECLS \
	ClassBasic_BasicObject_h_99_INCLASS_NO_PURE_DECLS \
	ClassBasic_BasicObject_h_99_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UPlainObject>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID ClassBasic_BasicObject_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
