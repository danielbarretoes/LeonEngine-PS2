/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ShooterCharacter.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	GAME_API UClass* Z_Construct_UClass_AShooterCharacter_NoRegister();
	GAME_API UClass* Z_Construct_UClass_AShooterCharacter();
	ENGINE_API UScriptStruct* Z_Construct_UScriptStruct_FHitResult();
	ENGINE_API UClass* Z_Construct_UClass_AActor_NoRegister();
	COREUOBJECT_API UClass* Z_Construct_UClass_UClass();
	ENGINE_API UEnum* Z_Construct_UEnum_Engine_EMovementMode();
	ENGINE_API UClass* Z_Construct_UClass_UStaticMesh_NoRegister();
	ENGINE_API UEnum* Z_Construct_UEnum_Engine_ECollisionChannel();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject_NoRegister();
	ENGINE_API UClass* Z_Construct_UClass_AActor();
	UPackage* Z_Construct_UPackage__Script_Game();
// End Cross Module References
	DEFINE_FUNCTION(AShooterCharacter::execTeleport)
	{
		P_GET_STRUCT_REF(FHitResult,Z_Param_Out_Where);
		P_GET_OBJECT(AActor,Z_Param_Other);
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->Teleport(Z_Param_Out_Where,Z_Param_Other);
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(AShooterCharacter::execFindTarget)
	{
		P_GET_OBJECT(UClass,Z_Param_Filter);
		P_GET_ENUM(EMovementMode,Z_Param_Mode);
		P_FINISH;
		P_NATIVE_BEGIN;
		*(AActor**)Z_Param__Result=P_THIS->FindTarget(Z_Param_Filter,EMovementMode(Z_Param_Mode));
		P_NATIVE_END;
	}
	void AShooterCharacter::StaticRegisterNativesAShooterCharacter()
	{
		UClass* Class = AShooterCharacter::StaticClass();
		static const FNameNativePtrPair Funcs[] = {
			{ "Teleport", &AShooterCharacter::execTeleport },
			{ "FindTarget", &AShooterCharacter::execFindTarget },
		};
		FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
	}
	struct Z_Construct_UFunction_AShooterCharacter_Teleport_Statics
	{
		struct ShooterCharacter_eventTeleport_Parms
		{
			FHitResult Where;
			AActor* Other;
		};
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Where;
		static const UE4CodeGen_Private::FObjectPropertyParams NewProp_Other;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::NewProp_Where = { "Where", nullptr, (EPropertyFlags)0x0010000008000182, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(ShooterCharacter_eventTeleport_Parms, Where), Z_Construct_UScriptStruct_FHitResult };
	const UE4CodeGen_Private::FObjectPropertyParams Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::NewProp_Other = { "Other", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(ShooterCharacter_eventTeleport_Parms, Other), Z_Construct_UClass_AActor_NoRegister };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::NewProp_Where,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::NewProp_Other,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_AShooterCharacter, nullptr, "Teleport", nullptr, nullptr, sizeof(Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::ShooterCharacter_eventTeleport_Parms), Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00420601, 0, 0 };
	UFunction* Z_Construct_UFunction_AShooterCharacter_Teleport()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_AShooterCharacter_Teleport_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics
	{
		struct ShooterCharacter_eventFindTarget_Parms
		{
			TSubclassOf<AActor> Filter;
			EMovementMode Mode;
			AActor* ReturnValue;
		};
		static const UE4CodeGen_Private::FClassPropertyParams NewProp_Filter;
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_Mode_Underlying;
		static const UE4CodeGen_Private::FEnumPropertyParams NewProp_Mode;
		static const UE4CodeGen_Private::FObjectPropertyParams NewProp_ReturnValue;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FFunctionParams FuncParams;
	};
	const UE4CodeGen_Private::FClassPropertyParams Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::NewProp_Filter = { "Filter", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Class, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(ShooterCharacter_eventFindTarget_Parms, Filter), Z_Construct_UClass_AActor_NoRegister, Z_Construct_UClass_UClass };
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::NewProp_Mode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, nullptr };
	const UE4CodeGen_Private::FEnumPropertyParams Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::NewProp_Mode = { "Mode", nullptr, (EPropertyFlags)0x0010000000000080, UE4CodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(ShooterCharacter_eventFindTarget_Parms, Mode), Z_Construct_UEnum_Engine_EMovementMode };
	const UE4CodeGen_Private::FObjectPropertyParams Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UE4CodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(ShooterCharacter_eventFindTarget_Parms, ReturnValue), Z_Construct_UClass_AActor_NoRegister };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::NewProp_Filter,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::NewProp_Mode_Underlying,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::NewProp_Mode,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::NewProp_ReturnValue,
	};
	const UE4CodeGen_Private::FFunctionParams Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_AShooterCharacter, nullptr, "FindTarget", nullptr, nullptr, sizeof(Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::ShooterCharacter_eventFindTarget_Parms), Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x40020401, 0, 0 };
	UFunction* Z_Construct_UFunction_AShooterCharacter_FindTarget()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UE4CodeGen_Private::ConstructUFunction(ReturnFunction, Z_Construct_UFunction_AShooterCharacter_FindTarget_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	UClass* Z_Construct_UClass_AShooterCharacter_NoRegister()
	{
		return AShooterCharacter::StaticClass();
	}
	struct Z_Construct_UClass_AShooterCharacter_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FClassFunctionLinkInfo FuncInfo[];
		static const UE4CodeGen_Private::FObjectPropertyParams NewProp_Mesh;
		static const UE4CodeGen_Private::FObjectPropertyParams NewProp_ConstMesh;
		static const UE4CodeGen_Private::FClassPropertyParams NewProp_ProjectileClass;
		static const UE4CodeGen_Private::FSoftObjectPropertyParams NewProp_LazyMesh;
		static const UE4CodeGen_Private::FSoftClassPropertyParams NewProp_LazyClass;
		static const UE4CodeGen_Private::FWeakObjectPropertyParams NewProp_Target;
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_LastHit;
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_Channel;
		static const UE4CodeGen_Private::FBytePropertyParams NewProp_Movement_Underlying;
		static const UE4CodeGen_Private::FEnumPropertyParams NewProp_Movement;
		static const UE4CodeGen_Private::FObjectPropertyParams NewProp_Anything;
		static const UE4CodeGen_Private::FObjectPropertyParams NewProp_Followers_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Followers;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UE4CodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_AShooterCharacter_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_AActor,
		(UObject* (*)())Z_Construct_UPackage__Script_Game,
	};
	const FClassFunctionLinkInfo Z_Construct_UClass_AShooterCharacter_Statics::FuncInfo[] = {
		{ &Z_Construct_UFunction_AShooterCharacter_Teleport, "Teleport" },
		{ &Z_Construct_UFunction_AShooterCharacter_FindTarget, "FindTarget" },
	};
	const UE4CodeGen_Private::FObjectPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Mesh = { "Mesh", nullptr, (EPropertyFlags)0x0012000000090009, UE4CodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, Mesh), Z_Construct_UClass_UStaticMesh_NoRegister };
	const UE4CodeGen_Private::FObjectPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_ConstMesh = { "ConstMesh", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, ConstMesh), Z_Construct_UClass_UStaticMesh_NoRegister };
	const UE4CodeGen_Private::FClassPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_ProjectileClass = { "ProjectileClass", nullptr, (EPropertyFlags)0x0010000000010001, UE4CodeGen_Private::EPropertyGenFlags::Class, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, ProjectileClass), Z_Construct_UClass_AActor_NoRegister, Z_Construct_UClass_UClass };
	const UE4CodeGen_Private::FSoftObjectPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_LazyMesh = { "LazyMesh", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::SoftObject, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, LazyMesh), Z_Construct_UClass_UStaticMesh_NoRegister };
	const UE4CodeGen_Private::FSoftClassPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_LazyClass = { "LazyClass", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::SoftClass, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, LazyClass), Z_Construct_UClass_AActor_NoRegister };
	const UE4CodeGen_Private::FWeakObjectPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Target = { "Target", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::WeakObject, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, Target), Z_Construct_UClass_AActor_NoRegister };
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_LastHit = { "LastHit", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, LastHit), Z_Construct_UScriptStruct_FHitResult };
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Channel = { "Channel", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, Channel), Z_Construct_UEnum_Engine_ECollisionChannel };
	const UE4CodeGen_Private::FBytePropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Movement_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, nullptr };
	const UE4CodeGen_Private::FEnumPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Movement = { "Movement", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, Movement), Z_Construct_UEnum_Engine_EMovementMode };
	const UE4CodeGen_Private::FObjectPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Anything = { "Anything", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, Anything), Z_Construct_UClass_UObject_NoRegister };
	const UE4CodeGen_Private::FObjectPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Followers_Inner = { "Followers", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, Z_Construct_UClass_AActor_NoRegister };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Followers = { "Followers", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(AShooterCharacter, Followers), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_AShooterCharacter_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Mesh,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_ConstMesh,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_ProjectileClass,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_LazyMesh,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_LazyClass,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Target,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_LastHit,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Channel,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Movement_Underlying,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Movement,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Anything,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Followers_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_AShooterCharacter_Statics::NewProp_Followers,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_AShooterCharacter_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<AShooterCharacter>::IsAbstract,
	};
	const UE4CodeGen_Private::FClassParams Z_Construct_UClass_AShooterCharacter_Statics::ClassParams = {
		&AShooterCharacter::StaticClass,
		"Game",
		&StaticCppClassTypeInfo,
		DependentSingletons,
		FuncInfo,
		Z_Construct_UClass_AShooterCharacter_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		UE_ARRAY_COUNT(FuncInfo),
		UE_ARRAY_COUNT(Z_Construct_UClass_AShooterCharacter_Statics::PropPointers),
		0,
		0x001000A4u,
	};
	UClass* Z_Construct_UClass_AShooterCharacter()
	{
		static UClass* OuterClass = nullptr;
		if (!OuterClass)
		{
			UE4CodeGen_Private::ConstructUClass(OuterClass, Z_Construct_UClass_AShooterCharacter_Statics::ClassParams);
		}
		return OuterClass;
	}
	IMPLEMENT_CLASS(AShooterCharacter, 0);
	template<> GAME_API UClass* StaticClass<AShooterCharacter>()
	{
		return AShooterCharacter::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(AShooterCharacter);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
