#include "Commandlets/CookCommandlet.h"

#include "CookRecipe.h"
#include "StaticMeshBuilder.h"

#include <iostream>
#include <string>

namespace
{

	void PrintUsage()
	{
		std::cout << "LeonCook - cook static meshes for Leon (UCookCommandlet)\n\n"
				  << "Usage:\n"
				  << "  LeonCook staticmesh --obj <mesh.obj>|--fbx <m.fbx>|--gltf <m.gltf> --out <m.lmesh>\n"
				  << "    [--materials <dir>]  (glTF: write .lmat + textures)\n\n"
				  << "  LeonCook recipe <file.json>\n"
				  << "    Runs steps from a recipe; relative paths resolve next to the JSON file.\n"
				  << "    Step types: staticmesh\n\n"
				  << "Writes (staticmesh):\n"
				  << "  binary .lmesh (LMSH)\n";
	}

	[[nodiscard]] const char* ArgValue(int Argc, char** Argv, int& I)
	{
		if (I + 1 >= Argc || Argv[I + 1] == nullptr || Argv[I + 1][0] == '\0')
		{
			return nullptr;
		}
		++I;
		return Argv[I];
	}

	[[nodiscard]] int CookStaticMesh(int Argc, char** Argv)
	{
		std::string Obj;
		std::string Fbx;
		std::string Gltf;
		std::string Out;
		std::string Materials;
		for (int I = 2; I < Argc; ++I)
		{
			const std::string A = Argv[I];
			if (A == "--obj")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					Obj = V;
				}
				else
				{
					std::cerr << "--obj requires a path\n";
					return 1;
				}
			}
			else if (A == "--fbx")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					Fbx = V;
				}
				else
				{
					std::cerr << "--fbx requires a path\n";
					return 1;
				}
			}
			else if (A == "--gltf")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					Gltf = V;
				}
				else
				{
					std::cerr << "--gltf requires a path\n";
					return 1;
				}
			}
			else if (A == "--materials")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					Materials = V;
				}
				else
				{
					std::cerr << "--materials requires a path\n";
					return 1;
				}
			}
			else if (A == "--out")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					Out = V;
				}
				else
				{
					std::cerr << "--out requires a path\n";
					return 1;
				}
			}
			else
			{
				std::cerr << "Unknown arg '" << A << "'\n";
				return 1;
			}
		}
		if (Out.empty())
		{
			std::cerr << "staticmesh requires --out\n";
			return 1;
		}
		const int Sources = (!Obj.empty() ? 1 : 0) + (!Fbx.empty() ? 1 : 0) + (!Gltf.empty() ? 1 : 0);
		if (Sources != 1)
		{
			std::cerr << "staticmesh requires exactly one of --obj, --fbx, or --gltf\n";
			return 1;
		}

		std::string Err;
		bool bOk = false;
		if (!Obj.empty())
		{
			bOk = FStaticMeshBuilder::CookFromObj(Obj, Out, Err);
		}
		else if (!Fbx.empty())
		{
			bOk = FStaticMeshBuilder::CookFromFbx(Fbx, Out, Err);
		}
		else
		{
			bOk = FStaticMeshBuilder::CookFromGltf(Gltf, Out, Materials, Err);
		}
		if (!bOk)
		{
			std::cerr << (Err.empty() ? "Cook static mesh failed" : Err) << '\n';
			return 2;
		}
		std::cout << "Cooked static mesh -> " << Out << '\n';
		return 0;
	}

} // namespace

int32 UCookCommandlet::Main(int32 Argc, char** Argv)
{
	if (Argc < 2)
	{
		PrintUsage();
		return 1;
	}

	const std::string Mode = Argv[1];
	if (Mode == "-h" || Mode == "--help" || Mode == "help")
	{
		PrintUsage();
		return 0;
	}

	if (Mode == "staticmesh")
	{
		return CookStaticMesh(Argc, Argv);
	}
	if (Mode == "recipe")
	{
		if (Argc < 3 || Argv[2] == nullptr)
		{
			std::cerr << "recipe requires a JSON path\n";
			PrintUsage();
			return 1;
		}
		return FCookRecipe::RunFile(Argv[2]);
	}

	std::cerr << "Unknown mode '" << Mode << "'\n";
	PrintUsage();
	return 1;
}
