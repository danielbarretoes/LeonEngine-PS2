#include "PhysicsEngine/PhysicsSettings.h"

#include "UObject/Class.h"

UPhysicsSettings::UPhysicsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPhysicsSettings* UPhysicsSettings::Get()
{
	return GetMutableDefault<UPhysicsSettings>();
}

FName UPhysicsSettings::GetSurfaceName(EPhysicalSurface Surface) const
{
	if (Surface == SurfaceType_Default)
	{
		return FName(TEXT("Default"));
	}
	for (const FPhysicalSurfaceName& Entry : PhysicalSurfaces)
	{
		if (Entry.Type.GetValue() == Surface)
		{
			return Entry.Name;
		}
	}
	return NAME_None;
}

bool UPhysicsSettings::FindSurfaceType(const FString& Name, EPhysicalSurface& OutSurface) const
{
	if (Name.IsEmpty())
	{
		return false;
	}
	if (Name == TEXT("Default") || Name == TEXT("SurfaceType_Default"))
	{
		OutSurface = SurfaceType_Default;
		return true;
	}
	for (const FPhysicalSurfaceName& Entry : PhysicalSurfaces)
	{
		if (Entry.Name != NAME_None && Name == Entry.Name.ToString())
		{
			OutSurface = Entry.Type.GetValue();
			return true;
		}
	}
	// An enumerator: SurfaceType1 to SurfaceType62.
	const FString Prefix(TEXT("SurfaceType"));
	if (Name.StartsWith(Prefix))
	{
		const FString Digits = Name.RightChop(Prefix.Len());
		const int32 Number = Digits.IsNumeric() ? FCString::Atoi(*Digits) : 0;
		if (Number >= 1 && Number < static_cast<int32>(SurfaceType_Max))
		{
			OutSurface = static_cast<EPhysicalSurface>(Number);
			return true;
		}
	}
	return false;
}
