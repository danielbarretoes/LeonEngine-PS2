/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_ConfigObject_generated_h
#error "ConfigObject.generated.h already included, missing '#pragma once' in ConfigObject.h"
#endif
#define LHTTEST_ConfigObject_generated_h

#define ConfigAndExec_ConfigObject_h_12_RPC_WRAPPERS \
	DECLARE_FUNCTION(execSetSensitivity); \
	DECLARE_FUNCTION(execAddMap);

#define ConfigAndExec_ConfigObject_h_12_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execSetSensitivity); \
	DECLARE_FUNCTION(execAddMap);

#define ConfigAndExec_ConfigObject_h_12_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUGameSettings(); \
	friend struct Z_Construct_UClass_UGameSettings_Statics; \
public: \
	DECLARE_CLASS(UGameSettings, UObject, COMPILED_IN_FLAGS(0 | CLASS_DefaultConfig | CLASS_Config), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UGameSettings) \
	static const TCHAR* StaticConfigName() {return TEXT("Game");}

#define ConfigAndExec_ConfigObject_h_12_INCLASS \
private: \
	static void StaticRegisterNativesUGameSettings(); \
	friend struct Z_Construct_UClass_UGameSettings_Statics; \
public: \
	DECLARE_CLASS(UGameSettings, UObject, COMPILED_IN_FLAGS(0 | CLASS_DefaultConfig | CLASS_Config), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UGameSettings) \
	static const TCHAR* StaticConfigName() {return TEXT("Game");}

#define ConfigAndExec_ConfigObject_h_12_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UGameSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UGameSettings) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UGameSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UGameSettings); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UGameSettings(UGameSettings&&); \
	NO_API UGameSettings(const UGameSettings&); \
public:

#define ConfigAndExec_ConfigObject_h_12_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UGameSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UGameSettings(UGameSettings&&); \
	NO_API UGameSettings(const UGameSettings&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UGameSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UGameSettings); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UGameSettings)

#define ConfigAndExec_ConfigObject_h_9_PROLOG

#define ConfigAndExec_ConfigObject_h_12_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ConfigAndExec_ConfigObject_h_12_RPC_WRAPPERS \
	ConfigAndExec_ConfigObject_h_12_INCLASS \
	ConfigAndExec_ConfigObject_h_12_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define ConfigAndExec_ConfigObject_h_12_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ConfigAndExec_ConfigObject_h_12_RPC_WRAPPERS_NO_PURE_DECLS \
	ConfigAndExec_ConfigObject_h_12_INCLASS_NO_PURE_DECLS \
	ConfigAndExec_ConfigObject_h_12_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UGameSettings>();

#define ConfigAndExec_ConfigObject_h_41_RPC_WRAPPERS

#define ConfigAndExec_ConfigObject_h_41_RPC_WRAPPERS_NO_PURE_DECLS

#define ConfigAndExec_ConfigObject_h_41_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUChildSettings(); \
	friend struct Z_Construct_UClass_UChildSettings_Statics; \
public: \
	DECLARE_CLASS(UChildSettings, UGameSettings, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UChildSettings)

#define ConfigAndExec_ConfigObject_h_41_INCLASS \
private: \
	static void StaticRegisterNativesUChildSettings(); \
	friend struct Z_Construct_UClass_UChildSettings_Statics; \
public: \
	DECLARE_CLASS(UChildSettings, UGameSettings, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UChildSettings)

#define ConfigAndExec_ConfigObject_h_41_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UChildSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UChildSettings) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UChildSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UChildSettings); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UChildSettings(UChildSettings&&); \
	NO_API UChildSettings(const UChildSettings&); \
public:

#define ConfigAndExec_ConfigObject_h_41_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UChildSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UChildSettings(UChildSettings&&); \
	NO_API UChildSettings(const UChildSettings&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UChildSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UChildSettings); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UChildSettings)

#define ConfigAndExec_ConfigObject_h_38_PROLOG

#define ConfigAndExec_ConfigObject_h_41_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ConfigAndExec_ConfigObject_h_41_RPC_WRAPPERS \
	ConfigAndExec_ConfigObject_h_41_INCLASS \
	ConfigAndExec_ConfigObject_h_41_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define ConfigAndExec_ConfigObject_h_41_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ConfigAndExec_ConfigObject_h_41_RPC_WRAPPERS_NO_PURE_DECLS \
	ConfigAndExec_ConfigObject_h_41_INCLASS_NO_PURE_DECLS \
	ConfigAndExec_ConfigObject_h_41_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UChildSettings>();

#define ConfigAndExec_ConfigObject_h_51_RPC_WRAPPERS

#define ConfigAndExec_ConfigObject_h_51_RPC_WRAPPERS_NO_PURE_DECLS

#define ConfigAndExec_ConfigObject_h_51_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUBindingSettings(); \
	friend struct Z_Construct_UClass_UBindingSettings_Statics; \
public: \
	DECLARE_CLASS(UBindingSettings, UObject, COMPILED_IN_FLAGS(0 | CLASS_Config | CLASS_PerObjectConfig), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UBindingSettings) \
	static const TCHAR* StaticConfigName() {return TEXT("Input");}

#define ConfigAndExec_ConfigObject_h_51_INCLASS \
private: \
	static void StaticRegisterNativesUBindingSettings(); \
	friend struct Z_Construct_UClass_UBindingSettings_Statics; \
public: \
	DECLARE_CLASS(UBindingSettings, UObject, COMPILED_IN_FLAGS(0 | CLASS_Config | CLASS_PerObjectConfig), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UBindingSettings) \
	static const TCHAR* StaticConfigName() {return TEXT("Input");}

#define ConfigAndExec_ConfigObject_h_51_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UBindingSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UBindingSettings) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UBindingSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UBindingSettings); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UBindingSettings(UBindingSettings&&); \
	NO_API UBindingSettings(const UBindingSettings&); \
public:

#define ConfigAndExec_ConfigObject_h_51_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UBindingSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UBindingSettings(UBindingSettings&&); \
	NO_API UBindingSettings(const UBindingSettings&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UBindingSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UBindingSettings); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UBindingSettings)

#define ConfigAndExec_ConfigObject_h_48_PROLOG

#define ConfigAndExec_ConfigObject_h_51_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ConfigAndExec_ConfigObject_h_51_RPC_WRAPPERS \
	ConfigAndExec_ConfigObject_h_51_INCLASS \
	ConfigAndExec_ConfigObject_h_51_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define ConfigAndExec_ConfigObject_h_51_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ConfigAndExec_ConfigObject_h_51_RPC_WRAPPERS_NO_PURE_DECLS \
	ConfigAndExec_ConfigObject_h_51_INCLASS_NO_PURE_DECLS \
	ConfigAndExec_ConfigObject_h_51_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UBindingSettings>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID ConfigAndExec_ConfigObject_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
