/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_FunctionsObject_generated_h
#error "FunctionsObject.generated.h already included, missing '#pragma once' in FunctionsObject.h"
#endif
#define LHTTEST_FunctionsObject_generated_h

#define ClassFunctions_FunctionsObject_h_11_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FHitInfo_Statics; \
	static class UScriptStruct* StaticStruct();

template<> LHTTEST_API UScriptStruct* StaticStruct<struct FHitInfo>();

#define FOREACH_ENUM_EFIREMODE(op) \
	op(EFireMode::Single) \
	op(EFireMode::Burst) \
	op(EFireMode::Auto)

enum class EFireMode : uint8;
template<> LHTTEST_API UEnum* StaticEnum<EFireMode>();

#define FOREACH_ENUM_ELEGACYCHANNEL(op) \
	op(LC_Visibility) \
	op(LC_Camera)


#define ClassFunctions_FunctionsObject_h_35_RPC_WRAPPERS \
	DECLARE_FUNCTION(execGiveAmmo); \
	DECLARE_FUNCTION(execFire); \
	DECLARE_FUNCTION(execTraceAt); \
	DECLARE_FUNCTION(execAddLarge); \
	DECLARE_FUNCTION(execOnHit); \
	DECLARE_FUNCTION(execGetCount); \
	DECLARE_FUNCTION(execReset); \
	DECLARE_FUNCTION(execGetTitle); \
	DECLARE_FUNCTION(execGetByte);

#define ClassFunctions_FunctionsObject_h_35_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execGiveAmmo); \
	DECLARE_FUNCTION(execFire); \
	DECLARE_FUNCTION(execTraceAt); \
	DECLARE_FUNCTION(execAddLarge); \
	DECLARE_FUNCTION(execOnHit); \
	DECLARE_FUNCTION(execGetCount); \
	DECLARE_FUNCTION(execReset); \
	DECLARE_FUNCTION(execGetTitle); \
	DECLARE_FUNCTION(execGetByte);

#define ClassFunctions_FunctionsObject_h_35_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUFunctionsObject(); \
	friend struct Z_Construct_UClass_UFunctionsObject_Statics; \
public: \
	DECLARE_CLASS(UFunctionsObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UFunctionsObject)

#define ClassFunctions_FunctionsObject_h_35_INCLASS \
private: \
	static void StaticRegisterNativesUFunctionsObject(); \
	friend struct Z_Construct_UClass_UFunctionsObject_Statics; \
public: \
	DECLARE_CLASS(UFunctionsObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UFunctionsObject)

#define ClassFunctions_FunctionsObject_h_35_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UFunctionsObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UFunctionsObject) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UFunctionsObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UFunctionsObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UFunctionsObject(UFunctionsObject&&); \
	NO_API UFunctionsObject(const UFunctionsObject&); \
public:

#define ClassFunctions_FunctionsObject_h_35_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UFunctionsObject(UFunctionsObject&&); \
	NO_API UFunctionsObject(const UFunctionsObject&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UFunctionsObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UFunctionsObject); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UFunctionsObject)

#define ClassFunctions_FunctionsObject_h_32_PROLOG

#define ClassFunctions_FunctionsObject_h_35_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ClassFunctions_FunctionsObject_h_35_RPC_WRAPPERS \
	ClassFunctions_FunctionsObject_h_35_INCLASS \
	ClassFunctions_FunctionsObject_h_35_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define ClassFunctions_FunctionsObject_h_35_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ClassFunctions_FunctionsObject_h_35_RPC_WRAPPERS_NO_PURE_DECLS \
	ClassFunctions_FunctionsObject_h_35_INCLASS_NO_PURE_DECLS \
	ClassFunctions_FunctionsObject_h_35_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UFunctionsObject>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID ClassFunctions_FunctionsObject_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
