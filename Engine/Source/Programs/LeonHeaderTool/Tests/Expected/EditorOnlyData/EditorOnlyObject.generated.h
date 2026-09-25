/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_EditorOnlyObject_generated_h
#error "EditorOnlyObject.generated.h already included, missing '#pragma once' in EditorOnlyObject.h"
#endif
#define LHTTEST_EditorOnlyObject_generated_h

#define EditorOnlyData_EditorOnlyObject_h_10_RPC_WRAPPERS

#define EditorOnlyData_EditorOnlyObject_h_10_RPC_WRAPPERS_NO_PURE_DECLS

#define EditorOnlyData_EditorOnlyObject_h_10_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUEditorOnlyObject(); \
	friend struct Z_Construct_UClass_UEditorOnlyObject_Statics; \
public: \
	DECLARE_CLASS(UEditorOnlyObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UEditorOnlyObject)

#define EditorOnlyData_EditorOnlyObject_h_10_INCLASS \
private: \
	static void StaticRegisterNativesUEditorOnlyObject(); \
	friend struct Z_Construct_UClass_UEditorOnlyObject_Statics; \
public: \
	DECLARE_CLASS(UEditorOnlyObject, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UEditorOnlyObject)

#define EditorOnlyData_EditorOnlyObject_h_10_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UEditorOnlyObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UEditorOnlyObject) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UEditorOnlyObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UEditorOnlyObject); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UEditorOnlyObject(UEditorOnlyObject&&); \
	NO_API UEditorOnlyObject(const UEditorOnlyObject&); \
public:

#define EditorOnlyData_EditorOnlyObject_h_10_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UEditorOnlyObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UEditorOnlyObject(UEditorOnlyObject&&); \
	NO_API UEditorOnlyObject(const UEditorOnlyObject&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UEditorOnlyObject); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UEditorOnlyObject); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UEditorOnlyObject)

#define EditorOnlyData_EditorOnlyObject_h_7_PROLOG

#define EditorOnlyData_EditorOnlyObject_h_10_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	EditorOnlyData_EditorOnlyObject_h_10_RPC_WRAPPERS \
	EditorOnlyData_EditorOnlyObject_h_10_INCLASS \
	EditorOnlyData_EditorOnlyObject_h_10_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define EditorOnlyData_EditorOnlyObject_h_10_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	EditorOnlyData_EditorOnlyObject_h_10_RPC_WRAPPERS_NO_PURE_DECLS \
	EditorOnlyData_EditorOnlyObject_h_10_INCLASS_NO_PURE_DECLS \
	EditorOnlyData_EditorOnlyObject_h_10_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UEditorOnlyObject>();

#define EditorOnlyData_EditorOnlyObject_h_31_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics; \
	static class UScriptStruct* StaticStruct();

template<> LHTTEST_API UScriptStruct* StaticStruct<struct FEditorOnlyStruct>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID EditorOnlyData_EditorOnlyObject_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
