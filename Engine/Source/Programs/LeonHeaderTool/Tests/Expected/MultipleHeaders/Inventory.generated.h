/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_Inventory_generated_h
#error "Inventory.generated.h already included, missing '#pragma once' in Inventory.h"
#endif
#define LHTTEST_Inventory_generated_h

#define MultipleHeaders_Inventory_h_11_RPC_WRAPPERS

#define MultipleHeaders_Inventory_h_11_RPC_WRAPPERS_NO_PURE_DECLS

#define MultipleHeaders_Inventory_h_11_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUInventory(); \
	friend struct Z_Construct_UClass_UInventory_Statics; \
public: \
	DECLARE_CLASS(UInventory, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UInventory)

#define MultipleHeaders_Inventory_h_11_INCLASS \
private: \
	static void StaticRegisterNativesUInventory(); \
	friend struct Z_Construct_UClass_UInventory_Statics; \
public: \
	DECLARE_CLASS(UInventory, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UInventory)

#define MultipleHeaders_Inventory_h_11_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UInventory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UInventory) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UInventory); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UInventory); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UInventory(UInventory&&); \
	NO_API UInventory(const UInventory&); \
public:

#define MultipleHeaders_Inventory_h_11_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UInventory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UInventory(UInventory&&); \
	NO_API UInventory(const UInventory&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UInventory); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UInventory); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UInventory)

#define MultipleHeaders_Inventory_h_8_PROLOG

#define MultipleHeaders_Inventory_h_11_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	MultipleHeaders_Inventory_h_11_RPC_WRAPPERS \
	MultipleHeaders_Inventory_h_11_INCLASS \
	MultipleHeaders_Inventory_h_11_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define MultipleHeaders_Inventory_h_11_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	MultipleHeaders_Inventory_h_11_RPC_WRAPPERS_NO_PURE_DECLS \
	MultipleHeaders_Inventory_h_11_INCLASS_NO_PURE_DECLS \
	MultipleHeaders_Inventory_h_11_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UInventory>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID MultipleHeaders_Inventory_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
