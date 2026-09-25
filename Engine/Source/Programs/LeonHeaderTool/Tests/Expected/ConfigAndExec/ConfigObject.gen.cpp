/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ConfigObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UClass* Z_Construct_UClass_UGameSettings_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UGameSettings();
	LHTTEST_API UClass* Z_Construct_UClass_UChildSettings_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UChildSettings();
	LHTTEST_API UClass* Z_Construct_UClass_UBindingSettings_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UBindingSettings();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	DEFINE_FUNCTION(UGameSettings::execSetSensitivity)
	{
		P_GET_PROPERTY(FFloatProperty,Z_Param_NewSensitivity);
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->SetSensitivity(Z_Param_NewSensitivity);
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UGameSettings::execAddMap)
	{
		P_GET_PROPERTY(FNameProperty,Z_Param_MapName);
		P_GET_PROPERTY(FStrProperty,Z_Param_Description);
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->AddMap(Z_Param_MapName,Z_Param_Description);
		P_NATIVE_END;
	}
	void UGameSettings::StaticRegisterNativesUGameSettings()
	{
		UClass* Class = UGameSettings::StaticClass();
		static const FNameNativePtrPair Funcs[] = {
			{ "SetSensitivity", &UGameSettings::execSetSensitivity },
			{ "AddMap", &UGameSettings::execAddMap },
		};
		FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
	}
	struct Z_Construct_UFunction_UGameSettings_SetSensitivity_Statics
	{
		struct GameSettings_eventSetSensitivity_Parms
		{
			float NewSensitivity;
		};
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_NewSensitivity;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UGameSettings_SetSensitivity_Statics::NewProp_NewSensitivity = { "NewSensitivity", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(GameSettings_eventSetSensitivity_Parms, NewSensitivity) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameSettings_SetSensitivity_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameSettings_SetSensitivity_Statics::NewProp_NewSensitivity,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameSettings_SetSensitivity_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameSettings, nullptr, "SetSensitivity", nullptr, nullptr, sizeof(Z_Construct_UFunction_UGameSettings_SetSensitivity_Statics::GameSettings_eventSetSensitivity_Parms), Z_Construct_UFunction_UGameSettings_SetSensitivity_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameSettings_SetSensitivity_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00020601, 0, 0 };
	UFunction* Z_Construct_UFunction_UGameSettings_SetSensitivity()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UGameSettings_SetSensitivity_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UGameSettings_AddMap_Statics
	{
		struct GameSettings_eventAddMap_Parms
		{
			FName MapName;
			FString Description;
		};
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_MapName;
		static const UE4CodeGen_Private::FStrPropertyParams NewProp_Description;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UFunction_UGameSettings_AddMap_Statics::NewProp_MapName = { "MapName", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(GameSettings_eventAddMap_Parms, MapName) };
	const UE4CodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UGameSettings_AddMap_Statics::NewProp_Description = { "Description", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(GameSettings_eventAddMap_Parms, Description) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UGameSettings_AddMap_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameSettings_AddMap_Statics::NewProp_MapName,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UGameSettings_AddMap_Statics::NewProp_Description,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UGameSettings_AddMap_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UGameSettings, nullptr, "AddMap", nullptr, nullptr, sizeof(Z_Construct_UFunction_UGameSettings_AddMap_Statics::GameSettings_eventAddMap_Parms), Z_Construct_UFunction_UGameSettings_AddMap_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UGameSettings_AddMap_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00020601, 0, 0 };
	UFunction* Z_Construct_UFunction_UGameSettings_AddMap()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UGameSettings_AddMap_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	UClass* Z_Construct_UClass_UGameSettings_NoRegister()
	{
		return UGameSettings::StaticClass();
	}
	struct Z_Construct_UClass_UGameSettings_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FClassFunctionLinkInfo FuncInfo[];
		static const UE4CodeGen_Private::FStrPropertyParams NewProp_MapCycle_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_MapCycle;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Slots;
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_MouseSensitivity;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_StartMoney_ValueProp;
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_StartMoney_Key_KeyProp;
		static const UE4CodeGen_Private::FMapPropertyParams NewProp_StartMoney;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_RuntimeOnly;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UGameSettings_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const FClassFunctionLinkInfo Z_Construct_UClass_UGameSettings_Statics::FuncInfo[] = {
		{ &Z_Construct_UFunction_UGameSettings_SetSensitivity, "SetSensitivity" },
		{ &Z_Construct_UFunction_UGameSettings_AddMap, "AddMap" },
	};
	const UE4CodeGen_Private::FStrPropertyParams Z_Construct_UClass_UGameSettings_Statics::NewProp_MapCycle_Inner = { "MapCycle", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UGameSettings_Statics::NewProp_MapCycle = { "MapCycle", nullptr, (EPropertyFlags)0x0010000000004000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UGameSettings, MapCycle), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UGameSettings_Statics::NewProp_Slots = { "Slots", nullptr, (EPropertyFlags)0x0010000000004000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, CPP_ARRAY_DIM(Slots, UGameSettings), STRUCT_OFFSET(UGameSettings, Slots) };
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UClass_UGameSettings_Statics::NewProp_MouseSensitivity = { "MouseSensitivity", nullptr, (EPropertyFlags)0x0010000000044000, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UGameSettings, MouseSensitivity) };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UGameSettings_Statics::NewProp_StartMoney_ValueProp = { "StartMoney", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, 1 };
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UClass_UGameSettings_Statics::NewProp_StartMoney_Key_KeyProp = { "StartMoney_Key", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FMapPropertyParams Z_Construct_UClass_UGameSettings_Statics::NewProp_StartMoney = { "StartMoney", nullptr, (EPropertyFlags)0x0010000000004001, UE4CodeGen_Private::EPropertyGenFlags::Map, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UGameSettings, StartMoney), EMapPropertyFlags::None };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UClass_UGameSettings_Statics::NewProp_RuntimeOnly = { "RuntimeOnly", nullptr, (EPropertyFlags)0x0010000000002000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UGameSettings, RuntimeOnly) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UGameSettings_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameSettings_Statics::NewProp_MapCycle_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameSettings_Statics::NewProp_MapCycle,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameSettings_Statics::NewProp_Slots,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameSettings_Statics::NewProp_MouseSensitivity,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameSettings_Statics::NewProp_StartMoney_ValueProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameSettings_Statics::NewProp_StartMoney_Key_KeyProp,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameSettings_Statics::NewProp_StartMoney,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UGameSettings_Statics::NewProp_RuntimeOnly,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UGameSettings_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UGameSettings>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UGameSettings_Statics::ClassParams = {
		&UGameSettings::StaticClass,
		"Game",
		&StaticCppClassTypeInfo,
		DependentSingletons,
		FuncInfo,
		Z_Construct_UClass_UGameSettings_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		UE_ARRAY_COUNT(FuncInfo),
		UE_ARRAY_COUNT(Z_Construct_UClass_UGameSettings_Statics::PropPointers),
		0,
		0x000000A6u,
	};
	UClass* Z_Construct_UClass_UGameSettings()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UGameSettings_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UGameSettings, 0);
	template<> LHTTEST_API UClass* StaticClass<UGameSettings>()
	{
		return UGameSettings::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UGameSettings);
	void UChildSettings::StaticRegisterNativesUChildSettings()
	{
	}
	UClass* Z_Construct_UClass_UChildSettings_NoRegister()
	{
		return UChildSettings::StaticClass();
	}
	struct Z_Construct_UClass_UChildSettings_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static void NewProp_bHardcore_SetBit(void* Obj);
		static const UE4CodeGen_Private::FBoolPropertyParams NewProp_bHardcore;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UChildSettings_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UGameSettings,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	void Z_Construct_UClass_UChildSettings_Statics::NewProp_bHardcore_SetBit(void* Obj)
	{
		((UChildSettings*)Obj)->bHardcore = 1;
	}
	const UE4CodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UChildSettings_Statics::NewProp_bHardcore = { "bHardcore", nullptr, (EPropertyFlags)0x0010000000004000, UE4CodeGen_Private::EPropertyGenFlags::Bool | UE4CodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, 1, sizeof(bool), sizeof(UChildSettings), &Z_Construct_UClass_UChildSettings_Statics::NewProp_bHardcore_SetBit };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UChildSettings_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UChildSettings_Statics::NewProp_bHardcore,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UChildSettings_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UChildSettings>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UChildSettings_Statics::ClassParams = {
		&UChildSettings::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UChildSettings_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UChildSettings_Statics::PropPointers),
		0,
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_UChildSettings()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UChildSettings_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UChildSettings, 0);
	template<> LHTTEST_API UClass* StaticClass<UChildSettings>()
	{
		return UChildSettings::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UChildSettings);
	void UBindingSettings::StaticRegisterNativesUBindingSettings()
	{
	}
	UClass* Z_Construct_UClass_UBindingSettings_NoRegister()
	{
		return UBindingSettings::StaticClass();
	}
	struct Z_Construct_UClass_UBindingSettings_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_Action;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UBindingSettings_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UClass_UBindingSettings_Statics::NewProp_Action = { "Action", nullptr, (EPropertyFlags)0x0010000000004000, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(UBindingSettings, Action) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UBindingSettings_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UBindingSettings_Statics::NewProp_Action,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UBindingSettings_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UBindingSettings>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UBindingSettings_Statics::ClassParams = {
		&UBindingSettings::StaticClass,
		"Input",
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UBindingSettings_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UBindingSettings_Statics::PropPointers),
		0,
		0x000004A4u,
	};
	UClass* Z_Construct_UClass_UBindingSettings()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UBindingSettings_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UBindingSettings, 0);
	template<> LHTTEST_API UClass* StaticClass<UBindingSettings>()
	{
		return UBindingSettings::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UBindingSettings);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
