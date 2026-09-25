/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Specifiers.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_ESpecifierTest();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FSpecifierStruct();
	LHTTEST_API UClass* Z_Construct_UClass_USpecifierObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_USpecifierObject();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	static UEnum* ESpecifierTest_StaticEnum()
	{
		static UEnum* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticEnum(Z_Construct_UEnum_LhtTest_ESpecifierTest, Z_Construct_UPackage__Script_LhtTest(), TEXT("ESpecifierTest"));
		}
		return Singleton;
	}
	template<> LHTTEST_API UEnum* StaticEnum<ESpecifierTest>()
	{
		return ESpecifierTest_StaticEnum();
	}
	struct Z_Construct_UEnum_LhtTest_ESpecifierTest_Statics
	{
		static const UE4CodeGen_Private::FEnumeratorParam Enumerators[];
		static const UE4CodeGen_Private::FEnumParams EnumParams;
	};
	const UE4CodeGen_Private::FEnumeratorParam Z_Construct_UEnum_LhtTest_ESpecifierTest_Statics::Enumerators[] = {
		{ "ESpecifierTest::One", (int64)ESpecifierTest::One },
	};
	const UE4CodeGen_Private::FEnumParams Z_Construct_UEnum_LhtTest_ESpecifierTest_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		"ESpecifierTest",
		"ESpecifierTest",
		Z_Construct_UEnum_LhtTest_ESpecifierTest_Statics::Enumerators,
		UE_ARRAY_COUNT(Z_Construct_UEnum_LhtTest_ESpecifierTest_Statics::Enumerators),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
	};
	UEnum* Z_Construct_UEnum_LhtTest_ESpecifierTest()
	{
		static UEnum* ReturnEnum = nullptr;
		if (!ReturnEnum)
		{
			UE4CodeGen_Private::ConstructUEnum(ReturnEnum, Z_Construct_UEnum_LhtTest_ESpecifierTest_Statics::EnumParams);
		}
		return ReturnEnum;
	}
	class UScriptStruct* FSpecifierStruct::StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FSpecifierStruct, Z_Construct_UPackage__Script_LhtTest(), TEXT("SpecifierStruct"), sizeof(FSpecifierStruct), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FSpecifierStruct>()
	{
		return FSpecifierStruct::StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FSpecifierStruct_Statics
	{
		static void* NewStructOps();
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FSpecifierStruct_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FSpecifierStruct>();
	}
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FSpecifierStruct_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"SpecifierStruct",
		sizeof(FSpecifierStruct),
		alignof(FSpecifierStruct),
		nullptr,
		0,
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000001),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FSpecifierStruct()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FSpecifierStruct_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	DEFINE_FUNCTION(USpecifierObject::execRun)
	{
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->Run();
		P_NATIVE_END;
	}
	void USpecifierObject::StaticRegisterNativesUSpecifierObject()
	{
		UClass* Class = USpecifierObject::StaticClass();
		static const FNameNativePtrPair Funcs[] = {
			{ "Run", &USpecifierObject::execRun },
		};
		FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
	}
	struct Z_Construct_UFunction_USpecifierObject_Run_Statics
	{
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_USpecifierObject_Run_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_USpecifierObject, nullptr, "Run", nullptr, nullptr, 0, nullptr, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0 };
	UFunction* Z_Construct_UFunction_USpecifierObject_Run()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_USpecifierObject_Run_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	UClass* Z_Construct_UClass_USpecifierObject_NoRegister()
	{
		return USpecifierObject::StaticClass();
	}
	struct Z_Construct_UClass_USpecifierObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FClassFunctionLinkInfo FuncInfo[];
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_Tuned;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_USpecifierObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const FClassFunctionLinkInfo Z_Construct_UClass_USpecifierObject_Statics::FuncInfo[] = {
		{ &Z_Construct_UFunction_USpecifierObject_Run, "Run" },
	};
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UClass_USpecifierObject_Statics::NewProp_Tuned = { "Tuned", nullptr, (EPropertyFlags)0x0010000000000001, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(USpecifierObject, Tuned) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_USpecifierObject_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_USpecifierObject_Statics::NewProp_Tuned,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_USpecifierObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<USpecifierObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_USpecifierObject_Statics::ClassParams = {
		&USpecifierObject::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		FuncInfo,
		Z_Construct_UClass_USpecifierObject_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		UE_ARRAY_COUNT(FuncInfo),
		UE_ARRAY_COUNT(Z_Construct_UClass_USpecifierObject_Statics::PropPointers),
		0,
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_USpecifierObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_USpecifierObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(USpecifierObject, 0);
	template<> LHTTEST_API UClass* StaticClass<USpecifierObject>()
	{
		return USpecifierObject::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(USpecifierObject);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
