/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "FunctionsObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FHitInfo();
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_EFireMode();
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_ELegacyChannel();
	LHTTEST_API UClass* Z_Construct_UClass_UFunctionsObject_NoRegister();
	LHTTEST_API UClass* Z_Construct_UClass_UFunctionsObject();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject_NoRegister();
	COREUOBJECT_API UClass* Z_Construct_UClass_UClass();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	class UScriptStruct* FHitInfo::StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FHitInfo, Z_Construct_UPackage__Script_LhtTest(), TEXT("HitInfo"), sizeof(FHitInfo), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FHitInfo>()
	{
		return FHitInfo::StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FHitInfo_Statics
	{
		static void* NewStructOps();
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_Distance;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FHitInfo_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FHitInfo>();
	}
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FHitInfo_Statics::NewProp_Distance = { "Distance", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FHitInfo, Distance) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FHitInfo_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHitInfo_Statics::NewProp_Distance,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FHitInfo_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"HitInfo",
		sizeof(FHitInfo),
		alignof(FHitInfo),
		Z_Construct_UScriptStruct_FHitInfo_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHitInfo_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000001),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FHitInfo()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FHitInfo_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	static UEnum* EFireMode_StaticEnum()
	{
		static UEnum* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticEnum(Z_Construct_UEnum_LhtTest_EFireMode, Z_Construct_UPackage__Script_LhtTest(), TEXT("EFireMode"));
		}
		return Singleton;
	}
	template<> LHTTEST_API UEnum* StaticEnum<EFireMode>()
	{
		return EFireMode_StaticEnum();
	}
	struct Z_Construct_UEnum_LhtTest_EFireMode_Statics
	{
		static const UE4CodeGen_Private::FEnumeratorParam Enumerators[];
		static const UE4CodeGen_Private::FEnumParams EnumParams;
	};
	const UE4CodeGen_Private::FEnumeratorParam Z_Construct_UEnum_LhtTest_EFireMode_Statics::Enumerators[] = {
		{ "EFireMode::Single", (int64)EFireMode::Single },
		{ "EFireMode::Burst", (int64)EFireMode::Burst },
		{ "EFireMode::Auto", (int64)EFireMode::Auto },
	};
	const UE4CodeGen_Private::FEnumParams Z_Construct_UEnum_LhtTest_EFireMode_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		"EFireMode",
		"EFireMode",
		Z_Construct_UEnum_LhtTest_EFireMode_Statics::Enumerators,
		UE_ARRAY_COUNT(Z_Construct_UEnum_LhtTest_EFireMode_Statics::Enumerators),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
	};
	UEnum* Z_Construct_UEnum_LhtTest_EFireMode()
	{
		static UEnum* ReturnEnum = nullptr;
		if (!ReturnEnum)
		{
			UE4CodeGen_Private::ConstructUEnum(ReturnEnum, Z_Construct_UEnum_LhtTest_EFireMode_Statics::EnumParams);
		}
		return ReturnEnum;
	}
	static UEnum* ELegacyChannel_StaticEnum()
	{
		static UEnum* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticEnum(Z_Construct_UEnum_LhtTest_ELegacyChannel, Z_Construct_UPackage__Script_LhtTest(), TEXT("ELegacyChannel"));
		}
		return Singleton;
	}
	template<> LHTTEST_API UEnum* StaticEnum<ELegacyChannel>()
	{
		return ELegacyChannel_StaticEnum();
	}
	struct Z_Construct_UEnum_LhtTest_ELegacyChannel_Statics
	{
		static const UE4CodeGen_Private::FEnumeratorParam Enumerators[];
		static const UE4CodeGen_Private::FEnumParams EnumParams;
	};
	const UE4CodeGen_Private::FEnumeratorParam Z_Construct_UEnum_LhtTest_ELegacyChannel_Statics::Enumerators[] = {
		{ "LC_Visibility", (int64)LC_Visibility },
		{ "LC_Camera", (int64)LC_Camera },
	};
	const UE4CodeGen_Private::FEnumParams Z_Construct_UEnum_LhtTest_ELegacyChannel_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		"ELegacyChannel",
		"ELegacyChannel",
		Z_Construct_UEnum_LhtTest_ELegacyChannel_Statics::Enumerators,
		UE_ARRAY_COUNT(Z_Construct_UEnum_LhtTest_ELegacyChannel_Statics::Enumerators),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::Regular,
	};
	UEnum* Z_Construct_UEnum_LhtTest_ELegacyChannel()
	{
		static UEnum* ReturnEnum = nullptr;
		if (!ReturnEnum)
		{
			UE4CodeGen_Private::ConstructUEnum(ReturnEnum, Z_Construct_UEnum_LhtTest_ELegacyChannel_Statics::EnumParams);
		}
		return ReturnEnum;
	}
	DEFINE_FUNCTION(UFunctionsObject::execGiveAmmo)
	{
		P_GET_PROPERTY(FIntProperty,Z_Param_Amount);
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->GiveAmmo(Z_Param_Amount);
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UFunctionsObject::execFire)
	{
		P_GET_ENUM(EFireMode,Z_Param_Mode);
		P_GET_PROPERTY(FStrProperty,Z_Param_Reason);
		P_GET_PROPERTY(FFloatProperty,Z_Param_Spread);
		P_FINISH;
		P_NATIVE_BEGIN;
		*(bool*)Z_Param__Result=P_THIS->Fire(EFireMode(Z_Param_Mode),Z_Param_Reason,Z_Param_Spread);
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UFunctionsObject::execTraceAt)
	{
		P_GET_STRUCT_REF(FHitInfo,Z_Param_Out_Previous);
		P_GET_OBJECT(UObject,Z_Param_Instigator);
		P_FINISH;
		P_NATIVE_BEGIN;
		*(FHitInfo*)Z_Param__Result=P_THIS->TraceAt(Z_Param_Out_Previous,Z_Param_Instigator);
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UFunctionsObject::execAddLarge)
	{
		P_GET_PROPERTY(FInt64Property,Z_Param_A);
		P_GET_PROPERTY(FInt64Property,Z_Param_B);
		P_FINISH;
		P_NATIVE_BEGIN;
		*(int64*)Z_Param__Result=UFunctionsObject::AddLarge(Z_Param_A,Z_Param_B);
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UFunctionsObject::execOnHit)
	{
		P_GET_OBJECT(UClass,Z_Param_DamageType);
		P_GET_PROPERTY(FNameProperty,Z_Param_Bone);
		P_GET_TARRAY_REF(int32,Z_Param_Out_Indices);
		P_GET_PROPERTY(FByteProperty,Z_Param_Channel);
		P_GET_UBOOL(Z_Param_bCritical);
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->OnHit(Z_Param_DamageType,Z_Param_Bone,Z_Param_Out_Indices,TEnumAsByte<ELegacyChannel>(Z_Param_Channel),Z_Param_bCritical);
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UFunctionsObject::execGetCount)
	{
		P_FINISH;
		P_NATIVE_BEGIN;
		*(int32*)Z_Param__Result=P_THIS->GetCount();
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UFunctionsObject::execReset)
	{
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->Reset();
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UFunctionsObject::execGetTitle)
	{
		P_FINISH;
		P_NATIVE_BEGIN;
		*(FText*)Z_Param__Result=P_THIS->GetTitle();
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UFunctionsObject::execGetByte)
	{
		P_FINISH;
		P_NATIVE_BEGIN;
		*(uint8*)Z_Param__Result=P_THIS->GetByte();
		P_NATIVE_END;
	}
	void UFunctionsObject::StaticRegisterNativesUFunctionsObject()
	{
		UClass* Class = UFunctionsObject::StaticClass();
		static const FNameNativePtrPair Funcs[] = {
			{ "GiveAmmo", &UFunctionsObject::execGiveAmmo },
			{ "Fire", &UFunctionsObject::execFire },
			{ "TraceAt", &UFunctionsObject::execTraceAt },
			{ "AddLarge", &UFunctionsObject::execAddLarge },
			{ "OnHit", &UFunctionsObject::execOnHit },
			{ "GetCount", &UFunctionsObject::execGetCount },
			{ "Reset", &UFunctionsObject::execReset },
			{ "GetTitle", &UFunctionsObject::execGetTitle },
			{ "GetByte", &UFunctionsObject::execGetByte },
		};
		FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
	}
	struct Z_Construct_UFunction_UFunctionsObject_GiveAmmo_Statics
	{
		struct FunctionsObject_eventGiveAmmo_Parms
		{
			int32 Amount;
		};
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Amount;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UFunction_UFunctionsObject_GiveAmmo_Statics::NewProp_Amount = { "Amount", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventGiveAmmo_Parms, Amount) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UFunctionsObject_GiveAmmo_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_GiveAmmo_Statics::NewProp_Amount,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UFunctionsObject_GiveAmmo_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UFunctionsObject, nullptr, "GiveAmmo", nullptr, nullptr, sizeof(Z_Construct_UFunction_UFunctionsObject_GiveAmmo_Statics::FunctionsObject_eventGiveAmmo_Parms), Z_Construct_UFunction_UFunctionsObject_GiveAmmo_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UFunctionsObject_GiveAmmo_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00020601, 0, 0 };
	UFunction* Z_Construct_UFunction_UFunctionsObject_GiveAmmo()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UFunctionsObject_GiveAmmo_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UFunctionsObject_Fire_Statics
	{
		struct FunctionsObject_eventFire_Parms
		{
			EFireMode Mode;
			FString Reason;
			float Spread;
			bool ReturnValue;
		};
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_Mode_Underlying;
		static const UE4CodeGen_Private::FEnumPropertyParams NewProp_Mode;
		static const UE4CodeGen_Private::FStrPropertyParams NewProp_Reason;
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_Spread;
		static void NewProp_ReturnValue_SetBit(void* Obj);
		static const UE4CodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_Mode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, nullptr };
	const UE4CodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_Mode = { "Mode", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventFire_Parms, Mode), Z_Construct_UEnum_LhtTest_EFireMode };
	const UE4CodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_Reason = { "Reason", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventFire_Parms, Reason) };
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_Spread = { "Spread", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventFire_Parms, Spread) };
	void Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_ReturnValue_SetBit(void* Obj)
	{
		((FunctionsObject_eventFire_Parms*)Obj)->ReturnValue = 1;
	}
	const UE4CodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UE4CodeGen_Private::EPropertyGenFlags::Bool | UE4CodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, 1, sizeof(bool), sizeof(FunctionsObject_eventFire_Parms), &Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_ReturnValue_SetBit };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UFunctionsObject_Fire_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_Mode_Underlying,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_Mode,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_Reason,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_Spread,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_Fire_Statics::NewProp_ReturnValue,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UFunctionsObject_Fire_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UFunctionsObject, nullptr, "Fire", nullptr, nullptr, sizeof(Z_Construct_UFunction_UFunctionsObject_Fire_Statics::FunctionsObject_eventFire_Parms), Z_Construct_UFunction_UFunctionsObject_Fire_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UFunctionsObject_Fire_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0 };
	UFunction* Z_Construct_UFunction_UFunctionsObject_Fire()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UFunctionsObject_Fire_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics
	{
		struct FunctionsObject_eventTraceAt_Parms
		{
			FHitInfo Previous;
			UObject* Instigator;
			FHitInfo ReturnValue;
		};
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Previous;
		static const UE4CodeGen_Private::FObjectPropertyParams NewProp_Instigator;
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_ReturnValue;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::NewProp_Previous = { "Previous", nullptr, (EPropertyFlags)0x0010000008000182, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventTraceAt_Parms, Previous), Z_Construct_UScriptStruct_FHitInfo };
	const UE4CodeGen_Private::FObjectPropertyParams Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::NewProp_Instigator = { "Instigator", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventTraceAt_Parms, Instigator), Z_Construct_UClass_UObject_NoRegister };
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventTraceAt_Parms, ReturnValue), Z_Construct_UScriptStruct_FHitInfo };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::NewProp_Previous,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::NewProp_Instigator,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::NewProp_ReturnValue,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UFunctionsObject, nullptr, "TraceAt", nullptr, nullptr, sizeof(Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::FunctionsObject_eventTraceAt_Parms), Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54420401, 0, 0 };
	UFunction* Z_Construct_UFunction_UFunctionsObject_TraceAt()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UFunctionsObject_TraceAt_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics
	{
		struct FunctionsObject_eventAddLarge_Parms
		{
			int64 A;
			int64 B;
			int64 ReturnValue;
		};
		static const UE4CodeGen_Private::FInt64PropertyParams NewProp_A;
		static const UE4CodeGen_Private::FInt64PropertyParams NewProp_B;
		static const UE4CodeGen_Private::FInt64PropertyParams NewProp_ReturnValue;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::NewProp_A = { "A", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventAddLarge_Parms, A) };
	const UE4CodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::NewProp_B = { "B", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventAddLarge_Parms, B) };
	const UE4CodeGen_Private::FInt64PropertyParams Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UE4CodeGen_Private::EPropertyGenFlags::Int64, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventAddLarge_Parms, ReturnValue) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::NewProp_A,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::NewProp_B,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::NewProp_ReturnValue,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UFunctionsObject, nullptr, "AddLarge", nullptr, nullptr, sizeof(Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::FunctionsObject_eventAddLarge_Parms), Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00022401, 0, 0 };
	UFunction* Z_Construct_UFunction_UFunctionsObject_AddLarge()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UFunctionsObject_AddLarge_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UFunctionsObject_OnHit_Statics
	{
		struct FunctionsObject_eventOnHit_Parms
		{
			TSubclassOf<UObject> DamageType;
			FName Bone;
			TArray<int32> Indices;
			TEnumAsByte<ELegacyChannel> Channel;
			bool bCritical;
		};
		static const UE4CodeGen_Private::FClassPropertyParams NewProp_DamageType;
		static const UE4CodeGen_Private::FNamePropertyParams NewProp_Bone;
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_Indices_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Indices;
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_Channel;
		static void NewProp_bCritical_SetBit(void* Obj);
		static const UE4CodeGen_Private::FBoolPropertyParams NewProp_bCritical;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FClassPropertyParams Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_DamageType = { "DamageType", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Class, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventOnHit_Parms, DamageType), Z_Construct_UClass_UObject_NoRegister, Z_Construct_UClass_UClass };
	const UE4CodeGen_Private::FNamePropertyParams Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_Bone = { "Bone", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventOnHit_Parms, Bone) };
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_Indices_Inner = { "Indices", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0 };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_Indices = { "Indices", nullptr, (EPropertyFlags)0x0010000008000182, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventOnHit_Parms, Indices), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_Channel = { "Channel", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventOnHit_Parms, Channel), Z_Construct_UEnum_LhtTest_ELegacyChannel };
	void Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_bCritical_SetBit(void* Obj)
	{
		((FunctionsObject_eventOnHit_Parms*)Obj)->bCritical = 1;
	}
	const UE4CodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_bCritical = { "bCritical", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Bool | UE4CodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, 1, sizeof(bool), sizeof(FunctionsObject_eventOnHit_Parms), &Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_bCritical_SetBit };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_DamageType,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_Bone,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_Indices_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_Indices,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_Channel,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::NewProp_bCritical,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UFunctionsObject, nullptr, "OnHit", nullptr, nullptr, sizeof(Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::FunctionsObject_eventOnHit_Parms), Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00420400, 0, 0 };
	UFunction* Z_Construct_UFunction_UFunctionsObject_OnHit()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UFunctionsObject_OnHit_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UFunctionsObject_GetCount_Statics
	{
		struct FunctionsObject_eventGetCount_Parms
		{
			int32 ReturnValue;
		};
		static const UE4CodeGen_Private::FIntPropertyParams NewProp_ReturnValue;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FIntPropertyParams Z_Construct_UFunction_UFunctionsObject_GetCount_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UE4CodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventGetCount_Parms, ReturnValue) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UFunctionsObject_GetCount_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_GetCount_Statics::NewProp_ReturnValue,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UFunctionsObject_GetCount_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UFunctionsObject, nullptr, "GetCount", nullptr, nullptr, sizeof(Z_Construct_UFunction_UFunctionsObject_GetCount_Statics::FunctionsObject_eventGetCount_Parms), Z_Construct_UFunction_UFunctionsObject_GetCount_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UFunctionsObject_GetCount_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54020401, 0, 0 };
	UFunction* Z_Construct_UFunction_UFunctionsObject_GetCount()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UFunctionsObject_GetCount_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UFunctionsObject_Reset_Statics
	{
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UFunctionsObject_Reset_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UFunctionsObject, nullptr, "Reset", nullptr, nullptr, 0, nullptr, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00020401, 0, 0 };
	UFunction* Z_Construct_UFunction_UFunctionsObject_Reset()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UFunctionsObject_Reset_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UFunctionsObject_GetTitle_Statics
	{
		struct FunctionsObject_eventGetTitle_Parms
		{
			FText ReturnValue;
		};
		static const UE4CodeGen_Private::FTextPropertyParams NewProp_ReturnValue;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FTextPropertyParams Z_Construct_UFunction_UFunctionsObject_GetTitle_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UE4CodeGen_Private::EPropertyGenFlags::Text, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventGetTitle_Parms, ReturnValue) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UFunctionsObject_GetTitle_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_GetTitle_Statics::NewProp_ReturnValue,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UFunctionsObject_GetTitle_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UFunctionsObject, nullptr, "GetTitle", nullptr, nullptr, sizeof(Z_Construct_UFunction_UFunctionsObject_GetTitle_Statics::FunctionsObject_eventGetTitle_Parms), Z_Construct_UFunction_UFunctionsObject_GetTitle_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UFunctionsObject_GetTitle_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x40080401, 0, 0 };
	UFunction* Z_Construct_UFunction_UFunctionsObject_GetTitle()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UFunctionsObject_GetTitle_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UFunctionsObject_GetByte_Statics
	{
		struct FunctionsObject_eventGetByte_Parms
		{
			uint8 ReturnValue;
		};
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_ReturnValue;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UFunction_UFunctionsObject_GetByte_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FunctionsObject_eventGetByte_Parms, ReturnValue), nullptr };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UFunctionsObject_GetByte_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UFunctionsObject_GetByte_Statics::NewProp_ReturnValue,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_UFunctionsObject_GetByte_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UFunctionsObject, nullptr, "GetByte", nullptr, nullptr, sizeof(Z_Construct_UFunction_UFunctionsObject_GetByte_Statics::FunctionsObject_eventGetByte_Parms), Z_Construct_UFunction_UFunctionsObject_GetByte_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UFunctionsObject_GetByte_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04040401, 0, 0 };
	UFunction* Z_Construct_UFunction_UFunctionsObject_GetByte()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_UFunctionsObject_GetByte_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	UClass* Z_Construct_UClass_UFunctionsObject_NoRegister()
	{
		return UFunctionsObject::StaticClass();
	}
	struct Z_Construct_UClass_UFunctionsObject_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FClassFunctionLinkInfo FuncInfo[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UFunctionsObject_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
	};
	const FClassFunctionLinkInfo Z_Construct_UClass_UFunctionsObject_Statics::FuncInfo[] = {
		{ &Z_Construct_UFunction_UFunctionsObject_GiveAmmo, "GiveAmmo" },
		{ &Z_Construct_UFunction_UFunctionsObject_Fire, "Fire" },
		{ &Z_Construct_UFunction_UFunctionsObject_TraceAt, "TraceAt" },
		{ &Z_Construct_UFunction_UFunctionsObject_AddLarge, "AddLarge" },
		{ &Z_Construct_UFunction_UFunctionsObject_OnHit, "OnHit" },
		{ &Z_Construct_UFunction_UFunctionsObject_GetCount, "GetCount" },
		{ &Z_Construct_UFunction_UFunctionsObject_Reset, "Reset" },
		{ &Z_Construct_UFunction_UFunctionsObject_GetTitle, "GetTitle" },
		{ &Z_Construct_UFunction_UFunctionsObject_GetByte, "GetByte" },
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UFunctionsObject_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UFunctionsObject>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_UFunctionsObject_Statics::ClassParams = {
		&UFunctionsObject::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		FuncInfo,
		nullptr,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		UE_ARRAY_COUNT(FuncInfo),
		0,
		0,
		0x000000A0u,
	};
	UClass* Z_Construct_UClass_UFunctionsObject()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_UFunctionsObject_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(UFunctionsObject, 0);
	template<> LHTTEST_API UClass* StaticClass<UFunctionsObject>()
	{
		return UFunctionsObject::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UFunctionsObject);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
