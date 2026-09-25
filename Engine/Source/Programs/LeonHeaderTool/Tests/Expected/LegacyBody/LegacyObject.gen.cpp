/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "LegacyObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UClass* Z_Construct_UClass_ULegacyObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_ULegacyObject();
	LHTTEST_API UClass* Z_Construct_UClass_UCustomVTableObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UCustomVTableObject();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	void ULegacyObject::StaticRegisterNativesULegacyObject()
	{
	}
	UClass* Z_Construct_UClass_ULegacyObject_NoRegister()
	{
		return ULegacyObject::StaticClass();
	}
	struct Z_Construct_UClass_ULegacyObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Value;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_ULegacyObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_ULegacyObject_Statics::NewProp_Value = { "Value", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(ULegacyObject, Value) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_ULegacyObject_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ULegacyObject_Statics::NewProp_Value,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_ULegacyObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<ULegacyObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_ULegacyObject_Statics::ClassParams = {
		&ULegacyObject::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_ULegacyObject_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_ULegacyObject_Statics::PropPointers),
		0,
		0x000000A8u,
	};
	UClass* Z_Construct_UClass_ULegacyObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_ULegacyObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(ULegacyObject, 0);
	template<> LHTTEST_API UClass* StaticClass<ULegacyObject>()
	{
		return ULegacyObject::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(ULegacyObject);
	void UCustomVTableObject::StaticRegisterNativesUCustomVTableObject()
	{
	}
	UClass* Z_Construct_UClass_UCustomVTableObject_NoRegister()
	{
		return UCustomVTableObject::StaticClass();
	}
	struct Z_Construct_UClass_UCustomVTableObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UCustomVTableObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UCustomVTableObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UCustomVTableObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UCustomVTableObject_Statics::ClassParams = {
		&UCustomVTableObject::StaticClass,
		"Engine",
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		nullptr,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		0,
		0,
		0x000812A6u,
	};
	UClass* Z_Construct_UClass_UCustomVTableObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UCustomVTableObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UCustomVTableObject, 0);
	template<> LHTTEST_API UClass* StaticClass<UCustomVTableObject>()
	{
		return UCustomVTableObject::StaticClass();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
