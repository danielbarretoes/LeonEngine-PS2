/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef LHTTEST_Weapon_generated_h
#error "Weapon.generated.h already included, missing '#pragma once' in Weapon.h"
#endif
#define LHTTEST_Weapon_generated_h

#define FOREACH_ENUM_EWEAPONSLOT(op) \
	op(EWeaponSlot::Primary) \
	op(EWeaponSlot::Secondary)

enum class EWeaponSlot : uint8;
template<> LHTTEST_API UEnum* StaticEnum<EWeaponSlot>();

#define MultipleHeaders_Weapon_h_17_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FAmmo_Statics; \
	static class UScriptStruct* StaticStruct();

template<> LHTTEST_API UScriptStruct* StaticStruct<struct FAmmo>();

#define MultipleHeaders_Weapon_h_26_RPC_WRAPPERS

#define MultipleHeaders_Weapon_h_26_RPC_WRAPPERS_NO_PURE_DECLS

#define MultipleHeaders_Weapon_h_26_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUWeapon(); \
	friend struct Z_Construct_UClass_UWeapon_Statics; \
public: \
	DECLARE_CLASS(UWeapon, UObject, COMPILED_IN_FLAGS(0 | CLASS_Abstract), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UWeapon)

#define MultipleHeaders_Weapon_h_26_INCLASS \
private: \
	static void StaticRegisterNativesUWeapon(); \
	friend struct Z_Construct_UClass_UWeapon_Statics; \
public: \
	DECLARE_CLASS(UWeapon, UObject, COMPILED_IN_FLAGS(0 | CLASS_Abstract), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(UWeapon)

#define MultipleHeaders_Weapon_h_26_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UWeapon) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UWeapon); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UWeapon); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UWeapon(UWeapon&&); \
	NO_API UWeapon(const UWeapon&); \
public:

#define MultipleHeaders_Weapon_h_26_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API UWeapon(UWeapon&&); \
	NO_API UWeapon(const UWeapon&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UWeapon); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UWeapon); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UWeapon)

#define MultipleHeaders_Weapon_h_23_PROLOG

#define MultipleHeaders_Weapon_h_26_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	MultipleHeaders_Weapon_h_26_RPC_WRAPPERS \
	MultipleHeaders_Weapon_h_26_INCLASS \
	MultipleHeaders_Weapon_h_26_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define MultipleHeaders_Weapon_h_26_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	MultipleHeaders_Weapon_h_26_RPC_WRAPPERS_NO_PURE_DECLS \
	MultipleHeaders_Weapon_h_26_INCLASS_NO_PURE_DECLS \
	MultipleHeaders_Weapon_h_26_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class UWeapon>();

#define MultipleHeaders_Weapon_h_32_RPC_WRAPPERS

#define MultipleHeaders_Weapon_h_32_RPC_WRAPPERS_NO_PURE_DECLS

#define MultipleHeaders_Weapon_h_32_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesURifle(); \
	friend struct Z_Construct_UClass_URifle_Statics; \
public: \
	DECLARE_CLASS(URifle, UWeapon, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(URifle)

#define MultipleHeaders_Weapon_h_32_INCLASS \
private: \
	static void StaticRegisterNativesURifle(); \
	friend struct Z_Construct_UClass_URifle_Statics; \
public: \
	DECLARE_CLASS(URifle, UWeapon, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/LhtTest"), NO_API) \
	DECLARE_SERIALIZER(URifle)

#define MultipleHeaders_Weapon_h_32_STANDARD_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API URifle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(URifle) \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, URifle); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(URifle); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API URifle(URifle&&); \
	NO_API URifle(const URifle&); \
public:

#define MultipleHeaders_Weapon_h_32_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API URifle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { } \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	NO_API URifle(URifle&&); \
	NO_API URifle(const URifle&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, URifle); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(URifle); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(URifle)

#define MultipleHeaders_Weapon_h_29_PROLOG

#define MultipleHeaders_Weapon_h_32_GENERATED_BODY_LEGACY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	MultipleHeaders_Weapon_h_32_RPC_WRAPPERS \
	MultipleHeaders_Weapon_h_32_INCLASS \
	MultipleHeaders_Weapon_h_32_STANDARD_CONSTRUCTORS \
public: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#define MultipleHeaders_Weapon_h_32_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	MultipleHeaders_Weapon_h_32_RPC_WRAPPERS_NO_PURE_DECLS \
	MultipleHeaders_Weapon_h_32_INCLASS_NO_PURE_DECLS \
	MultipleHeaders_Weapon_h_32_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS

template<> LHTTEST_API UClass* StaticClass<class URifle>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID MultipleHeaders_Weapon_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
