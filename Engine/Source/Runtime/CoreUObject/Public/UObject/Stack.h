#pragma once

// The call frame an exec thunk reads its parameters from (UE: UObject/Stack.h).

#include "CoreMinimal.h"
#include "UObject/Field.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

class UFunction;
class UObject;

/**
 * A native call in progress (UE: FFrame). Leon has no script VM, so Code is always null: the P_GET_* macros step
 * through the function's parameter properties (PropertyChainForCompiledIn) and read each value from Locals, the
 * parameter block ProcessEvent received.
 */
struct COREUOBJECT_API FFrame
{
	/** The function being called. */
	UFunction* Node;
	/** The object it is called on. */
	UObject* Object;
	/** Script bytecode; always nullptr in Leon. */
	uint8* Code;
	/** The parameter block. */
	uint8* Locals;
	/** The property the last Step read. */
	FProperty* MostRecentProperty;
	/** The address the last Step read (the parameter itself, for by-reference parameters). */
	uint8* MostRecentPropertyAddress;
	/** The parameter the next Step reads. */
	FField* PropertyChainForCompiledIn;
	/** The caller's frame, or nullptr. */
	FFrame* PreviousFrame;

	FFrame(UObject* InObject, UFunction* InNode, void* InLocals, FFrame* InPreviousFrame = nullptr,
		FField* InPropertyChainForCompiledIn = nullptr);

	/** Reads the next parameter into Result, which must be of its type (UE: StepCompiledIn). */
	template <class TProperty>
	FORCEINLINE void StepCompiledIn(void* const Result)
	{
		checkf(!Code, "Leon has no script VM: exec thunks are only called natively");
		TProperty* Property = (TProperty*)PropertyChainForCompiledIn;
		checkSlow(CastField<TProperty>(PropertyChainForCompiledIn));
		PropertyChainForCompiledIn = Property->Next;
		StepExplicitProperty(Result, Property);
	}

	/**
	 * Reads the next parameter, which is passed by reference: returns it in place when it is an out / reference
	 * parameter, else the copy in TemporaryBuffer (UE: StepCompiledInRef).
	 */
	template <class TProperty, typename TNativeType>
	FORCEINLINE TNativeType& StepCompiledInRef(void* const TemporaryBuffer)
	{
		MostRecentPropertyAddress = nullptr;
		StepCompiledIn<TProperty>(TemporaryBuffer);
		return MostRecentPropertyAddress != nullptr ? *(TNativeType*)MostRecentPropertyAddress
													: *(TNativeType*)TemporaryBuffer;
	}

	/**
	 * Reads Property: an out / reference parameter is only located (MostRecentPropertyAddress), any other one is
	 * copied into Result (UE: StepExplicitProperty; Leon's out parameters always live in Locals, so it has no
	 * FOutParmRec list).
	 */
	void StepExplicitProperty(void* const Result, FProperty* Property);

	/** "<Function> on <Object>", for errors (UE: GetStackTrace, shortened). */
	FString GetStackDescription() const;
};
