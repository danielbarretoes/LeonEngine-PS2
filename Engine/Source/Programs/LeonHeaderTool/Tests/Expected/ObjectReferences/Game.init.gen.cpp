/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ShooterCharacter.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	GAME_API UClass* Z_Construct_UClass_AShooterCharacter();
// End Cross Module References
	UPackage* Z_Construct_UPackage__Script_Game()
	{
		static UPackage* ReturnPackage = nullptr;
		if (!ReturnPackage)
		{
			static const UE4CodeGen_Private::FPackageParams PackageParams = {
				"/Script/Game",
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
	void RegisterReflection_Game()
	{
		static const FClassRegisterCompiledInInfo ClassInfo[] = {
			{ Z_Construct_UClass_AShooterCharacter, AShooterCharacter::StaticClass, TEXT("AShooterCharacter"), sizeof(AShooterCharacter) },
		};
		RegisterCompiledInInfo(TEXT("/Script/Game"), ClassInfo, UE_ARRAY_COUNT(ClassInfo), nullptr, 0, nullptr, 0);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
