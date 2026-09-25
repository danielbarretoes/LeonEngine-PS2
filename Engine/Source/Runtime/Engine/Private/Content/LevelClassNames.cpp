#include "Level/BasicLight.h"
#include "Level/BasicShape.h"

// Class names compare ignoring case (FString's == does).

bool TryParseBasicShapeName(const FString& Name, EBasicShape& Out)
{
	if (Name == "cube")
	{
		Out = EBasicShape::Cube;
		return true;
	}
	if (Name == "sphere")
	{
		Out = EBasicShape::Sphere;
		return true;
	}
	if (Name == "plane")
	{
		Out = EBasicShape::Plane;
		return true;
	}
	return false;
}

bool IsBlockingVolumeName(const FString& Name)
{
	return Name == "blockingvolume";
}

bool IsPlayerStartName(const FString& Name)
{
	return Name == "playerstart";
}

bool TryParseBasicLightName(const FString& Name, EBasicLight& Out)
{
	if (Name == "directionallight" || Name == "directional" || Name == "dirlight")
	{
		Out = EBasicLight::Directional;
		return true;
	}
	if (Name == "pointlight" || Name == "point")
	{
		Out = EBasicLight::Point;
		return true;
	}
	return false;
}
