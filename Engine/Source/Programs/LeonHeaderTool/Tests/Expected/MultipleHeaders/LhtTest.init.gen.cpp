/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Inventory.h"
#include "Weapon.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UClass* Z_Construct_UClass_UInventory();
	LHTTEST_API UEnum* Z_Construct_UEnum_LhtTest_EWeaponSlot();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FAmmo();
	LHTTEST_API UClass* Z_Construct_UClass_UWeapon();
	LHTTEST_API UClass* Z_Construct_UClass_URifle();
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
			{ Z_Construct_UClass_UInventory, UInventory::StaticClass, TEXT("UInventory"), sizeof(UInventory) },
			{ Z_Construct_UClass_UWeapon, UWeapon::StaticClass, TEXT("UWeapon"), sizeof(UWeapon) },
			{ Z_Construct_UClass_URifle, URifle::StaticClass, TEXT("URifle"), sizeof(URifle) },
		};
		static const FStructRegisterCompiledInInfo StructInfo[] = {
			{ Z_Construct_UScriptStruct_FAmmo, TEXT("Ammo"), sizeof(FAmmo) },
		};
		static const FEnumRegisterCompiledInInfo EnumInfo[] = {
			{ Z_Construct_UEnum_LhtTest_EWeaponSlot, TEXT("EWeaponSlot") },
		};
		RegisterCompiledInInfo(TEXT("/Script/LhtTest"), ClassInfo, UE_ARRAY_COUNT(ClassInfo), StructInfo, UE_ARRAY_COUNT(StructInfo), EnumInfo, UE_ARRAY_COUNT(EnumInfo));
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
