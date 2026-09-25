#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "UObject/Class.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogScriptCore, Log, All);

FFrame::FFrame(
	UObject* InObject, UFunction* InNode, void* InLocals, FFrame* InPreviousFrame, FField* InPropertyChainForCompiledIn)
	: Node(InNode)
	, Object(InObject)
	, Code(nullptr)
	, Locals((uint8*)InLocals)
	, MostRecentProperty(nullptr)
	, MostRecentPropertyAddress(nullptr)
	, PropertyChainForCompiledIn(InPropertyChainForCompiledIn)
	, PreviousFrame(InPreviousFrame)
{
}

void FFrame::StepExplicitProperty(void* const Result, FProperty* Property)
{
	checkf(Result, "No destination for parameter %s", *Property->GetName());
	MostRecentProperty = Property;
	MostRecentPropertyAddress = Property->ContainerPtrToValuePtr<uint8>(Locals);
	if (!Property->HasAnyPropertyFlags(CPF_OutParm))
	{
		Property->CopyCompleteValueToScriptVM(Result, MostRecentPropertyAddress);
	}
}

FString FFrame::GetStackDescription() const
{
	return FString::Printf(TEXT("%s on %s"), Node ? *Node->GetName() : TEXT("(no function)"),
		Object ? *Object->GetFullName() : TEXT("(no object)"));
}

// Console commands (UE: ScriptCore.cpp)

bool UObject::CallFunctionByNameWithArguments(
	const TCHAR* Str, FOutputDevice& Ar, UObject* Executor, bool bForceCallWithNonExec)
{
	// The function: the first word.
	FString MsgStr;
	if (!FParse::Token(Str, MsgStr, true))
	{
		UE_LOG(LogScriptCore, Verbose, TEXT("CallFunctionByNameWithArguments: Not Parsed '%s'"), Str);
		return false;
	}
	const FName Message(*MsgStr, FNAME_Find);
	if (Message.IsNone())
	{
		UE_LOG(LogScriptCore, Verbose, TEXT("CallFunctionByNameWithArguments: Name not found '%s'"), *MsgStr);
		return false;
	}
	UFunction* Function = FindFunction(Message);
	if (!Function)
	{
		UE_LOG(LogScriptCore, Verbose, TEXT("CallFunctionByNameWithArguments: Function not found '%s'"), *MsgStr);
		return false;
	}
	if (!Function->HasAnyFunctionFlags(FUNC_Exec) && !bForceCallWithNonExec)
	{
		UE_LOG(LogScriptCore, Verbose, TEXT("CallFunctionByNameWithArguments: Function not executable '%s'"), *MsgStr);
		return false;
	}

	// The parameters, return value excluded; the last one may take the rest of the line.
	FProperty* LastParameter = nullptr;
	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm;
		++It)
	{
		LastParameter = *It;
	}

	// A parameter block laid out like the generated _Parms struct, every parameter initialized (UE).
	uint8* Parms = (uint8*)FMemory::Malloc(FMath::Max<SIZE_T>(Function->ParmsSize, 1),
		uint32(FMath::Max(Function->GetMinAlignment(), int32(alignof(void*)))));
	FMemory::Memzero(Parms, Function->ParmsSize);
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (!It->HasAnyPropertyFlags(CPF_ZeroConstructor))
		{
			It->InitializeValue_InContainer(Parms);
		}
	}

	bool bFailed = false;
	int32 NumParamsEvaluated = 0;
	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm;
		++It, ++NumParamsEvaluated)
	{
		FProperty* PropertyParam = *It;
		if (NumParamsEvaluated == 0 && Executor)
		{
			// A first object parameter that fits receives the object running the command (UE).
			FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PropertyParam);
			if (ObjectProperty && Executor->IsA(ObjectProperty->PropertyClass))
			{
				ObjectProperty->SetObjectPropertyValue(ObjectProperty->ContainerPtrToValuePtr<uint8>(Parms), Executor);
				continue;
			}
		}

		const TCHAR* RemainingStr = Str;
		FString ArgStr;
		if (!FParse::Token(Str, ArgStr, true))
		{
			// UE fills a missing argument from the function's CPP_Default_ metadata, which Leon does not generate:
			// the parameter keeps its zero / default value.
			Ar.Logf(ELogVerbosity::Warning, TEXT("%s: missing argument '%s', using its default value"),
				*Function->GetName(), *PropertyParam->GetName());
			continue;
		}
		// The last FString parameter takes the rest of the line, unquoted: a sub-command for another exec (UE).
		if (PropertyParam == LastParameter && PropertyParam->IsA<FStrProperty>() && *Str)
		{
			ArgStr = FString(RemainingStr).TrimStart();
		}
		if (!PropertyParam->ImportText(
				*ArgStr, PropertyParam->ContainerPtrToValuePtr<uint8>(Parms), PPF_None, nullptr, &Ar))
		{
			Ar.Logf(TEXT("'%s': Bad or missing property '%s' when trying to call %s"), *ArgStr,
				*PropertyParam->GetName(), *Function->GetName());
			bFailed = true;
			break;
		}
	}

	if (!bFailed)
	{
		ProcessEvent(Function, Parms);
	}

	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Parms);
	}
	FMemory::Free(Parms);
	// Handled: the function exists and is executable, whether or not the arguments were good (UE).
	return true;
}
