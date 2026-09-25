/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "EnumsObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_ETeam();
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_EWeaponFlags();
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_EUntyped();
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_ERoundState();
	LHTTEST_API UClass* Z_Construct_UClass_UEnumsObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UEnumsObject();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	static UEnum* ETeam_StaticEnum()
	{
		static UEnum* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticEnum(Z_Construct_UEnum_LhtTest_ETeam, Z_Construct_UPackage__Script_LhtTest(), TEXT("ETeam"));
		}
		return Singleton;
	}
	template<> LHTTEST_API UEnum* StaticEnum<ETeam>()
	{
		return ETeam_StaticEnum();
	}
	struct Z_Construct_UEnum_LhtTest_ETeam_Statics
	{
		static const UE4CodeGen_Private::FEnumeratorParam Enumerators[];
		static const UE4CodeGen_Private::FEnumParams EnumParams;
	};
	const UE4CodeGen_Private::FEnumeratorParam Z_Construct_UEnum_LhtTest_ETeam_Statics::Enumerators[] = {
		{ "ETeam::None", (int64)ETeam::None },
		{ "ETeam::CounterTerrorists", (int64)ETeam::CounterTerrorists },
		{ "ETeam::Terrorists", (int64)ETeam::Terrorists },
	};
	const UE4CodeGen_Private::FEnumParams Z_Construct_UEnum_LhtTest_ETeam_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		"ETeam",
		"ETeam",
		Z_Construct_UEnum_LhtTest_ETeam_Statics::Enumerators,
		UE_ARRAY_COUNT(Z_Construct_UEnum_LhtTest_ETeam_Statics::Enumerators),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
	};
	UEnum* Z_Construct_UEnum_LhtTest_ETeam()
	{
		static UEnum* ReturnEnum = nullptr;
		if (!ReturnEnum)
		{
			UE4CodeGen_Private::ConstructUEnum(ReturnEnum, Z_Construct_UEnum_LhtTest_ETeam_Statics::EnumParams);
		}
		return ReturnEnum;
	}
	static UEnum* EWeaponFlags_StaticEnum()
	{
		static UEnum* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticEnum(Z_Construct_UEnum_LhtTest_EWeaponFlags, Z_Construct_UPackage__Script_LhtTest(), TEXT("EWeaponFlags"));
		}
		return Singleton;
	}
	template<> LHTTEST_API UEnum* StaticEnum<EWeaponFlags>()
	{
		return EWeaponFlags_StaticEnum();
	}
	struct Z_Construct_UEnum_LhtTest_EWeaponFlags_Statics
	{
		static const UE4CodeGen_Private::FEnumeratorParam Enumerators[];
		static const UE4CodeGen_Private::FEnumParams EnumParams;
	};
	const UE4CodeGen_Private::FEnumeratorParam Z_Construct_UEnum_LhtTest_EWeaponFlags_Statics::Enumerators[] = {
		{ "EWeaponFlags::None", (int64)EWeaponFlags::None },
		{ "EWeaponFlags::Automatic", (int64)EWeaponFlags::Automatic },
		{ "EWeaponFlags::Silenced", (int64)EWeaponFlags::Silenced },
	};
	const UE4CodeGen_Private::FEnumParams Z_Construct_UEnum_LhtTest_EWeaponFlags_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		"EWeaponFlags",
		"EWeaponFlags",
		Z_Construct_UEnum_LhtTest_EWeaponFlags_Statics::Enumerators,
		UE_ARRAY_COUNT(Z_Construct_UEnum_LhtTest_EWeaponFlags_Statics::Enumerators),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EEnumFlags::Flags,
		(uint8)UEnum::ECppForm::EnumClass,
	};
	UEnum* Z_Construct_UEnum_LhtTest_EWeaponFlags()
	{
		static UEnum* ReturnEnum = nullptr;
		if (!ReturnEnum)
		{
			UE4CodeGen_Private::ConstructUEnum(ReturnEnum, Z_Construct_UEnum_LhtTest_EWeaponFlags_Statics::EnumParams);
		}
		return ReturnEnum;
	}
	static UEnum* EUntyped_StaticEnum()
	{
		static UEnum* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticEnum(Z_Construct_UEnum_LhtTest_EUntyped, Z_Construct_UPackage__Script_LhtTest(), TEXT("EUntyped"));
		}
		return Singleton;
	}
	template<> LHTTEST_API UEnum* StaticEnum<EUntyped>()
	{
		return EUntyped_StaticEnum();
	}
	struct Z_Construct_UEnum_LhtTest_EUntyped_Statics
	{
		static const UE4CodeGen_Private::FEnumeratorParam Enumerators[];
		static const UE4CodeGen_Private::FEnumParams EnumParams;
	};
	const UE4CodeGen_Private::FEnumeratorParam Z_Construct_UEnum_LhtTest_EUntyped_Statics::Enumerators[] = {
		{ "EUntyped::First", (int64)EUntyped::First },
		{ "EUntyped::Second", (int64)EUntyped::Second },
	};
	const UE4CodeGen_Private::FEnumParams Z_Construct_UEnum_LhtTest_EUntyped_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		"EUntyped",
		"EUntyped",
		Z_Construct_UEnum_LhtTest_EUntyped_Statics::Enumerators,
		UE_ARRAY_COUNT(Z_Construct_UEnum_LhtTest_EUntyped_Statics::Enumerators),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
	};
	UEnum* Z_Construct_UEnum_LhtTest_EUntyped()
	{
		static UEnum* ReturnEnum = nullptr;
		if (!ReturnEnum)
		{
			UE4CodeGen_Private::ConstructUEnum(ReturnEnum, Z_Construct_UEnum_LhtTest_EUntyped_Statics::EnumParams);
		}
		return ReturnEnum;
	}
	static UEnum* ERoundState_StaticEnum()
	{
		static UEnum* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticEnum(Z_Construct_UEnum_LhtTest_ERoundState, Z_Construct_UPackage__Script_LhtTest(), TEXT("ERoundState"));
		}
		return Singleton;
	}
	template<> LHTTEST_API UEnum* StaticEnum<ERoundState>()
	{
		return ERoundState_StaticEnum();
	}
	struct Z_Construct_UEnum_LhtTest_ERoundState_Statics
	{
		static const UE4CodeGen_Private::FEnumeratorParam Enumerators[];
		static const UE4CodeGen_Private::FEnumParams EnumParams;
	};
	const UE4CodeGen_Private::FEnumeratorParam Z_Construct_UEnum_LhtTest_ERoundState_Statics::Enumerators[] = {
		{ "RS_Freeze", (int64)RS_Freeze },
		{ "RS_Live", (int64)RS_Live },
		{ "RS_End", (int64)RS_End },
	};
	const UE4CodeGen_Private::FEnumParams Z_Construct_UEnum_LhtTest_ERoundState_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		"ERoundState",
		"ERoundState",
		Z_Construct_UEnum_LhtTest_ERoundState_Statics::Enumerators,
		UE_ARRAY_COUNT(Z_Construct_UEnum_LhtTest_ERoundState_Statics::Enumerators),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::Regular,
	};
	UEnum* Z_Construct_UEnum_LhtTest_ERoundState()
	{
		static UEnum* ReturnEnum = nullptr;
		if (!ReturnEnum)
		{
			UE4CodeGen_Private::ConstructUEnum(ReturnEnum, Z_Construct_UEnum_LhtTest_ERoundState_Statics::EnumParams);
		}
		return ReturnEnum;
	}
	void UEnumsObject::StaticRegisterNativesUEnumsObject()
	{
	}
	UClass* Z_Construct_UClass_UEnumsObject_NoRegister()
	{
		return UEnumsObject::StaticClass();
	}
	struct Z_Construct_UClass_UEnumsObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_Team_Underlying;
		static const UE4CodeGen_Private::FEnumPropertyParams NewProp_Team;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_WeaponFlags_Underlying;
		static const UE4CodeGen_Private::FEnumPropertyParams NewProp_WeaponFlags;
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_RoundState;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Scores_ValueProp;
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_Scores_Key_KeyProp_Underlying;
		static const UE4CodeGen_Private::FEnumPropertyParams NewProp_Scores_Key_KeyProp;
		static const UE4CodeGen_Private::FMapPropertyParams NewProp_Scores;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UEnumsObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UClass_UEnumsObject_Statics::NewProp_Team_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, nullptr };
	const UE4CodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UEnumsObject_Statics::NewProp_Team = { "Team", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UEnumsObject, Team), Z_Construct_UEnum_LhtTest_ETeam };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UEnumsObject_Statics::NewProp_WeaponFlags_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UEnumsObject_Statics::NewProp_WeaponFlags = { "WeaponFlags", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UEnumsObject, WeaponFlags), Z_Construct_UEnum_LhtTest_EWeaponFlags };
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UClass_UEnumsObject_Statics::NewProp_RoundState = { "RoundState", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UEnumsObject, RoundState), Z_Construct_UEnum_LhtTest_ERoundState };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UEnumsObject_Statics::NewProp_Scores_ValueProp = { "Scores", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, 1 };
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UClass_UEnumsObject_Statics::NewProp_Scores_Key_KeyProp_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, nullptr };
	const UE4CodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UEnumsObject_Statics::NewProp_Scores_Key_KeyProp = { "Scores_Key", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, Z_Construct_UEnum_LhtTest_ETeam };
	const UE4CodeGen_Private::FMapPropertyParams Z_Construct_UClass_UEnumsObject_Statics::NewProp_Scores = { "Scores", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Map, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UEnumsObject, Scores), EMapPropertyFlags::None };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UEnumsObject_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEnumsObject_Statics::NewProp_Team_Underlying,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEnumsObject_Statics::NewProp_Team,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEnumsObject_Statics::NewProp_WeaponFlags_Underlying,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEnumsObject_Statics::NewProp_WeaponFlags,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEnumsObject_Statics::NewProp_RoundState,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEnumsObject_Statics::NewProp_Scores_ValueProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEnumsObject_Statics::NewProp_Scores_Key_KeyProp_Underlying,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEnumsObject_Statics::NewProp_Scores_Key_KeyProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEnumsObject_Statics::NewProp_Scores,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UEnumsObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UEnumsObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UEnumsObject_Statics::ClassParams = {
		&UEnumsObject::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UEnumsObject_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UEnumsObject_Statics::PropPointers),
		0,
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_UEnumsObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UEnumsObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UEnumsObject, 0);
	template<> LHTTEST_API UClass* StaticClass<UEnumsObject>()
	{
		return UEnumsObject::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UEnumsObject);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
