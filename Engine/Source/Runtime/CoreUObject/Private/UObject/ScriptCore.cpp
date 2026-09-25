#include "UObject/Class.h"
#include "UObject/Stack.h"
#include "UObject/UnrealType.h"

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
