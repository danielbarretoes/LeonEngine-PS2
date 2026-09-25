/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_StructTypes_generated_h
#error "StructTypes.generated.h already included, missing '#pragma once' in StructTypes.h"
#endif
#define LHTTEST_StructTypes_generated_h

#define Structs_StructTypes_h_10_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FBaseStats_Statics; \
	static class UScriptStruct* StaticStruct();

template<> LHTTEST_API UScriptStruct* StaticStruct<struct FBaseStats>();

#define Structs_StructTypes_h_24_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FDerivedStats_Statics; \
	static class UScriptStruct* StaticStruct(); \
	typedef FBaseStats Super;

template<> LHTTEST_API UScriptStruct* StaticStruct<struct FDerivedStats>();

#define Structs_StructTypes_h_43_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FEmptyStruct_Statics; \
	static class UScriptStruct* StaticStruct();

template<> LHTTEST_API UScriptStruct* StaticStruct<struct FEmptyStruct>();

#define Structs_StructTypes_h_49_RPC_WRAPPERS

#define Structs_StructTypes_h_49_RPC_WRAPPERS_NO_PURE_DECLS

#define Structs_StructTypes_h_49_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUStructHolder(); \
	friend struct Z_Construct_UClass_UStructHolder_Statics; \
public: \
	DECLARE_CLASS(UStructHolder, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UStructHolder)

#define Structs_StructTypes_h_49_INCLASS \
private: \
	static void StaticRegisterNativesUStructHolder(); \
	friend struct Z_Construct_UClass_UStructHolder_Statics; \
public: \
	DECLARE_CLASS(UStructHolder, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UStructHolder)

#define Structs_StructTypes_h_49_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UStructHolder(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UStructHolder) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UStructHolder); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UStructHolder); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UStructHolder(UStructHolder&&); \
	NO_API UStructHolder(const UStructHolder&); \
public:

#define Structs_StructTypes_h_49_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UStructHolder(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UStructHolder(UStructHolder&&); \
	NO_API UStructHolder(const UStructHolder&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UStructHolder); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UStructHolder); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UStructHolder)

#define Structs_StructTypes_h_46_PROLOG

#define Structs_StructTypes_h_49_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	Structs_StructTypes_h_49_RPC_WRAPPERS \
	Structs_StructTypes_h_49_INCLASS \
	Structs_StructTypes_h_49_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define Structs_StructTypes_h_49_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	Structs_StructTypes_h_49_RPC_WRAPPERS_NO_PURE_DECLS \
	Structs_StructTypes_h_49_INCLASS_NO_PURE_DECLS \
	Structs_StructTypes_h_49_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UStructHolder>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID Structs_StructTypes_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
