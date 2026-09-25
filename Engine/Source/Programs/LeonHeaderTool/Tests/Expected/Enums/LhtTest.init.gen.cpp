/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "EnumsObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_ETeam();
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_EWeaponFlags();
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_EUntyped();
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_ERoundState();
	LHTTEST_API UClass* Z_Construct_UClass_UEnumsObject();
// End Cross Module References
	UPackage* Z_Construct_UPackage__Script_LhtTest()
	{
		static UPackage* ReturnPackage = nullptr;
		if (!ReturnPackage)
		{
			static const UE4CodeGen_Private::FPackageParams PackageParams = {
				"/Script/LhtTest",
				nullptr,
				0,
				PKG_CompiledIn | 0x00000000,
				0x00000000,
				0x00000000,
			};
			UE4CodeGen_Private::ConstructUPackage(ReturnPackage, PackageParams);
		}
		return ReturnPackage;
	}
	void RegisterReflection_LhtTest()
	{
		static const FClassRegisterCompiledInInfo ClassInfo[] = {
			{ Z_Construct_UClass_UEnumsObject, UEnumsObject::StaticClass, TEXT("UEnumsObject"), sizeof(UEnumsObject) },
		};
		static const FEnumRegisterCompiledInInfo EnumInfo[] = {
			{ Z_Construct_UEnum_LhtTest_ETeam, TEXT("ETeam") },
			{ Z_Construct_UEnum_LhtTest_EWeaponFlags, TEXT("EWeaponFlags") },
			{ Z_Construct_UEnum_LhtTest_EUntyped, TEXT("EUntyped") },
			{ Z_Construct_UEnum_LhtTest_ERoundState, TEXT("ERoundState") },
		};
		RegisterCompiledInInfo(TEXT("/Script/LhtTest"), ClassInfo, UE_ARRAY_COUNT(ClassInfo), nullptr, 0, EnumInfo, UE_ARRAY_COUNT(EnumInfo));
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
