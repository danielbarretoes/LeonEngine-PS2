/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Weapon.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_EWeaponSlot();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FAmmo();
	LHTTEST_API UClass* Z_Construct_UClass_UWeapon_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UWeapon();
	LHTTEST_API UClass* Z_Construct_UClass_URifle_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_URifle();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	static UEnum* EWeaponSlot_StaticEnum()
	{
		static UEnum* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticEnum(Z_Construct_UEnum_LhtTest_EWeaponSlot, Z_Construct_UPackage__Script_LhtTest(), TEXT("EWeaponSlot"));
		}
		return Singleton;
	}
	template<> LHTTEST_API UEnum* StaticEnum<EWeaponSlot>()
	{
		return EWeaponSlot_StaticEnum();
	}
	struct Z_Construct_UEnum_LhtTest_EWeaponSlot_Statics
	{
		static const UE4CodeGen_Private::FEnumeratorParam Enumerators[];
		static const UE4CodeGen_Private::FEnumParams EnumParams;
	};
	const UE4CodeGen_Private::FEnumeratorParam Z_Construct_UEnum_LhtTest_EWeaponSlot_Statics::Enumerators[] = {
		{ "EWeaponSlot::Primary", (int64)EWeaponSlot::Primary },
		{ "EWeaponSlot::Secondary", (int64)EWeaponSlot::Secondary },
	};
	const UE4CodeGen_Private::FEnumParams Z_Construct_UEnum_LhtTest_EWeaponSlot_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		"EWeaponSlot",
		"EWeaponSlot",
		Z_Construct_UEnum_LhtTest_EWeaponSlot_Statics::Enumerators,
		UE_ARRAY_COUNT(Z_Construct_UEnum_LhtTest_EWeaponSlot_Statics::Enumerators),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
	};
	UEnum* Z_Construct_UEnum_LhtTest_EWeaponSlot()
	{
		static UEnum* ReturnEnum = nullptr;
		if (!ReturnEnum)
		{
			UE4CodeGen_Private::ConstructUEnum(ReturnEnum, Z_Construct_UEnum_LhtTest_EWeaponSlot_Statics::EnumParams);
		}
		return ReturnEnum;
	}
	class UScriptStruct* FAmmo::StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FAmmo, Z_Construct_UPackage__Script_LhtTest(), TEXT("Ammo"), sizeof(FAmmo), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FAmmo>()
	{
		return FAmmo::StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FAmmo_Statics
	{
		static void* NewStructOps();
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Rounds;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FAmmo_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FAmmo>();
	}
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UScriptStruct_FAmmo_Statics::NewProp_Rounds = { "Rounds", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FAmmo, Rounds) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FAmmo_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FAmmo_Statics::NewProp_Rounds,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FAmmo_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"Ammo",
		sizeof(FAmmo),
		alignof(FAmmo),
		Z_Construct_UScriptStruct_FAmmo_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FAmmo_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000001),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FAmmo()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FAmmo_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	void UWeapon::StaticRegisterNativesUWeapon()
	{
	}
	UClass* Z_Construct_UClass_UWeapon_NoRegister()
	{
		return UWeapon::StaticClass();
	}
	struct Z_Construct_UClass_UWeapon_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UWeapon_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UWeapon_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UWeapon>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UWeapon_Statics::ClassParams = {
		&UWeapon::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		nullptr,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		0,
		0,
		0x000000A1u,
	};
	UClass* Z_Construct_UClass_UWeapon()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UWeapon_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UWeapon, 0);
	template<> LHTTEST_API UClass* StaticClass<UWeapon>()
	{
		return UWeapon::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UWeapon);
	void URifle::StaticRegisterNativesURifle()
	{
	}
	UClass* Z_Construct_UClass_URifle_NoRegister()
	{
		return URifle::StaticClass();
	}
	struct Z_Construct_UClass_URifle_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Magazine;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_URifle_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UWeapon,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UClass_URifle_Statics::NewProp_Magazine = { "Magazine", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(URifle, Magazine), Z_Construct_UScriptStruct_FAmmo };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_URifle_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_URifle_Statics::NewProp_Magazine,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_URifle_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<URifle>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_URifle_Statics::ClassParams = {
		&URifle::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_URifle_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_URifle_Statics::PropPointers),
		0,
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_URifle()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_URifle_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(URifle, 0);
	template<> LHTTEST_API UClass* StaticClass<URifle>()
	{
		return URifle::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(URifle);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
