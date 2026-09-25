/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_ContainersObject_generated_h
#error "ContainersObject.generated.h already included, missing '#pragma once' in ContainersObject.h"
#endif
#define LHTTEST_ContainersObject_generated_h

#define FOREACH_ENUM_ECONTAINERKIND(op) \
	op(EContainerKind::Small) \
	op(EContainerKind::Large)

enum class EContainerKind : uint8;
template<> LHTTEST_API UEnum* StaticEnum<EContainerKind>();

#define Containers_ContainersObject_h_17_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FContainerEntry_Statics; \
	static class UScriptStruct* StaticStruct();

template<> LHTTEST_API UScriptStruct* StaticStruct<struct FContainerEntry>();

#define Containers_ContainersObject_h_26_RPC_WRAPPERS

#define Containers_ContainersObject_h_26_RPC_WRAPPERS_NO_PURE_DECLS

#define Containers_ContainersObject_h_26_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUContainersObject(); \
	friend struct Z_Construct_UClass_UContainersObject_Statics; \
public: \
	DECLARE_CLASS(UContainersObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UContainersObject)

#define Containers_ContainersObject_h_26_INCLASS \
private: \
	static void StaticRegisterNativesUContainersObject(); \
	friend struct Z_Construct_UClass_UContainersObject_Statics; \
public: \
	DECLARE_CLASS(UContainersObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UContainersObject)

#define Containers_ContainersObject_h_26_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UContainersObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UContainersObject) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UContainersObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UContainersObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UContainersObject(UContainersObject&&); \
	NO_API UContainersObject(const UContainersObject&); \
public:

#define Containers_ContainersObject_h_26_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UContainersObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UContainersObject(UContainersObject&&); \
	NO_API UContainersObject(const UContainersObject&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UContainersObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UContainersObject); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UContainersObject)

#define Containers_ContainersObject_h_23_PROLOG

#define Containers_ContainersObject_h_26_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	Containers_ContainersObject_h_26_RPC_WRAPPERS \
	Containers_ContainersObject_h_26_INCLASS \
	Containers_ContainersObject_h_26_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define Containers_ContainersObject_h_26_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	Containers_ContainersObject_h_26_RPC_WRAPPERS_NO_PURE_DECLS \
	Containers_ContainersObject_h_26_INCLASS_NO_PURE_DECLS \
	Containers_ContainersObject_h_26_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UContainersObject>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID Containers_ContainersObject_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
