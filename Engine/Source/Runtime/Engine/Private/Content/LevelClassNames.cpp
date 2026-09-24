#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Misc/CString.h"

bool TryParseBasicShapeName(std::string_view Name, EBasicShape& Out)
{
	const std::string Key = FCString::ToLower(Name);
	if (Key == "cube")
	{
		Out = EBasicShape::Cube;
		return true;
	}
	if (Key == "sphere")
	{
		Out = EBasicShape::Sphere;
		return true;
	}
	if (Key == "plane")
	{
		Out = EBasicShape::Plane;
		return true;
	}
	return false;
}

bool IsBlockingVolumeName(std::string_view Name)
{
	return FCString::ToLower(Name) == "blockingvolume";
}

bool IsPlayerStartName(std::string_view Name)
{
	return FCString::ToLower(Name) == "playerstart";
}

bool TryParseBasicLightName(std::string_view Name, EBasicLight& Out)
{
	const std::string Key = FCString::ToLower(Name);
	if (Key == "directionallight" || Key == "directional" || Key == "dirlight")
	{
		Out = EBasicLight::Directional;
		return true;
	}
	if (Key == "pointlight" || Key == "point")
	{
		Out = EBasicLight::Point;
		return true;
	}
	return false;
}
