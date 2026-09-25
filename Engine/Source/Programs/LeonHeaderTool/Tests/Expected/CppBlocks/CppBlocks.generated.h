/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_CppBlocks_generated_h
#error "CppBlocks.generated.h already included, missing '#pragma once' in CppBlocks.h"
#endif
#define LHTTEST_CppBlocks_generated_h

#define CppBlocks_CppBlocks_h_14_RPC_WRAPPERS

#define CppBlocks_CppBlocks_h_14_RPC_WRAPPERS_NO_PURE_DECLS

#define CppBlocks_CppBlocks_h_14_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUCppBlocksObject(); \
	friend struct Z_Construct_UClass_UCppBlocksObject_Statics; \
public: \
	DECLARE_CLASS(UCppBlocksObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UCppBlocksObject)

#define CppBlocks_CppBlocks_h_14_INCLASS \
private: \
	static void StaticRegisterNativesUCppBlocksObject(); \
	friend struct Z_Construct_UClass_UCppBlocksObject_Statics; \
public: \
	DECLARE_CLASS(UCppBlocksObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UCppBlocksObject)

#define CppBlocks_CppBlocks_h_14_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UCppBlocksObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UCppBlocksObject) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UCppBlocksObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UCppBlocksObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UCppBlocksObject(UCppBlocksObject&&); \
	NO_API UCppBlocksObject(const UCppBlocksObject&); \
public:

#define CppBlocks_CppBlocks_h_14_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UCppBlocksObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UCppBlocksObject(UCppBlocksObject&&); \
	NO_API UCppBlocksObject(const UCppBlocksObject&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UCppBlocksObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UCppBlocksObject); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UCppBlocksObject)

#define CppBlocks_CppBlocks_h_11_PROLOG

#define CppBlocks_CppBlocks_h_14_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	CppBlocks_CppBlocks_h_14_RPC_WRAPPERS \
	CppBlocks_CppBlocks_h_14_INCLASS \
	CppBlocks_CppBlocks_h_14_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define CppBlocks_CppBlocks_h_14_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	CppBlocks_CppBlocks_h_14_RPC_WRAPPERS_NO_PURE_DECLS \
	CppBlocks_CppBlocks_h_14_INCLASS_NO_PURE_DECLS \
	CppBlocks_CppBlocks_h_14_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UCppBlocksObject>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID CppBlocks_CppBlocks_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
