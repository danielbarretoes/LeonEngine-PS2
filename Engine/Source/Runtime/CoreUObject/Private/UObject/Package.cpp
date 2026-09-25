#include "UObject/Package.h"

#include "Misc/PackageName.h"
#include "UObject/Class.h"
#include "UObject/LinkerLoad.h"

UPackage::UPackage(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
	, LinkerLoad(nullptr)
	, FileName(NAME_None)
	, PackageFlagsPrivate(PKG_None)
	, Guid()
	, bHasBeenFullyLoaded(false)
{
}

void UPackage::BeginDestroy()
{
	// A loaded package releases its linker once loaded; one that is destroyed while loading takes it with it.
	if (LinkerLoad)
	{
		FLinkerLoad* Linker = LinkerLoad;
		Linker->Detach();
		delete Linker;
	}
	Super::BeginDestroy();
}

bool UPackage::IsFullyLoaded() const
{
	if (!bHasBeenFullyLoaded && !LinkerLoad && !HasAnyPackageFlags(PKG_CompiledIn))
	{
		// A package with nothing on disk (or in memory) to load was created in memory: it counts as loaded (UE).
		if (!FPackageName::DoesPackageExist(GetName()))
		{
			bHasBeenFullyLoaded = true;
		}
	}
	return bHasBeenFullyLoaded;
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
