#include "CookRecipe.h"

#include "CookPaths.h"
#include "CookerLog.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshBuilder.h"

namespace
{

	/** A string field, or empty when missing or not a string. */
	[[nodiscard]] FString StringField(const FJsonObject& Step, const TCHAR* Name)
	{
		FString Value;
		(void)Step.TryGetStringField(Name, Value);
		return Value;
	}

	[[nodiscard]] int32 CookRecipeStaticMesh(const FJsonObject& Step, const FString& BaseDir, int32 StepIndex)
	{
		const FString Obj = StringField(Step, "obj");
		const FString Fbx = StringField(Step, "fbx");
		const FString Gltf = StringField(Step, "gltf");
		const FString Out = StringField(Step, "out");
		const FString MaterialsRel = StringField(Step, "materials");
		if (Out.IsEmpty())
		{
			UE_LOG(LogCook, Error, "Recipe step %d: staticmesh needs out", StepIndex);
			return 1;
		}
		const int32 Sources = (!Obj.IsEmpty() ? 1 : 0) + (!Fbx.IsEmpty() ? 1 : 0) + (!Gltf.IsEmpty() ? 1 : 0);
		if (Sources != 1)
		{
			UE_LOG(LogCook, Error, "Recipe step %d: staticmesh needs exactly one of obj/fbx/gltf", StepIndex);
			return 1;
		}
		const FString OutAbs = FCookPaths::ResolveBeside(BaseDir, Out);
		const FString MaterialsAbs =
			MaterialsRel.IsEmpty() ? FString() : FCookPaths::ResolveBeside(BaseDir, MaterialsRel);
		FString Err;
		bool bOk = false;
		if (!Obj.IsEmpty())
		{
			const FString Src = FCookPaths::ResolveBeside(BaseDir, Obj);
			UE_LOG(LogCook, Log, "Cook staticmesh OBJ '%s' -> %s", *Src, *OutAbs);
			bOk = FStaticMeshBuilder::CookFromObj(Src, OutAbs, Err);
		}
		else if (!Fbx.IsEmpty())
		{
			const FString Src = FCookPaths::ResolveBeside(BaseDir, Fbx);
			UE_LOG(LogCook, Log, "Cook staticmesh FBX '%s' -> %s", *Src, *OutAbs);
			bOk = FStaticMeshBuilder::CookFromFbx(Src, OutAbs, Err);
		}
		else
		{
			const FString Src = FCookPaths::ResolveBeside(BaseDir, Gltf);
			UE_LOG(LogCook, Log, "Cook staticmesh glTF '%s' -> %s", *Src, *OutAbs);
			bOk = FStaticMeshBuilder::CookFromGltf(Src, OutAbs, MaterialsAbs, Err);
		}
		if (!bOk)
		{
			UE_LOG(LogCook, Error, "Cook staticmesh failed (step %d): %s", StepIndex,
				Err.IsEmpty() ? "unknown error" : *Err);
			return 2;
		}
		return 0;
	}

} // namespace

int32 FCookRecipe::RunFile(const FString& RecipePath)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath))
	{
		UE_LOG(LogCook, Error, "Cannot open recipe '%s'", *RecipePath);
		return 1;
	}

	TSharedPtr<FJsonObject> Doc;
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc) || !Doc.IsValid() ||
		!Doc->TryGetArrayField("steps", Steps))
	{
		UE_LOG(LogCook, Error, "Recipe must be JSON with a \"steps\" array");
		return 1;
	}

	const FString BaseDir = FPaths::GetPath(RecipePath);
	int32 StepIndex = 0;
	for (const TSharedPtr<FJsonValue>& StepValue : *Steps)
	{
		++StepIndex;
		const TSharedPtr<FJsonObject>* Step = nullptr;
		FString Type;
		if (!StepValue.IsValid() || !StepValue->TryGetObject(Step) || !(*Step)->TryGetStringField("type", Type))
		{
			UE_LOG(LogCook, Error, "Recipe step %d: missing \"type\"", StepIndex);
			return 1;
		}
		if (Type.Equals("staticmesh", ESearchCase::CaseSensitive))
		{
			const int32 Rc = CookRecipeStaticMesh(**Step, BaseDir, StepIndex);
			if (Rc != 0)
			{
				return Rc;
			}
		}
		else
		{
			UE_LOG(LogCook, Error, "Recipe step %d: unknown type '%s'", StepIndex, *Type);
			return 1;
		}
	}
	return 0;
}
