#include "CookRecipe.h"

#include "CookPaths.h"
#include "StaticMeshBuilder.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace
{

	namespace fs = std::filesystem;

	[[nodiscard]] int CookRecipeStaticMesh(const nlohmann::json& Step, const fs::path& BaseDir, int StepIndex)
	{
		const std::string Obj = Step.value("obj", "");
		const std::string Fbx = Step.value("fbx", "");
		const std::string Gltf = Step.value("gltf", "");
		const std::string Out = Step.value("out", "");
		const std::string MaterialsRel = Step.value("materials", "");
		if (Out.empty())
		{
			std::cerr << "Recipe step " << StepIndex << ": staticmesh needs out\n";
			return 1;
		}
		const int Sources = (!Obj.empty() ? 1 : 0) + (!Fbx.empty() ? 1 : 0) + (!Gltf.empty() ? 1 : 0);
		if (Sources != 1)
		{
			std::cerr << "Recipe step " << StepIndex << ": staticmesh needs exactly one of obj/fbx/gltf\n";
			return 1;
		}
		const std::string OutAbs = FCookPaths::ResolveBeside(BaseDir, Out);
		const std::string MaterialsAbs =
			MaterialsRel.empty() ? std::string{} : FCookPaths::ResolveBeside(BaseDir, MaterialsRel);
		std::string Err;
		bool bOk = false;
		if (!Obj.empty())
		{
			const std::string Src = FCookPaths::ResolveBeside(BaseDir, Obj);
			std::cout << "Cook staticmesh OBJ '" << Src << "' -> " << OutAbs << '\n';
			bOk = FStaticMeshBuilder::CookFromObj(Src, OutAbs, Err);
		}
		else if (!Fbx.empty())
		{
			const std::string Src = FCookPaths::ResolveBeside(BaseDir, Fbx);
			std::cout << "Cook staticmesh FBX '" << Src << "' -> " << OutAbs << '\n';
			bOk = FStaticMeshBuilder::CookFromFbx(Src, OutAbs, Err);
		}
		else
		{
			const std::string Src = FCookPaths::ResolveBeside(BaseDir, Gltf);
			std::cout << "Cook staticmesh glTF '" << Src << "' -> " << OutAbs << '\n';
			bOk = FStaticMeshBuilder::CookFromGltf(Src, OutAbs, MaterialsAbs, Err);
		}
		if (!bOk)
		{
			std::cerr << "Cook staticmesh failed (step " << StepIndex << "): " << (Err.empty() ? "unknown error" : Err)
					  << '\n';
			return 2;
		}
		return 0;
	}

} // namespace

int FCookRecipe::RunFile(const std::string& RecipePath)
{
	std::ifstream In(RecipePath);
	if (!In)
	{
		std::cerr << "Cannot open recipe '" << RecipePath << "'\n";
		return 1;
	}

	nlohmann::json Doc = nlohmann::json::parse(In, nullptr, false);
	if (Doc.is_discarded() || !Doc.contains("steps") || !Doc["steps"].is_array())
	{
		std::cerr << "Recipe must be JSON with a \"steps\" array\n";
		return 1;
	}

	const fs::path BaseDir = fs::path(RecipePath).parent_path();
	int StepIndex = 0;
	for (const auto& Step : Doc["steps"])
	{
		++StepIndex;
		if (!Step.is_object() || !Step.contains("type") || !Step["type"].is_string())
		{
			std::cerr << "Recipe step " << StepIndex << ": missing \"type\"\n";
			return 1;
		}
		const std::string Type = Step["type"].get<std::string>();
		if (Type == "staticmesh")
		{
			const int Rc = CookRecipeStaticMesh(Step, BaseDir, StepIndex);
			if (Rc != 0)
			{
				return Rc;
			}
		}
		else
		{
			std::cerr << "Recipe step " << StepIndex << ": unknown type '" << Type << "'\n";
			return 1;
		}
	}
	return 0;
}
