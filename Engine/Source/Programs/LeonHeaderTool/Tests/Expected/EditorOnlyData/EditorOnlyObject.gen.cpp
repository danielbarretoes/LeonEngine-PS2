/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "EditorOnlyObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UClass* Z_Construct_UClass_UEditorOnlyObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UEditorOnlyObject();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FEditorOnlyStruct();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	void UEditorOnlyObject::StaticRegisterNativesUEditorOnlyObject()
	{
	}
	UClass* Z_Construct_UClass_UEditorOnlyObject_NoRegister()
	{
		return UEditorOnlyObject::StaticClass();
	}
	struct Z_Construct_UClass_UEditorOnlyObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_RuntimeValue;
#if WITH_EDITORONLY_DATA
		static const UE4CodeGen_Private::FStrPropertyParams NewProp_EditorNote;
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_EditorTags_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_EditorTags;
#endif // WITH_EDITORONLY_DATA
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_AfterEditor;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UEditorOnlyObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_RuntimeValue = { "RuntimeValue", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UEditorOnlyObject, RuntimeValue) };
#if WITH_EDITORONLY_DATA
	const UE4CodeGen_Private::FStrPropertyParams Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_EditorNote = { "EditorNote", nullptr, (EPropertyFlags)0x0010000800000001, UE4CodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UEditorOnlyObject, EditorNote) };
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_EditorTags_Inner = { "EditorTags", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_EditorTags = { "EditorTags", nullptr, (EPropertyFlags)0x0010000800000000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UEditorOnlyObject, EditorTags), EArrayPropertyFlags::None };
#endif // WITH_EDITORONLY_DATA
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_AfterEditor = { "AfterEditor", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UEditorOnlyObject, AfterEditor) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UEditorOnlyObject_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_RuntimeValue,
#if WITH_EDITORONLY_DATA
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_EditorNote,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_EditorTags_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_EditorTags,
#endif // WITH_EDITORONLY_DATA
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UEditorOnlyObject_Statics::NewProp_AfterEditor,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UEditorOnlyObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UEditorOnlyObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UEditorOnlyObject_Statics::ClassParams = {
		&UEditorOnlyObject::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UEditorOnlyObject_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UEditorOnlyObject_Statics::PropPointers),
		0,
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_UEditorOnlyObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UEditorOnlyObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UEditorOnlyObject, 0);
	template<> LHTTEST_API UClass* StaticClass<UEditorOnlyObject>()
	{
		return UEditorOnlyObject::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UEditorOnlyObject);
	class UScriptStruct* FEditorOnlyStruct::StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FEditorOnlyStruct, Z_Construct_UPackage__Script_LhtTest(), TEXT("EditorOnlyStruct"), sizeof(FEditorOnlyStruct), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FEditorOnlyStruct>()
	{
		return FEditorOnlyStruct::StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics
	{
		static void* NewStructOps();
#if WITH_EDITORONLY_DATA
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_OnlyInEditor;
		static void NewProp_bEditorFlag_SetBit(void* Obj);
		static const UE4CodeGen_Private::FBoolPropertyParams NewProp_bEditorFlag;
#endif // WITH_EDITORONLY_DATA
#if WITH_EDITORONLY_DATA
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
#endif
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FEditorOnlyStruct>();
	}
#if WITH_EDITORONLY_DATA
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::NewProp_OnlyInEditor = { "OnlyInEditor", nullptr, (EPropertyFlags)0x0010000800000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FEditorOnlyStruct, OnlyInEditor) };
	void Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::NewProp_bEditorFlag_SetBit(void* Obj)
	{
		((FEditorOnlyStruct*)Obj)->bEditorFlag = 1;
	}
	const UE4CodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::NewProp_bEditorFlag = { "bEditorFlag", nullptr, (EPropertyFlags)0x0010000800000000, UE4CodeGen_Private::EPropertyGenFlags::Bool | UE4CodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, 1, sizeof(bool), sizeof(FEditorOnlyStruct), &Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::NewProp_bEditorFlag_SetBit };
#endif // WITH_EDITORONLY_DATA
#if WITH_EDITORONLY_DATA
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::NewProp_OnlyInEditor,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::NewProp_bEditorFlag,
	};
#endif
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"EditorOnlyStruct",
		sizeof(FEditorOnlyStruct),
		alignof(FEditorOnlyStruct),
#if WITH_EDITORONLY_DATA
		Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::PropPointers),
#else
		nullptr,
		0,
#endif
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000001),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FEditorOnlyStruct()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FEditorOnlyStruct_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
