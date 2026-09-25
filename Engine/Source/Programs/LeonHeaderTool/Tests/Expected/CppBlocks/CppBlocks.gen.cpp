/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "CppBlocks.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UClass* Z_Construct_UClass_UCppBlocksObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UCppBlocksObject();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	void UCppBlocksObject::StaticRegisterNativesUCppBlocksObject()
	{
	}
	UClass* Z_Construct_UClass_UCppBlocksObject_NoRegister()
	{
		return UCppBlocksObject::StaticClass();
	}
	struct Z_Construct_UClass_UCppBlocksObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_SeenByTheHeaderTool;
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_NotCpp;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UCppBlocksObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UCppBlocksObject_Statics::NewProp_SeenByTheHeaderTool = { "SeenByTheHeaderTool", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UCppBlocksObject, SeenByTheHeaderTool) };
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UClass_UCppBlocksObject_Statics::NewProp_NotCpp = { "NotCpp", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UCppBlocksObject, NotCpp) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UCppBlocksObject_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UCppBlocksObject_Statics::NewProp_SeenByTheHeaderTool,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UCppBlocksObject_Statics::NewProp_NotCpp,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UCppBlocksObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UCppBlocksObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UCppBlocksObject_Statics::ClassParams = {
		&UCppBlocksObject::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UCppBlocksObject_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UCppBlocksObject_Statics::PropPointers),
		0,
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_UCppBlocksObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UCppBlocksObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UCppBlocksObject, 0);
	template<> LHTTEST_API UClass* StaticClass<UCppBlocksObject>()
	{
		return UCppBlocksObject::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UCppBlocksObject);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
