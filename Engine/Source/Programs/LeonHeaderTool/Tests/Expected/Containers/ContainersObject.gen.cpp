/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ContainersObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_EContainerKind();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FContainerEntry();
	LHTTEST_API UClass* Z_Construct_UClass_UContainersObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UContainersObject();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject_NoRegister();
	COREUOBJECT_API UClass* Z_Construct_UClass_UClass();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	static UEnum* EContainerKind_StaticEnum()
	{
		static UEnum* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticEnum(Z_Construct_UEnum_LhtTest_EContainerKind, Z_Construct_UPackage__Script_LhtTest(), TEXT("EContainerKind"));
		}
		return Singleton;
	}
	template<> LHTTEST_API UEnum* StaticEnum<EContainerKind>()
	{
		return EContainerKind_StaticEnum();
	}
	struct Z_Construct_UEnum_LhtTest_EContainerKind_Statics
	{
		static const UE4CodeGen_Private::FEnumeratorParam Enumerators[];
		static const UE4CodeGen_Private::FEnumParams EnumParams;
	};
	const UE4CodeGen_Private::FEnumeratorParam Z_Construct_UEnum_LhtTest_EContainerKind_Statics::Enumerators[] = {
		{ "EContainerKind::Small", (int64)EContainerKind::Small },
		{ "EContainerKind::Large", (int64)EContainerKind::Large },
	};
	const UE4CodeGen_Private::FEnumParams Z_Construct_UEnum_LhtTest_EContainerKind_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		"EContainerKind",
		"EContainerKind",
		Z_Construct_UEnum_LhtTest_EContainerKind_Statics::Enumerators,
		UE_ARRAY_COUNT(Z_Construct_UEnum_LhtTest_EContainerKind_Statics::Enumerators),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
	};
	UEnum* Z_Construct_UEnum_LhtTest_EContainerKind()
	{
		static UEnum* ReturnEnum = nullptr;
		if (!ReturnEnum)
		{
			UE4CodeGen_Private::ConstructUEnum(ReturnEnum, Z_Construct_UEnum_LhtTest_EContainerKind_Statics::EnumParams);
		}
		return ReturnEnum;
	}
	class UScriptStruct* FContainerEntry::StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FContainerEntry, Z_Construct_UPackage__Script_LhtTest(), TEXT("ContainerEntry"), sizeof(FContainerEntry), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FContainerEntry>()
	{
		return FContainerEntry::StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FContainerEntry_Statics
	{
		static void* NewStructOps();
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Id;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FContainerEntry_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FContainerEntry>();
	}
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UScriptStruct_FContainerEntry_Statics::NewProp_Id = { "Id", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FContainerEntry, Id) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FContainerEntry_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FContainerEntry_Statics::NewProp_Id,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FContainerEntry_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"ContainerEntry",
		sizeof(FContainerEntry),
		alignof(FContainerEntry),
		Z_Construct_UScriptStruct_FContainerEntry_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FContainerEntry_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000001),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FContainerEntry()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FContainerEntry_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	void UContainersObject::StaticRegisterNativesUContainersObject()
	{
	}
	UClass* Z_Construct_UClass_UContainersObject_NoRegister()
	{
		return UContainersObject::StaticClass();
	}
	struct Z_Construct_UClass_UContainersObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Values_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Values;
		static const UE4CodeGen_Private::FStrPropertyParams NewProp_Names_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Names;
		static const UE4CodeGen_Private::FObjectPropertyParams NewProp_Objects_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Objects;
		static const UE4CodeGen_Private::FClassPropertyParams NewProp_Classes_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Classes;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Counts_ValueProp;
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_Counts_Key_KeyProp;
		static const UE4CodeGen_Private::FMapPropertyParams NewProp_Counts;
		static const UE4CodeGen_Private::FSoftObjectPropertyParams NewProp_Assets_ValueProp;
		static const UE4CodeGen_Private::FStrPropertyParams NewProp_Assets_Key_KeyProp;
		static const UE4CodeGen_Private::FMapPropertyParams NewProp_Assets;
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_Tags_ElementProp;
		static const UE4CodeGen_Private::FSetPropertyParams NewProp_Tags;
		static const UE4CodeGen_Private::FBoolPropertyParams NewProp_Flags_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Flags;
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_Kinds_Inner_Underlying;
		static const UE4CodeGen_Private::FEnumPropertyParams NewProp_Kinds_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Kinds;
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Entries_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Entries;
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_Weights;
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_Slots;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UContainersObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Values_Inner = { "Values", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Values = { "Values", nullptr, (EPropertyFlags)0x0010000000000001, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Values), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FStrPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Names_Inner = { "Names", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Names = { "Names", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Names), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FObjectPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Objects_Inner = { "Objects", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, Z_Construct_UClass_UObject_NoRegister };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Objects = { "Objects", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Objects), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FClassPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Classes_Inner = { "Classes", nullptr, (EPropertyFlags)0x0004000000000000, UE4CodeGen_Private::EPropertyGenFlags::Class, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, Z_Construct_UClass_UObject_NoRegister, Z_Construct_UClass_UClass };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Classes = { "Classes", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Classes), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Counts_ValueProp = { "Counts", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, 1 };
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Counts_Key_KeyProp = { "Counts_Key", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FMapPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Counts = { "Counts", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Map, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Counts), EMapPropertyFlags::None };
	const UE4CodeGen_Private::FSoftObjectPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Assets_ValueProp = { "Assets", nullptr, (EPropertyFlags)0x0004000000000000, UE4CodeGen_Private::EPropertyGenFlags::SoftObject, RF_Public|RF_Transient|RF_MarkAsNative, 1, 1, Z_Construct_UClass_UObject_NoRegister };
	const UE4CodeGen_Private::FStrPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Assets_Key_KeyProp = { "Assets_Key", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FMapPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Assets = { "Assets", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Map, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Assets), EMapPropertyFlags::None };
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Tags_ElementProp = { "Tags", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FSetPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Tags = { "Tags", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Set, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Tags) };
	const UE4CodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Flags_Inner = { "Flags", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Bool | UE4CodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, 1, sizeof(bool), 0, nullptr };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Flags = { "Flags", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Flags), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Kinds_Inner_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, nullptr };
	const UE4CodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Kinds_Inner = { "Kinds", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, Z_Construct_UEnum_LhtTest_EContainerKind };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Kinds = { "Kinds", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Kinds), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Entries_Inner = { "Entries", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, Z_Construct_UScriptStruct_FContainerEntry };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Entries = { "Entries", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UContainersObject, Entries), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Weights = { "Weights", nullptr, (EPropertyFlags)0x0010000000000040, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, CPP_ARRAY_DIM(Weights, UContainersObject), STRUCT_OFFSET(UContainersObject, Weights) };
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UClass_UContainersObject_Statics::NewProp_Slots = { "Slots", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, CPP_ARRAY_DIM(Slots, UContainersObject), STRUCT_OFFSET(UContainersObject, Slots) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UContainersObject_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Values_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Values,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Names_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Names,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Objects_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Objects,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Classes_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Classes,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Counts_ValueProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Counts_Key_KeyProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Counts,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Assets_ValueProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Assets_Key_KeyProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Assets,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Tags_ElementProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Tags,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Flags_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Flags,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Kinds_Inner_Underlying,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Kinds_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Kinds,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Entries_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Entries,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Weights,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UContainersObject_Statics::NewProp_Slots,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UContainersObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UContainersObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UContainersObject_Statics::ClassParams = {
		&UContainersObject::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UContainersObject_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UContainersObject_Statics::PropPointers),
		0,
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_UContainersObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UContainersObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UContainersObject, 0);
	template<> LHTTEST_API UClass* StaticClass<UContainersObject>()
	{
		return UContainersObject::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UContainersObject);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
