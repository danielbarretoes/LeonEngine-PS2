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
	LHTTEST_API UClass* Z_Construct_UClass_UContainersObject();
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
			{ Z_Construct_UClass_UContainersObject, UContainersObject::StaticClass, TEXT("UContainersObject"), sizeof(UContainersObject) },
		};
		static const FStructRegisterCompiledInInfo StructInfo[] = {
			{ Z_Construct_UScriptStruct_FContainerEntry, TEXT("ContainerEntry"), sizeof(FContainerEntry) },
		};
		static const FEnumRegisterCompiledInInfo EnumInfo[] = {
			{ Z_Construct_UEnum_LhtTest_EContainerKind, TEXT("EContainerKind") },
		};
		RegisterCompiledInInfo(TEXT("/Script/LhtTest"), ClassInfo, UE_ARRAY_COUNT(ClassInfo), StructInfo, UE_ARRAY_COUNT(StructInfo), EnumInfo, UE_ARRAY_COUNT(EnumInfo));
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
