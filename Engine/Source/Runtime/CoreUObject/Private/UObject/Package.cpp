#include "UObject/Package.h"

#include "UObject/Class.h"

UPackage::UPackage(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
	, PackageFlagsPrivate(PKG_None)
	, Guid()
{
}

// UPackage is intrinsic (UE: IMPLEMENT_CORE_INTRINSIC_CLASS(UPackage, UObject, ...) in Package.cpp).
IMPLEMENT_CLASS(UPackage, 0)

COREUOBJECT_API UClass* Z_Construct_UClass_UObject();

COREUOBJECT_API UClass* Z_Construct_UClass_UPackage_NoRegister()
{
	return UPackage::StaticClass();
}

COREUOBJECT_API UClass* Z_Construct_UClass_UPackage()
{
	static UClass* Class = nullptr;
	if (!Class)
	{
		Z_Construct_UClass_UObject();
		Class = UPackage::StaticClass();
		UObjectForceRegistration(Class);
		Class->ClassFlags |= CLASS_Constructed;
		Class->StaticLink();
	}
	return Class;
}
