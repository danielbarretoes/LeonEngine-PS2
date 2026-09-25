/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "StructTypes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FBaseStats();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FDerivedStats();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FEmptyStruct();
	LHTTEST_API UClass* Z_Construct_UClass_UStructHolder_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UStructHolder();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	class UScriptStruct* FBaseStats::StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FBaseStats, Z_Construct_UPackage__Script_LhtTest(), TEXT("BaseStats"), sizeof(FBaseStats), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FBaseStats>()
	{
		return FBaseStats::StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FBaseStats_Statics
	{
		static void* NewStructOps();
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Level;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FBaseStats_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FBaseStats>();
	}
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UScriptStruct_FBaseStats_Statics::NewProp_Level = { "Level", nullptr, (EPropertyFlags)0x0010000000000001, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FBaseStats, Level) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FBaseStats_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FBaseStats_Statics::NewProp_Level,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FBaseStats_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"BaseStats",
		sizeof(FBaseStats),
		alignof(FBaseStats),
		Z_Construct_UScriptStruct_FBaseStats_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FBaseStats_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000201),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FBaseStats()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FBaseStats_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	class UScriptStruct* FDerivedStats::StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FDerivedStats, Z_Construct_UPackage__Script_LhtTest(), TEXT("DerivedStats"), sizeof(FDerivedStats), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FDerivedStats>()
	{
		return FDerivedStats::StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FDerivedStats_Statics
	{
		static void* NewStructOps();
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Nested;
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_ByName_ValueProp;
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_ByName_Key_KeyProp;
		static const UE4CodeGen_Private::FMapPropertyParams NewProp_ByName;
		static void NewProp_bHidden_SetBit(void* Obj);
		static const UE4CodeGen_Private::FBoolPropertyParams NewProp_bHidden;
		static const UE4CodeGen_Private::FStrPropertyParams NewProp_Secret;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FDerivedStats_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FDerivedStats>();
	}
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_Nested = { "Nested", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FDerivedStats, Nested), Z_Construct_UScriptStruct_FBaseStats };
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_ByName_ValueProp = { "ByName", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, 1, Z_Construct_UScriptStruct_FBaseStats };
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_ByName_Key_KeyProp = { "ByName_Key", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FMapPropertyParams Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_ByName = { "ByName", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Map, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FDerivedStats, ByName), EMapPropertyFlags::None };
	void Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_bHidden_SetBit(void* Obj)
	{
		((FDerivedStats*)Obj)->bHidden = 1;
	}
	const UE4CodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_bHidden = { "bHidden", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Bool, RF_Public|RF_Transient|RF_MarkAsNative, 1, sizeof(uint32), sizeof(FDerivedStats), &Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_bHidden_SetBit };
	const UE4CodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_Secret = { "Secret", nullptr, (EPropertyFlags)0x0040000000000000, UE4CodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FDerivedStats, Secret) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FDerivedStats_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_Nested,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_ByName_ValueProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_ByName_Key_KeyProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_ByName,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_bHidden,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FDerivedStats_Statics::NewProp_Secret,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FDerivedStats_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		Z_Construct_UScriptStruct_FBaseStats,
		&NewStructOps,
		"DerivedStats",
		sizeof(FDerivedStats),
		alignof(FDerivedStats),
		Z_Construct_UScriptStruct_FDerivedStats_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FDerivedStats_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000001),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FDerivedStats()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FDerivedStats_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	class UScriptStruct* FEmptyStruct::StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FEmptyStruct, Z_Construct_UPackage__Script_LhtTest(), TEXT("EmptyStruct"), sizeof(FEmptyStruct), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FEmptyStruct>()
	{
		return FEmptyStruct::StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FEmptyStruct_Statics
	{
		static void* NewStructOps();
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FEmptyStruct_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FEmptyStruct>();
	}
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FEmptyStruct_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"EmptyStruct",
		sizeof(FEmptyStruct),
		alignof(FEmptyStruct),
		nullptr,
		0,
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000011),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FEmptyStruct()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FEmptyStruct_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	void UStructHolder::StaticRegisterNativesUStructHolder()
	{
	}
	UClass* Z_Construct_UClass_UStructHolder_NoRegister()
	{
		return UStructHolder::StaticClass();
	}
	struct Z_Construct_UClass_UStructHolder_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Stats;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UStructHolder_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UClass_UStructHolder_Statics::NewProp_Stats = { "Stats", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UStructHolder, Stats), Z_Construct_UScriptStruct_FDerivedStats };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UStructHolder_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UStructHolder_Statics::NewProp_Stats,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UStructHolder_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UStructHolder>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UStructHolder_Statics::ClassParams = {
		&UStructHolder::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UStructHolder_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UStructHolder_Statics::PropPointers),
		0,
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_UStructHolder()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UStructHolder_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UStructHolder, 0);
	template<> LHTTEST_API UClass* StaticClass<UStructHolder>()
	{
		return UStructHolder::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UStructHolder);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
