#pragma once

// Function flags and the native function signature (UE: UObject/Script.h). Leon has no script VM: every UFunction is
// native, called through its exec thunk.

#include "CoreMinimal.h"

class UObject;
struct FFrame;

/** Flags describing a UFunction (UE: EFunctionFlags, 4.27 values). */
enum EFunctionFlags : uint32
{
	FUNC_None = 0x00000000,
	/** Not virtual in C++. */
	FUNC_Final = 0x00000001,
	FUNC_RequiredAPI = 0x00000002,
	FUNC_BlueprintAuthorityOnly = 0x00000004,
	FUNC_BlueprintCosmetic = 0x00000008,
	FUNC_Net = 0x00000040,
	FUNC_NetReliable = 0x00000080,
	FUNC_NetRequest = 0x00000100,
	/** A console command (P10: CallFunctionByNameWithArguments). */
	FUNC_Exec = 0x00000200,
	/** Implemented in C++ (always set in Leon). */
	FUNC_Native = 0x00000400,
	FUNC_Event = 0x00000800,
	FUNC_NetResponse = 0x00001000,
	FUNC_Static = 0x00002000,
	FUNC_NetMulticast = 0x00004000,
	FUNC_UbergraphFunction = 0x00008000,
	FUNC_MulticastDelegate = 0x00010000,
	FUNC_Public = 0x00020000,
	FUNC_Private = 0x00040000,
	FUNC_Protected = 0x00080000,
	FUNC_Delegate = 0x00100000,
	FUNC_NetServer = 0x00200000,
	/** Has out or by-reference parameters. */
	FUNC_HasOutParms = 0x00400000,
	FUNC_HasDefaults = 0x00800000,
	FUNC_NetClient = 0x01000000,
	FUNC_DLLImport = 0x02000000,
	FUNC_BlueprintCallable = 0x04000000,
	FUNC_BlueprintEvent = 0x08000000,
	FUNC_BlueprintPure = 0x10000000,
	FUNC_EditorOnly = 0x20000000,
	FUNC_Const = 0x40000000,
	FUNC_NetValidate = 0x80000000,
	FUNC_AllFlags = 0xFFFFFFFF,
};
ENUM_CLASS_FLAGS(EFunctionFlags)

/** The return-value parameter of a native thunk (UE: RESULT_DECL / RESULT_PARAM). */
#define RESULT_PARAM Z_Param__Result
#define RESULT_DECL void* const RESULT_PARAM

/** A native function: the exec thunk LeonHeaderTool generates for a UFUNCTION (UE: FNativeFuncPtr). */
typedef void (*FNativeFuncPtr)(UObject* Context, FFrame& TheStack, RESULT_DECL);

/** Declares / defines an exec thunk (UE: DECLARE_FUNCTION / DEFINE_FUNCTION). */
#define DECLARE_FUNCTION(func) static void func(UObject* Context, FFrame& Stack, RESULT_DECL)
#define DEFINE_FUNCTION(func) void func(UObject* Context, FFrame& Stack, RESULT_DECL)
