/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "EditorOnlyObject.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UClass* Z_Construct_UClass_UEditorOnlyObject();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FEditorOnlyStruct();
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
			{ Z_Construct_UClass_UEditorOnlyObject, UEditorOnlyObject::StaticClass, TEXT("UEditorOnlyObject"), sizeof(UEditorOnlyObject) },
		};
		static const FStructRegisterCompiledInInfo StructInfo[] = {
			{ Z_Construct_UScriptStruct_FEditorOnlyStruct, TEXT("EditorOnlyStruct"), sizeof(FEditorOnlyStruct) },
		};
		RegisterCompiledInInfo(TEXT("/Script/LhtTest"), ClassInfo, UE_ARRAY_COUNT(ClassInfo), StructInfo, UE_ARRAY_COUNT(StructInfo), nullptr, 0);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
