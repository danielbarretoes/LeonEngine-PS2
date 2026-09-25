/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "NoExportTypes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FVector();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FQuat();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FPlane();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FTransform();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FUsesNoExport();
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
		static const FStructRegisterCompiledInInfo StructInfo[] = {
			{ Z_Construct_UScriptStruct_FVector, TEXT("Vector"), sizeof(FVector) },
			{ Z_Construct_UScriptStruct_FQuat, TEXT("Quat"), sizeof(FQuat) },
			{ Z_Construct_UScriptStruct_FPlane, TEXT("Plane"), sizeof(FPlane) },
			{ Z_Construct_UScriptStruct_FTransform, TEXT("Transform"), sizeof(FTransform) },
			{ Z_Construct_UScriptStruct_FUsesNoExport, TEXT("UsesNoExport"), sizeof(FUsesNoExport) },
		};
		RegisterCompiledInInfo(TEXT("/Script/LhtTest"), nullptr, 0, StructInfo, UE_ARRAY_COUNT(StructInfo), nullptr, 0);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
