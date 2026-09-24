#include "Serialization/JsonUtils.h"

#include <fstream>
#include <iostream>

glm::vec3 FJsonUtils::ReadVec3(const nlohmann::json& J, const glm::vec3& Fallback)
{
	if (!J.is_array() || J.size() < 3)
	{
		return Fallback;
	}
	return {J[0].get<float>(), J[1].get<float>(), J[2].get<float>()};
}

bool FJsonUtils::LoadJsonFile(const std::string& Path, nlohmann::json& Out)
{
	std::ifstream In(Path);
	if (!In.is_open())
	{
		std::cerr << "Serialization: cannot open " << Path << '\n';
		return false;
	}
	try
	{
		In >> Out;
	}
	catch (const std::exception& Ex)
	{
		std::cerr << "Serialization: JSON error in " << Path << ": " << Ex.what() << '\n';
		return false;
	}
	return true;
}
