/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef GAME_ShooterCharacter_generated_h
#error "ShooterCharacter.generated.h already included, missing '#pragma once' in ShooterCharacter.h"
#endif
#define GAME_ShooterCharacter_generated_h

#define ObjectReferences_ShooterCharacter_h_13_RPC_WRAPPERS \
	DECLARE_FUNCTION(execTeleport); \
	DECLARE_FUNCTION(execFindTarget);

#define ObjectReferences_ShooterCharacter_h_13_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execTeleport); \
	DECLARE_FUNCTION(execFindTarget);

#define ObjectReferences_ShooterCharacter_h_13_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAShooterCharacter(); \
	friend struct Z_Construct_UClass_AShooterCharacter_Statics; \
public: \
	DECLARE_CLASS(AShooterCharacter, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/Game"), GAME_API) \
	DECLARE_SERIALIZER(AShooterCharacter) \
	static const TCHAR* StaticConfigName() {return TEXT("Game");}

#define ObjectReferences_ShooterCharacter_h_13_INCLASS \
private: \
	static void StaticRegisterNativesAShooterCharacter(); \
	friend struct Z_Construct_UClass_AShooterCharacter_Statics; \
public: \
	DECLARE_CLASS(AShooterCharacter, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/Game"), GAME_API) \
	DECLARE_SERIALIZER(AShooterCharacter) \
	static const TCHAR* StaticConfigName() {return TEXT("Game");}

#define ObjectReferences_ShooterCharacter_h_13_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	GAME_API AShooterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(AShooterCharacter) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(GAME_API, AShooterCharacter); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AShooterCharacter); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	GAME_API AShooterCharacter(AShooterCharacter&&); \
	GAME_API AShooterCharacter(const AShooterCharacter&); \
public:

#define ObjectReferences_ShooterCharacter_h_13_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	GAME_API AShooterCharacter(AShooterCharacter&&); \
	GAME_API AShooterCharacter(const AShooterCharacter&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(GAME_API, AShooterCharacter); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AShooterCharacter); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AShooterCharacter)

#define ObjectReferences_ShooterCharacter_h_10_PROLOG

#define ObjectReferences_ShooterCharacter_h_13_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ObjectReferences_ShooterCharacter_h_13_RPC_WRAPPERS \
	ObjectReferences_ShooterCharacter_h_13_INCLASS \
	ObjectReferences_ShooterCharacter_h_13_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define ObjectReferences_ShooterCharacter_h_13_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	ObjectReferences_ShooterCharacter_h_13_RPC_WRAPPERS_NO_PURE_DECLS \
	ObjectReferences_ShooterCharacter_h_13_INCLASS_NO_PURE_DECLS \
	ObjectReferences_ShooterCharacter_h_13_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> GAME_API UClass* StaticClass<class AShooterCharacter>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID ObjectReferences_ShooterCharacter_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
