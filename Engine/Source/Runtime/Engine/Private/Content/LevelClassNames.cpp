#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Misc/CString.h"

#include <string_view>

namespace
{
	[[nodiscard]] bool EqualsIgnoreCase(std::string_view Name, std::string_view Literal)
	{
		return Name.size() == Literal.size() && FCString::Strnicmp(Name.data(), Literal.data(), Name.size()) == 0;
	}
} // namespace

bool TryParseBasicShapeName(std::string_view Name, EBasicShape& Out)
{
	if (EqualsIgnoreCase(Name, "cube"))
	{
		Out = EBasicShape::Cube;
		return true;
	}
	if (EqualsIgnoreCase(Name, "sphere"))
	{
		Out = EBasicShape::Sphere;
		return true;
	}
	if (EqualsIgnoreCase(Name, "plane"))
	{
		Out = EBasicShape::Plane;
		return true;
	}
	return false;
}

bool IsBlockingVolumeName(std::string_view Name)
{
	return EqualsIgnoreCase(Name, "blockingvolume");
}

bool IsPlayerStartName(std::string_view Name)
{
	return EqualsIgnoreCase(Name, "playerstart");
}

bool TryParseBasicLightName(std::string_view Name, EBasicLight& Out)
{
	if (EqualsIgnoreCase(Name, "directionallight") || EqualsIgnoreCase(Name, "directional") ||
		EqualsIgnoreCase(Name, "dirlight"))
	{
		Out = EBasicLight::Directional;
		return true;
	}
	if (EqualsIgnoreCase(Name, "pointlight") || EqualsIgnoreCase(Name, "point"))
	{
		Out = EBasicLight::Point;
		return true;
	}
	return false;
}
