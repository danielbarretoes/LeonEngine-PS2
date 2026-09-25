/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "BasicObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UClass* Z_Construct_UClass_UBasicObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UBasicObject();
	LHTTEST_API UClass* Z_Construct_UClass_UPlainObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UPlainObject();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	void UBasicObject::StaticRegisterNativesUBasicObject()
	{
	}
	UClass* Z_Construct_UClass_UBasicObject_NoRegister()
	{
		return UBasicObject::StaticClass();
	}
	struct Z_Construct_UClass_UBasicObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_Health;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_MaxAmmo;
		static void NewProp_bIsAlive_SetBit(void* Obj);
		static const UE4CodeGen_Private::FBoolPropertyParams NewProp_bIsAlive;
		static void NewProp_bNativeFlag_SetBit(void* Obj);
		static const UE4CodeGen_Private::FBoolPropertyParams NewProp_bNativeFlag;
		static const UE4CodeGen_Private::FDoublePropertyParams NewProp_Precise;
		static const UE4CodeGen_Private::FInt8PropertyParams NewProp_Small;
		static const UE4CodeGen_Private::FInt16PropertyParams NewProp_Medium;
		static const UE4CodeGen_Private::FInt64PropertyParams NewProp_Large;
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_Byte;
		static const UE4CodeGen_Private::FUInt16PropertyParams NewProp_UnsignedMedium;
		static const UE4CodeGen_Private::FUInt32PropertyParams NewProp_UnsignedValue;
		static const UE4CodeGen_Private::FUInt64PropertyParams NewProp_UnsignedLarge;
		static const UE4CodeGen_Private::FStrPropertyParams NewProp_DisplayName;
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_Tag;
		static const UE4CodeGen_Private::FTextPropertyParams NewProp_Title;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_ProtectedValue;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_PrivateCounter;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UBasicObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_Health = { "Health", nullptr, (EPropertyFlags)0x0010000000000005, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, Health) };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_MaxAmmo = { "MaxAmmo", nullptr, (EPropertyFlags)0x0010000000004000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, MaxAmmo) };
	void Z_Construct_UClass_UBasicObject_Statics::NewProp_bIsAlive_SetBit(void* Obj)
	{
		((UBasicObject*)Obj)->bIsAlive = 1;
	}
	const UE4CodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_bIsAlive = { "bIsAlive", nullptr, (EPropertyFlags)0x0010000000020001, UE4CodeGen_Private::EPropertyGenFlags::Bool, RF_Public|RF_Transient|RF_MarkAsNative, 1, sizeof(uint8), sizeof(UBasicObject), &Z_Construct_UClass_UBasicObject_Statics::NewProp_bIsAlive_SetBit };
	void Z_Construct_UClass_UBasicObject_Statics::NewProp_bNativeFlag_SetBit(void* Obj)
	{
		((UBasicObject*)Obj)->bNativeFlag = 1;
	}
	const UE4CodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_bNativeFlag = { "bNativeFlag", nullptr, (EPropertyFlags)0x0010000000044000, UE4CodeGen_Private::EPropertyGenFlags::Bool | UE4CodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, 1, sizeof(bool), sizeof(UBasicObject), &Z_Construct_UClass_UBasicObject_Statics::NewProp_bNativeFlag_SetBit };
	const UE4CodeGen_Private::FDoublePropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_Precise = { "Precise", nullptr, (EPropertyFlags)0x0010000000002000, UE4CodeGen_Private::EPropertyGenFlags::Double, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, Precise) };
	const UE4CodeGen_Private::FInt8PropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_Small = { "Small", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int8, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, Small) };
	const UE4CodeGen_Private::FInt16PropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_Medium = { "Medium", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int16, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, Medium) };
	const UE4CodeGen_Private::FInt64PropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_Large = { "Large", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, Large) };
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_Byte = { "Byte", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, Byte), nullptr };
	const UE4CodeGen_Private::FUInt16PropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_UnsignedMedium = { "UnsignedMedium", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::UInt16, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, UnsignedMedium) };
	const UE4CodeGen_Private::FUInt32PropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_UnsignedValue = { "UnsignedValue", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::UInt32, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, UnsignedValue) };
	const UE4CodeGen_Private::FUInt64PropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_UnsignedLarge = { "UnsignedLarge", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::UInt64, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, UnsignedLarge) };
	const UE4CodeGen_Private::FStrPropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_DisplayName = { "DisplayName", nullptr, (EPropertyFlags)0x0010000001010001, UE4CodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, DisplayName) };
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_Tag = { "Tag", nullptr, (EPropertyFlags)0x0010000000200000, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, Tag) };
	const UE4CodeGen_Private::FTextPropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_Title = { "Title", nullptr, (EPropertyFlags)0x0010040000030001, UE4CodeGen_Private::EPropertyGenFlags::Text, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, Title) };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_ProtectedValue = { "ProtectedValue", nullptr, (EPropertyFlags)0x0020080002000801, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, ProtectedValue) };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UBasicObject_Statics::NewProp_PrivateCounter = { "PrivateCounter", nullptr, (EPropertyFlags)0x0040000000020815, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBasicObject, PrivateCounter) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UBasicObject_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_Health,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_MaxAmmo,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_bIsAlive,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_bNativeFlag,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_Precise,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_Small,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_Medium,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_Large,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_Byte,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_UnsignedMedium,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_UnsignedValue,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_UnsignedLarge,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_DisplayName,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_Tag,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_Title,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_ProtectedValue,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBasicObject_Statics::NewProp_PrivateCounter,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UBasicObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UBasicObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UBasicObject_Statics::ClassParams = {
		&UBasicObject::StaticClass,
		"Game",
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UBasicObject_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UBasicObject_Statics::PropPointers),
		0,
		0x001000ADu,
	};
	UClass* Z_Construct_UClass_UBasicObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UBasicObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UBasicObject, 0);
	template<> LHTTEST_API UClass* StaticClass<UBasicObject>()
	{
		return UBasicObject::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UBasicObject);
	void UPlainObject::StaticRegisterNativesUPlainObject()
	{
	}
	UClass* Z_Construct_UClass_UPlainObject_NoRegister()
	{
		return UPlainObject::StaticClass();
	}
	struct Z_Construct_UClass_UPlainObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UPlainObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UPlainObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UPlainObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UPlainObject_Statics::ClassParams = {
		&UPlainObject::StaticClass,
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
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_UPlainObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UPlainObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UPlainObject, 0);
	template<> LHTTEST_API UClass* StaticClass<UPlainObject>()
	{
		return UPlainObject::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UPlainObject);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
