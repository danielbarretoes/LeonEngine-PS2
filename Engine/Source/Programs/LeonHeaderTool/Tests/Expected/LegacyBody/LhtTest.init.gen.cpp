/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "LegacyObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UClass* Z_Construct_UClass_ULegacyObject();
	LHTTEST_API UClass* Z_Construct_UClass_UCustomVTableObject();
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
			{ Z_Construct_UClass_ULegacyObject, ULegacyObject::StaticClass, TEXT("ULegacyObject"), sizeof(ULegacyObject) },
			{ Z_Construct_UClass_UCustomVTableObject, UCustomVTableObject::StaticClass, TEXT("UCustomVTableObject"), sizeof(UCustomVTableObject) },
		};
		RegisterCompiledInInfo(TEXT("/Script/LhtTest"), ClassInfo, UE_ARRAY_COUNT(ClassInfo), nullptr, 0, nullptr, 0);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
