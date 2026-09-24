#include "Commandlets/CookCommandlet.h"

#include "Animation/CookedSkeletal.h"
#include "CookRecipe.h"
#include "StaticMeshBuilder.h"

#include <iostream>
#include <string>

namespace
{

	void PrintUsage()
	{
		std::cout << "leon-cook — cook meshes / skeletal assets for Leon\n\n"
				  << "Usage:\n"
				  << "  leon-cook staticmesh --obj <mesh.obj>|--fbx <m.fbx>|--gltf <m.gltf> --out <m.lmesh>\n"
				  << "    [--materials <dir>]  (glTF: write .lmat + textures)\n\n"
				  << "  leon-cook character --name <Name> --mesh <idle.fbx> --run <run.fbx> --out <dir>\n"
				  << "    [--jump <JumpingUp.fbx>] [--fall <FallingIdle.fbx>] [--land <Land.fbx>]\n\n"
				  << "  leon-cook anim --fbx <clip.fbx> --skeleton <Bot.lskel>\n"
				  << "    --name <ClipName> --out <Anims/Clip.lanim> [--noloop]\n\n"
				  << "  leon-cook recipe <file.json>\n"
				  << "    Runs steps from a recipe; relative paths resolve next to the JSON file.\n"
				  << "    Step types: character | anim | staticmesh\n\n"
				  << "Writes (staticmesh):\n"
				  << "  binary .lmesh (LMSH)\n"
				  << "Writes (character):\n"
				  << "  <Name>.lskel / .lskm / Materials / Anims / blendspace / .lchar\n"
				  << "Writes (anim):\n"
				  << "  <out>.lanim\n";
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

	[[nodiscard]] int CookCharacter(int Argc, char** Argv)
	{
		std::string Name;
		std::string MeshFbx;
		std::string RunFbx;
		std::string OutDir;
		FCookJumpAnimPaths JumpAnims{};

		for (int I = 2; I < Argc; ++I)
		{
			const std::string A = Argv[I];
			if (A == "--name")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					Name = V;
				}
			}
			else if (A == "--mesh")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					MeshFbx = V;
				}
			}
			else if (A == "--run")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					RunFbx = V;
				}
			}
			else if (A == "--jump")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					JumpAnims.JumpStartFbx = V;
				}
			}
			else if (A == "--fall")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					JumpAnims.FallLoopFbx = V;
				}
			}
			else if (A == "--land")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					JumpAnims.LandFbx = V;
				}
			}
			else if (A == "--out")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					OutDir = V;
				}
			}
			else
			{
				std::cerr << "Unknown arg '" << A << "'\n";
				return 1;
			}
		}

		if (Name.empty() || MeshFbx.empty() || RunFbx.empty() || OutDir.empty())
		{
			std::cerr << "character requires --name --mesh --run --out\n";
			PrintUsage();
			return 1;
		}

		if (!CookCharacterFromFbx(Name, MeshFbx, RunFbx, OutDir, JumpAnims))
		{
			std::cerr << "Cook failed\n";
			return 2;
		}
		return 0;
	}

	[[nodiscard]] int CookAnim(int Argc, char** Argv)
	{
		std::string Fbx;
		std::string Skeleton;
		std::string Name;
		std::string OutJson;
		bool bLooping = true;

		for (int I = 2; I < Argc; ++I)
		{
			const std::string A = Argv[I];
			if (A == "--fbx")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					Fbx = V;
				}
			}
			else if (A == "--skeleton")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					Skeleton = V;
				}
			}
			else if (A == "--name")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					Name = V;
				}
			}
			else if (A == "--out")
			{
				if (const char* V = ArgValue(Argc, Argv, I))
				{
					OutJson = V;
				}
			}
			else if (A == "--noloop")
			{
				bLooping = false;
			}
			else
			{
				std::cerr << "Unknown arg '" << A << "'\n";
				return 1;
			}
		}

		if (Fbx.empty() || Skeleton.empty() || OutJson.empty())
		{
			std::cerr << "anim requires --fbx --skeleton --out\n";
			PrintUsage();
			return 1;
		}

		if (!CookAnimSequenceFromFbx(Fbx, Skeleton, OutJson, Name, bLooping))
		{
			std::cerr << "Cook anim failed\n";
			return 2;
		}
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
	if (Mode == "character")
	{
		return CookCharacter(Argc, Argv);
	}
	if (Mode == "anim")
	{
		return CookAnim(Argc, Argv);
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
