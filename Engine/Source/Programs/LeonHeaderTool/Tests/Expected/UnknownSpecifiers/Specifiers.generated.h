/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_Specifiers_generated_h
#error "Specifiers.generated.h already included, missing '#pragma once' in Specifiers.h"
#endif
#define LHTTEST_Specifiers_generated_h

#define FOREACH_ENUM_ESPECIFIERTEST(op) \
	op(ESpecifierTest::One)

enum class ESpecifierTest : uint8;
template<> LHTTEST_API UEnum* StaticEnum<ESpecifierTest>();

#define UnknownSpecifiers_Specifiers_h_16_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FSpecifierStruct_Statics; \
	static class UScriptStruct* StaticStruct();

template<> LHTTEST_API UScriptStruct* StaticStruct<struct FSpecifierStruct>();

#define UnknownSpecifiers_Specifiers_h_22_RPC_WRAPPERS \
	DECLARE_FUNCTION(execRun);

#define UnknownSpecifiers_Specifiers_h_22_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execRun);

#define UnknownSpecifiers_Specifiers_h_22_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUSpecifierObject(); \
	friend struct Z_Construct_UClass_USpecifierObject_Statics; \
public: \
	DECLARE_CLASS(USpecifierObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(USpecifierObject)

#define UnknownSpecifiers_Specifiers_h_22_INCLASS \
private: \
	static void StaticRegisterNativesUSpecifierObject(); \
	friend struct Z_Construct_UClass_USpecifierObject_Statics; \
public: \
	DECLARE_CLASS(USpecifierObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(USpecifierObject)

#define UnknownSpecifiers_Specifiers_h_22_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API USpecifierObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(USpecifierObject) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, USpecifierObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(USpecifierObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API USpecifierObject(USpecifierObject&&); \
	NO_API USpecifierObject(const USpecifierObject&); \
public:

#define UnknownSpecifiers_Specifiers_h_22_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API USpecifierObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API USpecifierObject(USpecifierObject&&); \
	NO_API USpecifierObject(const USpecifierObject&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, USpecifierObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(USpecifierObject); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(USpecifierObject)

#define UnknownSpecifiers_Specifiers_h_19_PROLOG

#define UnknownSpecifiers_Specifiers_h_22_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	UnknownSpecifiers_Specifiers_h_22_RPC_WRAPPERS \
	UnknownSpecifiers_Specifiers_h_22_INCLASS \
	UnknownSpecifiers_Specifiers_h_22_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define UnknownSpecifiers_Specifiers_h_22_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	UnknownSpecifiers_Specifiers_h_22_RPC_WRAPPERS_NO_PURE_DECLS \
	UnknownSpecifiers_Specifiers_h_22_INCLASS_NO_PURE_DECLS \
	UnknownSpecifiers_Specifiers_h_22_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class USpecifierObject>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID UnknownSpecifiers_Specifiers_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
