#include "Commandlets/CookCommandlet.h"

#include "CookRecipe.h"
#include "CookerLog.h"
#include "Misc/CString.h"
#include "StaticMeshBuilder.h"

namespace
{

	void PrintUsage()
	{
		UE_LOG(LogCook, Display, "LeonCook - cook static meshes for Leon (UCookCommandlet)");
		UE_LOG(LogCook, Display, "Usage:");
		UE_LOG(
			LogCook, Display, "  LeonCook staticmesh --obj <mesh.obj>|--fbx <m.fbx>|--gltf <m.gltf> --out <m.lmesh>");
		UE_LOG(LogCook, Display, "    [--materials <dir>]  (glTF: write .lmat + textures)");
		UE_LOG(LogCook, Display, "  LeonCook recipe <file.json>");
		UE_LOG(LogCook, Display, "    Runs steps from a recipe; relative paths resolve next to the JSON file.");
		UE_LOG(LogCook, Display, "    Step types: staticmesh");
		UE_LOG(LogCook, Display, "Writes (staticmesh): binary .lmesh (LMSH)");
	}

	/** The value after the switch at I (advancing I), or null when it is missing. */
	[[nodiscard]] const ANSICHAR* ArgValue(int32 ArgC, char** ArgV, int32& I)
	{
		if (I + 1 >= ArgC || ArgV[I + 1] == nullptr || ArgV[I + 1][0] == '\0')
		{
			return nullptr;
		}
		++I;
		return ArgV[I];
	}

	[[nodiscard]] int32 CookStaticMesh(int32 ArgC, char** ArgV)
	{
		struct FSwitch
		{
			const ANSICHAR* Name;
			FString* Value;
		};
		FString Obj;
		FString Fbx;
		FString Gltf;
		FString Out;
		FString Materials;
		const FSwitch Switches[] = {
			{"--obj", &Obj}, {"--fbx", &Fbx}, {"--gltf", &Gltf}, {"--materials", &Materials}, {"--out", &Out}};

		for (int32 I = 2; I < ArgC; ++I)
		{
			const ANSICHAR* Arg = ArgV[I];
			const FSwitch* Match = nullptr;
			for (const FSwitch& Switch : Switches)
			{
				if (FCStringAnsi::Strcmp(Arg, Switch.Name) == 0)
				{
					Match = &Switch;
					break;
				}
			}
			if (Match == nullptr)
			{
				UE_LOG(LogCook, Error, "Unknown arg '%s'", Arg);
				return 1;
			}
			const ANSICHAR* Value = ArgValue(ArgC, ArgV, I);
			if (Value == nullptr)
			{
				UE_LOG(LogCook, Error, "%s requires a path", Match->Name);
				return 1;
			}
			*Match->Value = Value;
		}
		if (Out.IsEmpty())
		{
			UE_LOG(LogCook, Error, "staticmesh requires --out");
			return 1;
		}
		const int32 Sources = (!Obj.IsEmpty() ? 1 : 0) + (!Fbx.IsEmpty() ? 1 : 0) + (!Gltf.IsEmpty() ? 1 : 0);
		if (Sources != 1)
		{
			UE_LOG(LogCook, Error, "staticmesh requires exactly one of --obj, --fbx, or --gltf");
			return 1;
		}

		FString Err;
		bool bOk = false;
		if (!Obj.IsEmpty())
		{
			bOk = FStaticMeshBuilder::CookFromObj(Obj, Out, Err);
		}
		else if (!Fbx.IsEmpty())
		{
			bOk = FStaticMeshBuilder::CookFromFbx(Fbx, Out, Err);
		}
		else
		{
			bOk = FStaticMeshBuilder::CookFromGltf(Gltf, Out, Materials, Err);
		}
		if (!bOk)
		{
			UE_LOG(LogCook, Error, "%s", Err.IsEmpty() ? "Cook static mesh failed" : *Err);
			return 2;
		}
		UE_LOG(LogCook, Log, "Cooked static mesh -> %s", *Out);
		return 0;
	}

} // namespace

int32 UCookCommandlet::Main(int32 ArgC, char** ArgV)
{
	if (ArgC < 2)
	{
		PrintUsage();
		return 1;
	}

	const ANSICHAR* Mode = ArgV[1];
	if (FCStringAnsi::Strcmp(Mode, "-h") == 0 || FCStringAnsi::Strcmp(Mode, "--help") == 0 ||
		FCStringAnsi::Strcmp(Mode, "help") == 0)
	{
		PrintUsage();
		return 0;
	}

	if (FCStringAnsi::Strcmp(Mode, "staticmesh") == 0)
	{
		return CookStaticMesh(ArgC, ArgV);
	}
	if (FCStringAnsi::Strcmp(Mode, "recipe") == 0)
	{
		if (ArgC < 3 || ArgV[2] == nullptr)
		{
			UE_LOG(LogCook, Error, "recipe requires a JSON path");
			PrintUsage();
			return 1;
		}
		return FCookRecipe::RunFile(ArgV[2]);
	}

	UE_LOG(LogCook, Error, "Unknown mode '%s'", Mode);
	PrintUsage();
	return 1;
}
